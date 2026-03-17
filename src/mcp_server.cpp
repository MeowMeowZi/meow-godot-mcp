#include "mcp_server.h"
#include "mcp_protocol.h"
#include "scene_tools.h"
#include "scene_mutation.h"
#include "script_tools.h"
#include "project_tools.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <chrono>

using namespace godot;

MCPServer::MCPServer()
    : initialized(false), port(6800) {
}

MCPServer::~MCPServer() {
    stop();
}

void MCPServer::set_undo_redo(godot::EditorUndoRedoManager* ur) {
    undo_redo = ur;
}

void MCPServer::start(int p_port) {
    port = p_port;
    tcp_server.instantiate();
    godot::Error err = tcp_server->listen(port);
    if (err != godot::OK) {
        UtilityFunctions::printerr("MCP Meow: Failed to start TCP server on port ", port, " (error: ", (int)err, ")");
        tcp_server.unref();
        return;
    }
    running.store(true);
    io_thread = std::thread(&MCPServer::io_thread_func, this);
    UtilityFunctions::print("MCP Meow: TCP server listening on port ", port, " (IO thread started)");
}

void MCPServer::stop() {
    running.store(false);

    // Wake up IO thread if waiting on response_cv
    response_cv.notify_all();

    if (io_thread.joinable()) {
        io_thread.join();
    }

    // Clean up TCP state on main thread after IO thread has stopped
    if (client_peer.is_valid()) {
        client_peer->disconnect_from_host();
        client_peer.unref();
    }
    if (tcp_server.is_valid()) {
        tcp_server->stop();
        tcp_server.unref();
    }
    initialized = false;
    read_buffer.clear();

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!request_queue.empty()) request_queue.pop();
        while (!response_queue.empty()) response_queue.pop();
    }
}

bool MCPServer::is_running() const {
    return running.load() && tcp_server.is_valid();
}

void MCPServer::io_thread_func() {
    while (running.load()) {
        // Accept new connections
        if (tcp_server.is_valid() && tcp_server->is_connection_available()) {
            if (client_peer.is_valid()) {
                client_peer->disconnect_from_host();
                client_peer.unref();
                read_buffer.clear();
            }
            client_peer = tcp_server->take_connection();
            initialized = false;
            UtilityFunctions::print("MCP Meow: Client connected");
        }

        if (!client_peer.is_valid()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        client_peer->poll();
        auto status = client_peer->get_status();
        if (status == StreamPeerTCP::STATUS_NONE || status == StreamPeerTCP::STATUS_ERROR) {
            UtilityFunctions::print("MCP Meow: Client disconnected");
            client_peer.unref();
            read_buffer.clear();
            initialized = false;
            continue;
        }
        if (status != StreamPeerTCP::STATUS_CONNECTED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int available = client_peer->get_available_bytes();
        if (available <= 0) {
            // Check for pending responses to send
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                while (!response_queue.empty()) {
                    auto resp = response_queue.front();
                    response_queue.pop();
                    std::string json_str = resp.response.dump() + "\n";
                    PackedByteArray data;
                    data.resize(static_cast<int64_t>(json_str.size()));
                    memcpy(data.ptrw(), json_str.data(), json_str.size());
                    client_peer->put_data(data);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Read available data
        Array get_result = client_peer->get_partial_data(available);
        if (get_result.size() < 2) continue;
        int64_t err_code = get_result[0];
        if (err_code != 0) continue;
        PackedByteArray data = get_result[1];
        if (data.size() == 0) continue;

        read_buffer.append(reinterpret_cast<const char*>(data.ptr()), data.size());

        // Process complete lines
        size_t pos;
        while ((pos = read_buffer.find('\n')) != std::string::npos) {
            std::string line = read_buffer.substr(0, pos);
            read_buffer.erase(0, pos + 1);
            if (line.empty() || (line.size() == 1 && line[0] == '\r')) continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) {
                try {
                    bool handled = process_message_io(line);
                    if (!handled) {
                        // Request was queued -- wait for main thread response
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        response_cv.wait(lock, [this]{ return !response_queue.empty() || !running.load(); });
                        if (!running.load()) break;
                        // Send all pending responses
                        while (!response_queue.empty()) {
                            auto resp = response_queue.front();
                            response_queue.pop();
                            std::string json_str = resp.response.dump() + "\n";
                            PackedByteArray resp_data;
                            resp_data.resize(static_cast<int64_t>(json_str.size()));
                            memcpy(resp_data.ptrw(), json_str.data(), json_str.size());
                            client_peer->put_data(resp_data);
                        }
                    }
                } catch (const std::exception& e) {
                    UtilityFunctions::printerr("MCP Meow: Error processing message: ", e.what());
                }
            }
        }
    }
}

bool MCPServer::process_message_io(const std::string& line) {
    auto result = mcp::parse_jsonrpc(line);

    if (!result.success) {
        // Send error response directly (no Godot API calls needed)
        if (client_peer.is_valid()) {
            std::string json_str = result.error_response.dump() + "\n";
            PackedByteArray data;
            data.resize(static_cast<int64_t>(json_str.size()));
            memcpy(data.ptrw(), json_str.data(), json_str.size());
            client_peer->put_data(data);
        }
        return true;
    }

    // Handle notifications inline (no response expected)
    if (result.message.is_notification) {
        if (result.message.method == "notifications/initialized") {
            initialized = true;
            UtilityFunctions::print("MCP Meow: Client initialized");
        }
        return true;
    }

    // Queue request for main thread processing
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        request_queue.push({result.message.method, result.message.id, result.message.params});
    }
    return false;
}

void MCPServer::poll() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!request_queue.empty()) {
        auto req = request_queue.front();
        request_queue.pop();
        // Execute on main thread (all Godot API calls are safe here)
        auto response = handle_request(req.method, req.id, req.params);
        response_queue.push({response});
        response_cv.notify_one();
    }
}

nlohmann::json MCPServer::handle_request(const std::string& method, const nlohmann::json& id, const nlohmann::json& params) {
    if (method == "initialize") {
        return mcp::create_initialize_response(id);
    }

    if (method == "tools/list") {
        return mcp::create_tools_list_response(id);
    }

    if (method == "resources/list") {
        nlohmann::json resources = nlohmann::json::array();
        resources.push_back({
            {"uri", "godot://scene_tree"},
            {"name", "Scene Tree"},
            {"description", "Current scene tree structure with node names, types, and paths"},
            {"mimeType", "application/json"}
        });
        resources.push_back({
            {"uri", "godot://project_files"},
            {"name", "Project Files"},
            {"description", "Flat listing of all files in the project (res://)"},
            {"mimeType", "application/json"}
        });
        return mcp::create_resources_list_response(id, resources);
    }

    if (method == "resources/read") {
        std::string uri;
        if (params.contains("uri") && params["uri"].is_string()) {
            uri = params["uri"].get<std::string>();
        }
        if (uri == "godot://scene_tree") {
            nlohmann::json tree = get_scene_tree();
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", tree.dump()}});
            return mcp::create_resource_read_response(id, contents);
        }
        if (uri == "godot://project_files") {
            nlohmann::json files = list_project_files();
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", files.dump()}});
            return mcp::create_resource_read_response(id, contents);
        }
        return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Unknown resource URI: " + uri);
    }

    if (method == "tools/call") {
        std::string tool_name;
        if (params.contains("name") && params["name"].is_string()) {
            tool_name = params["name"].get<std::string>();
        } else {
            return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing or invalid 'name' parameter");
        }

        if (tool_name == "get_scene_tree") {
            // Extract optional arguments
            int max_depth = -1;  // unlimited
            bool include_properties = true;
            std::string root_path;

            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("max_depth") && args["max_depth"].is_number_integer()) {
                    max_depth = args["max_depth"].get<int>();
                }
                if (args.contains("include_properties") && args["include_properties"].is_boolean()) {
                    include_properties = args["include_properties"].get<bool>();
                }
                if (args.contains("root_path") && args["root_path"].is_string()) {
                    root_path = args["root_path"].get<std::string>();
                }
            }

            nlohmann::json result = get_scene_tree(max_depth, include_properties, root_path);
            return mcp::create_tool_result(id, result);
        }

        if (tool_name == "create_node") {
            std::string type, parent_path, node_name;
            nlohmann::json properties;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("type") && args["type"].is_string())
                    type = args["type"].get<std::string>();
                if (args.contains("parent_path") && args["parent_path"].is_string())
                    parent_path = args["parent_path"].get<std::string>();
                if (args.contains("name") && args["name"].is_string())
                    node_name = args["name"].get<std::string>();
                if (args.contains("properties") && args["properties"].is_object())
                    properties = args["properties"];
            }
            if (type.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: type");
            }
            return mcp::create_tool_result(id, create_node(type, parent_path, node_name, properties, undo_redo));
        }

        if (tool_name == "set_node_property") {
            std::string node_path, property, value;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                if (args.contains("property") && args["property"].is_string())
                    property = args["property"].get<std::string>();
                if (args.contains("value") && args["value"].is_string())
                    value = args["value"].get<std::string>();
            }
            if (node_path.empty() || property.empty() || value.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameters: node_path, property, value");
            }
            return mcp::create_tool_result(id, set_node_property(node_path, property, value, undo_redo));
        }

        if (tool_name == "delete_node") {
            std::string node_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
            }
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: node_path");
            }
            return mcp::create_tool_result(id, delete_node(node_path, undo_redo));
        }

        if (tool_name == "read_script") {
            std::string path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
            }
            if (path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: path");
            }
            return mcp::create_tool_result(id, read_script(path));
        }

        if (tool_name == "write_script") {
            std::string path, content;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
                if (args.contains("content") && args["content"].is_string())
                    content = args["content"].get<std::string>();
            }
            if (path.empty() || content.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameters: path, content");
            }
            return mcp::create_tool_result(id, write_script(path, content));
        }

        if (tool_name == "edit_script") {
            std::string path, operation, content;
            int line = 0, end_line = -1;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
                if (args.contains("operation") && args["operation"].is_string())
                    operation = args["operation"].get<std::string>();
                if (args.contains("line") && args["line"].is_number_integer())
                    line = args["line"].get<int>();
                if (args.contains("content") && args["content"].is_string())
                    content = args["content"].get<std::string>();
                if (args.contains("end_line") && args["end_line"].is_number_integer())
                    end_line = args["end_line"].get<int>();
            }
            if (path.empty() || operation.empty() || line == 0) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameters: path, operation, line");
            }
            return mcp::create_tool_result(id, edit_script(path, operation, line, content, end_line));
        }

        if (tool_name == "attach_script") {
            std::string node_path, script_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                if (args.contains("script_path") && args["script_path"].is_string())
                    script_path = args["script_path"].get<std::string>();
            }
            if (node_path.empty() || script_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameters: node_path, script_path");
            }
            return mcp::create_tool_result(id, attach_script(node_path, script_path, undo_redo));
        }

        if (tool_name == "detach_script") {
            std::string node_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
            }
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: node_path");
            }
            return mcp::create_tool_result(id, detach_script(node_path, undo_redo));
        }

        if (tool_name == "list_project_files") {
            return mcp::create_tool_result(id, list_project_files());
        }

        if (tool_name == "get_project_settings") {
            return mcp::create_tool_result(id, get_project_settings());
        }

        if (tool_name == "get_resource_info") {
            std::string path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
            }
            if (path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: path");
            }
            return mcp::create_tool_result(id, get_resource_info(path));
        }

        return mcp::create_tool_not_found_error(id, tool_name);
    }

    return mcp::create_error_response(id, mcp::METHOD_NOT_FOUND, "Method not found: " + method);
}
