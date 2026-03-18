#include "mcp_server.h"
#include "mcp_protocol.h"
#include "mcp_prompts.h"
#include "scene_tools.h"
#include "scene_mutation.h"
#include "script_tools.h"
#include "project_tools.h"
#include "runtime_tools.h"
#include "signal_tools.h"
#include "scene_file_tools.h"
#include "ui_tools.h"

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

void MCPServer::set_godot_version(const GodotVersion& v) {
    godot_version = v;
}

bool MCPServer::has_client() const {
    return client_connected.load();
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
    client_connected.store(false);

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
            client_connected.store(true);
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
            client_connected.store(false);
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
        return mcp::create_tools_list_response(id, godot_version);
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

    if (method == "prompts/list") {
        return mcp::create_prompts_list_response(id);
    }

    if (method == "prompts/get") {
        std::string prompt_name;
        nlohmann::json arguments = nlohmann::json::object();
        if (params.contains("name") && params["name"].is_string()) {
            prompt_name = params["name"].get<std::string>();
        }
        if (params.contains("arguments") && params["arguments"].is_object()) {
            arguments = params["arguments"];
        }
        if (prompt_name.empty() || !prompt_exists(prompt_name)) {
            return mcp::create_prompt_not_found_error(id, prompt_name);
        }
        auto messages = get_prompt_messages(prompt_name, arguments);
        // Lookup description from prompts list
        auto all_prompts = get_all_prompts_json();
        std::string description;
        for (const auto& p : all_prompts) {
            if (p["name"] == prompt_name) {
                description = p["description"].get<std::string>();
                break;
            }
        }
        return mcp::create_prompt_get_response(id, description, messages);
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

        if (tool_name == "run_game") {
            std::string mode;
            std::string scene_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("mode") && args["mode"].is_string())
                    mode = args["mode"].get<std::string>();
                if (args.contains("scene_path") && args["scene_path"].is_string())
                    scene_path = args["scene_path"].get<std::string>();
            }
            if (mode.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: mode");
            }
            return mcp::create_tool_result(id, run_game(mode, scene_path));
        }

        if (tool_name == "stop_game") {
            return mcp::create_tool_result(id, stop_game());
        }

        if (tool_name == "get_game_output") {
            bool clear_after_read = true;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("clear_after_read") && args["clear_after_read"].is_boolean())
                    clear_after_read = args["clear_after_read"].get<bool>();
            }
            return mcp::create_tool_result(id, get_game_output(clear_after_read));
        }

        if (tool_name == "get_node_signals") {
            std::string node_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
            }
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: node_path");
            }
            return mcp::create_tool_result(id, get_node_signals(node_path));
        }

        if (tool_name == "connect_signal") {
            std::string source_path, signal_name, target_path, method_name;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("source_path") && args["source_path"].is_string())
                    source_path = args["source_path"].get<std::string>();
                if (args.contains("signal_name") && args["signal_name"].is_string())
                    signal_name = args["signal_name"].get<std::string>();
                if (args.contains("target_path") && args["target_path"].is_string())
                    target_path = args["target_path"].get<std::string>();
                if (args.contains("method_name") && args["method_name"].is_string())
                    method_name = args["method_name"].get<std::string>();
            }
            if (source_path.empty() || signal_name.empty() || target_path.empty() || method_name.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: source_path, signal_name, target_path, method_name");
            }
            return mcp::create_tool_result(id, connect_signal(source_path, signal_name, target_path, method_name));
        }

        if (tool_name == "disconnect_signal") {
            std::string source_path, signal_name, target_path, method_name;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("source_path") && args["source_path"].is_string())
                    source_path = args["source_path"].get<std::string>();
                if (args.contains("signal_name") && args["signal_name"].is_string())
                    signal_name = args["signal_name"].get<std::string>();
                if (args.contains("target_path") && args["target_path"].is_string())
                    target_path = args["target_path"].get<std::string>();
                if (args.contains("method_name") && args["method_name"].is_string())
                    method_name = args["method_name"].get<std::string>();
            }
            if (source_path.empty() || signal_name.empty() || target_path.empty() || method_name.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: source_path, signal_name, target_path, method_name");
            }
            return mcp::create_tool_result(id, disconnect_signal(source_path, signal_name, target_path, method_name));
        }

        if (tool_name == "save_scene") {
            std::string path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
            }
            return mcp::create_tool_result(id, save_scene(path));
        }

        if (tool_name == "open_scene") {
            std::string path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
            }
            if (path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: path");
            }
            return mcp::create_tool_result(id, open_scene(path));
        }

        if (tool_name == "list_open_scenes") {
            return mcp::create_tool_result(id, list_open_scenes());
        }

        if (tool_name == "create_scene") {
            std::string root_type, path, root_name;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("root_type") && args["root_type"].is_string())
                    root_type = args["root_type"].get<std::string>();
                if (args.contains("path") && args["path"].is_string())
                    path = args["path"].get<std::string>();
                if (args.contains("root_name") && args["root_name"].is_string())
                    root_name = args["root_name"].get<std::string>();
            }
            if (root_type.empty() || path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameters: root_type, path");
            }
            return mcp::create_tool_result(id, create_scene(root_type, path, root_name));
        }

        if (tool_name == "instantiate_scene") {
            std::string scene_path, parent_path, name;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("scene_path") && args["scene_path"].is_string())
                    scene_path = args["scene_path"].get<std::string>();
                if (args.contains("parent_path") && args["parent_path"].is_string())
                    parent_path = args["parent_path"].get<std::string>();
                if (args.contains("name") && args["name"].is_string())
                    name = args["name"].get<std::string>();
            }
            if (scene_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: scene_path");
            }
            return mcp::create_tool_result(id, instantiate_scene(scene_path, parent_path, name, undo_redo));
        }

        // --- Phase 7: UI System tools ---

        if (tool_name == "set_layout_preset") {
            std::string node_path, preset;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                if (args.contains("preset") && args["preset"].is_string())
                    preset = args["preset"].get<std::string>();
            }
            if (node_path.empty() || preset.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: node_path, preset");
            }
            return mcp::create_tool_result(id, set_layout_preset(node_path, preset, undo_redo));
        }

        if (tool_name == "set_theme_override") {
            std::string node_path;
            nlohmann::json overrides;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                if (args.contains("overrides") && args["overrides"].is_object())
                    overrides = args["overrides"];
            }
            if (node_path.empty() || overrides.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: node_path, overrides");
            }
            return mcp::create_tool_result(id, set_theme_override(node_path, overrides, undo_redo));
        }

        if (tool_name == "create_stylebox") {
            std::string node_path, override_name;
            nlohmann::json properties;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                if (args.contains("override_name") && args["override_name"].is_string())
                    override_name = args["override_name"].get<std::string>();
                // Collect all arguments as properties (function will read what it needs)
                properties = args;
            }
            if (node_path.empty() || override_name.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: node_path, override_name");
            }
            return mcp::create_tool_result(id, create_stylebox(node_path, override_name, properties, undo_redo));
        }

        if (tool_name == "get_ui_properties") {
            std::string node_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
            }
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: node_path");
            }
            return mcp::create_tool_result(id, get_ui_properties(node_path));
        }

        if (tool_name == "set_container_layout") {
            std::string node_path;
            nlohmann::json layout_params;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                layout_params = args;
            }
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: node_path");
            }
            return mcp::create_tool_result(id, set_container_layout(node_path, layout_params, undo_redo));
        }

        if (tool_name == "get_theme_overrides") {
            std::string node_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
            }
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: node_path");
            }
            return mcp::create_tool_result(id, get_theme_overrides(node_path));
        }

        return mcp::create_tool_not_found_error(id, tool_name);
    }

    return mcp::create_error_response(id, mcp::METHOD_NOT_FOUND, "Method not found: " + method);
}
