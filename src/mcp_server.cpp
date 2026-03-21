#include "mcp_server.h"
#include "mcp_protocol.h"
#include "mcp_prompts.h"
#include "game_bridge.h"
#include "scene_tools.h"
#include "scene_mutation.h"
#include "script_tools.h"
#include "project_tools.h"
#include "runtime_tools.h"
#include "signal_tools.h"
#include "scene_file_tools.h"
#include "ui_tools.h"
#include "animation_tools.h"
#include "viewport_tools.h"

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

void MCPServer::set_game_bridge(MeowDebuggerPlugin* bridge) {
    game_bridge = bridge;
    if (bridge) {
        bridge->set_deferred_response_callback([this](const nlohmann::json& response) {
            queue_deferred_response(response);
        });
    }
}

void MCPServer::queue_deferred_response(const nlohmann::json& response) {
    std::lock_guard<std::recursive_mutex> lock(queue_mutex);
    response_queue.push({response});
    response_cv.notify_one();
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
    UtilityFunctions::print(String::utf8("MCP Meow: TCP 服务监听端口 "), port, String::utf8(" (IO 线程已启动)"));
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

    // Clear game bridge reference
    game_bridge = nullptr;

    // Clear queues
    {
        std::lock_guard<std::recursive_mutex> lock(queue_mutex);
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
                // Already have a client -- silently reject new connection
                auto rejected = tcp_server->take_connection();
                rejected->disconnect_from_host();
            } else {
                client_peer = tcp_server->take_connection();
                client_connected.store(true);
                initialized = false;
                UtilityFunctions::print(String::utf8("MCP Meow: 客户端已连接"));
            }
        }

        if (!client_peer.is_valid()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        client_peer->poll();
        auto status = client_peer->get_status();
        if (status == StreamPeerTCP::STATUS_NONE || status == StreamPeerTCP::STATUS_ERROR) {
            UtilityFunctions::print(String::utf8("MCP Meow: 客户端已断开"));
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
                std::lock_guard<std::recursive_mutex> lock(queue_mutex);
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
                bool handled = process_message_io(line);
                if (!handled) {
                    // Request was queued -- wait for main thread response
                    std::unique_lock<std::recursive_mutex> lock(queue_mutex);
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
            UtilityFunctions::print(String::utf8("MCP Meow: 客户端已初始化"));
        }
        return true;
    }

    // Queue request for main thread processing
    {
        std::lock_guard<std::recursive_mutex> lock(queue_mutex);
        request_queue.push({result.message.method, result.message.id, result.message.params});
    }
    return false;
}

void MCPServer::poll() {
    std::lock_guard<std::recursive_mutex> lock(queue_mutex);

    // Check bridge-wait state (run_game wait_for_bridge)
    if (waiting_for_bridge) {
        if (game_bridge && game_bridge->is_game_connected()) {
            // Bridge connected - send success response
            bridge_wait_result["bridge_connected"] = true;
            auto response = mcp::create_tool_result(bridge_wait_id, bridge_wait_result);
            response_queue.push({response});
            response_cv.notify_one();
            waiting_for_bridge = false;
        } else if (std::chrono::steady_clock::now() >= bridge_wait_deadline) {
            // Timeout - send result with bridge_connected=false
            bridge_wait_result["bridge_connected"] = false;
            bridge_wait_result["timeout"] = true;
            auto response = mcp::create_tool_result(bridge_wait_id, bridge_wait_result);
            response_queue.push({response});
            response_cv.notify_one();
            waiting_for_bridge = false;
        }
        // else: keep waiting (poll() called again next frame)
    }

    while (!request_queue.empty()) {
        auto req = request_queue.front();
        request_queue.pop();
        // Execute on main thread (all Godot API calls are safe here)
        auto response = handle_request(req.method, req.id, req.params);
        // Check for deferred response marker -- do not queue immediately
        if (response.contains("__deferred") && response["__deferred"].get<bool>()) {
            // Response will be queued later via queue_deferred_response
            continue;
        }
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
            std::string category;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("category") && args["category"].is_string())
                    category = args["category"].get<std::string>();
            }
            return mcp::create_tool_result(id, get_project_settings(category));
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
            bool wait_for_bridge = false;
            int timeout_ms = 10000;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("mode") && args["mode"].is_string())
                    mode = args["mode"].get<std::string>();
                if (args.contains("scene_path") && args["scene_path"].is_string())
                    scene_path = args["scene_path"].get<std::string>();
                if (args.contains("wait_for_bridge") && args["wait_for_bridge"].is_boolean())
                    wait_for_bridge = args["wait_for_bridge"].get<bool>();
                if (args.contains("timeout") && args["timeout"].is_number_integer())
                    timeout_ms = args["timeout"].get<int>();
            }
            if (mode.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS, "Missing required parameter: mode");
            }
            auto result = run_game(mode, scene_path);

            // If wait_for_bridge requested and game launched/running successfully
            if (wait_for_bridge && result.contains("success") && result["success"].get<bool>()) {
                // If already running, check bridge immediately
                bool already_running = result.value("already_running", false);
                if (already_running && game_bridge && game_bridge->is_game_connected()) {
                    result["bridge_connected"] = true;
                    return mcp::create_tool_result(id, result);
                }
                // Defer response -- poll() will check bridge connection each frame
                bridge_wait_id = id;
                bridge_wait_result = result;
                bridge_wait_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
                waiting_for_bridge = true;
                return {{"__deferred", true}};
            }

            return mcp::create_tool_result(id, result);
        }

        if (tool_name == "stop_game") {
            return mcp::create_tool_result(id, stop_game());
        }

        if (tool_name == "get_game_output") {
            bool clear_after_read = true;
            std::string level;
            int64_t since = 0;
            std::string keyword;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("clear_after_read") && args["clear_after_read"].is_boolean())
                    clear_after_read = args["clear_after_read"].get<bool>();
                if (args.contains("level") && args["level"].is_string())
                    level = args["level"].get<std::string>();
                if (args.contains("since") && args["since"].is_number_integer())
                    since = args["since"].get<int64_t>();
                if (args.contains("keyword") && args["keyword"].is_string())
                    keyword = args["keyword"].get<std::string>();
            }
            // Try debugger-channel buffer first, fall back to file-based reading
            if (game_bridge) {
                auto result = game_bridge->get_buffered_game_output(clear_after_read, level, since, keyword);
                // If debugger buffer had data, use it
                if (result.contains("count") && result["count"].get<int>() > 0) {
                    return mcp::create_tool_result(id, result);
                }
            }
            // File-based reading (auto-enabled by run_game)
            auto file_result = get_game_output(clear_after_read);
            // Convert plain string lines to structured format with level inference
            nlohmann::json structured = nlohmann::json::array();
            if (file_result.contains("lines")) {
                for (auto& line : file_result["lines"]) {
                    std::string text = line.is_string() ? line.get<std::string>() : "";
                    // Infer level from text prefixes (Godot log format)
                    std::string inferred_level = "info";
                    if (text.find("ERROR:") == 0 || text.find("   at:") == 0 ||
                        text.find("   GDScript backtrace") == 0 ||
                        text.find("       [") == 0) {
                        inferred_level = "error";
                    } else if (text.find("WARNING:") == 0) {
                        inferred_level = "warning";
                    }
                    // Apply filters
                    if (!level.empty() && inferred_level != level) continue;
                    if (!keyword.empty() && text.find(keyword) == std::string::npos) continue;
                    structured.push_back({{"text", text}, {"level", inferred_level}, {"timestamp_ms", 0}});
                }
            }
            file_result["lines"] = structured;
            file_result["count"] = structured.size();
            file_result["total_buffered"] = file_result.value("count", 0);
            return mcp::create_tool_result(id, file_result);
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

        // --- Phase 8: Animation System tools ---

        if (tool_name == "create_animation") {
            std::string animation_name, player_path, parent_path, node_name;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("animation_name") && args["animation_name"].is_string())
                    animation_name = args["animation_name"].get<std::string>();
                if (args.contains("player_path") && args["player_path"].is_string())
                    player_path = args["player_path"].get<std::string>();
                if (args.contains("parent_path") && args["parent_path"].is_string())
                    parent_path = args["parent_path"].get<std::string>();
                if (args.contains("node_name") && args["node_name"].is_string())
                    node_name = args["node_name"].get<std::string>();
            }
            if (animation_name.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: animation_name");
            }
            return mcp::create_tool_result(id, create_animation(animation_name, player_path, parent_path, node_name, undo_redo));
        }

        if (tool_name == "add_animation_track") {
            std::string player_path, animation_name, track_type, track_path;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("player_path") && args["player_path"].is_string())
                    player_path = args["player_path"].get<std::string>();
                if (args.contains("animation_name") && args["animation_name"].is_string())
                    animation_name = args["animation_name"].get<std::string>();
                if (args.contains("track_type") && args["track_type"].is_string())
                    track_type = args["track_type"].get<std::string>();
                if (args.contains("track_path") && args["track_path"].is_string())
                    track_path = args["track_path"].get<std::string>();
            }
            if (player_path.empty() || animation_name.empty() || track_type.empty() || track_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: player_path, animation_name, track_type, track_path");
            }
            return mcp::create_tool_result(id, add_animation_track(player_path, animation_name, track_type, track_path));
        }

        if (tool_name == "set_keyframe") {
            std::string player_path, animation_name, action_str, value_str;
            int track_index = -1;
            double time = 0.0;
            double transition = 1.0;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("player_path") && args["player_path"].is_string())
                    player_path = args["player_path"].get<std::string>();
                if (args.contains("animation_name") && args["animation_name"].is_string())
                    animation_name = args["animation_name"].get<std::string>();
                if (args.contains("track_index") && args["track_index"].is_number_integer())
                    track_index = args["track_index"].get<int>();
                if (args.contains("time") && args["time"].is_number())
                    time = args["time"].get<double>();
                if (args.contains("action") && args["action"].is_string())
                    action_str = args["action"].get<std::string>();
                if (args.contains("value") && args["value"].is_string())
                    value_str = args["value"].get<std::string>();
                if (args.contains("transition") && args["transition"].is_number())
                    transition = args["transition"].get<double>();
            }
            if (player_path.empty() || animation_name.empty() || track_index < 0 || action_str.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: player_path, animation_name, track_index, time, action");
            }
            return mcp::create_tool_result(id, set_keyframe(player_path, animation_name, track_index, time, action_str, value_str, transition));
        }

        if (tool_name == "get_animation_info") {
            std::string player_path, animation_name;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("player_path") && args["player_path"].is_string())
                    player_path = args["player_path"].get<std::string>();
                if (args.contains("animation_name") && args["animation_name"].is_string())
                    animation_name = args["animation_name"].get<std::string>();
            }
            if (player_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: player_path");
            }
            return mcp::create_tool_result(id, get_animation_info(player_path, animation_name));
        }

        if (tool_name == "set_animation_properties") {
            std::string player_path, animation_name;
            nlohmann::json props;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("player_path") && args["player_path"].is_string())
                    player_path = args["player_path"].get<std::string>();
                if (args.contains("animation_name") && args["animation_name"].is_string())
                    animation_name = args["animation_name"].get<std::string>();
                props = args;
            }
            if (player_path.empty() || animation_name.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameters: player_path, animation_name");
            }
            return mcp::create_tool_result(id, set_animation_properties(player_path, animation_name, props, undo_redo));
        }

        // --- Phase 9: Viewport Screenshot tools ---

        if (tool_name == "capture_viewport") {
            std::string viewport_type = "2d";
            int width = 0, height = 0;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("viewport_type") && args["viewport_type"].is_string())
                    viewport_type = args["viewport_type"].get<std::string>();
                if (args.contains("width") && args["width"].is_number_integer())
                    width = args["width"].get<int>();
                if (args.contains("height") && args["height"].is_number_integer())
                    height = args["height"].get<int>();
            }
            auto result = capture_viewport(viewport_type, width, height);
            // If error, return as regular TextContent
            if (result.contains("error")) {
                return mcp::create_tool_result(id, result);
            }
            // Success: return as MCP ImageContent
            return mcp::create_image_tool_result(id,
                result["data"].get<std::string>(),
                result["mimeType"].get<std::string>(),
                result.value("metadata", nlohmann::json()));
        }

        // --- Phase 10: Game Bridge tools ---

        if (tool_name == "inject_input") {
            nlohmann::json args;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                args = params["arguments"];
            }
            if (!args.contains("type") || !args["type"].is_string()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: type");
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            return mcp::create_tool_result(id, game_bridge->inject_input_tool(args));
        }

        if (tool_name == "capture_game_viewport") {
            int width = 0, height = 0;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("width") && args["width"].is_number_integer())
                    width = args["width"].get<int>();
                if (args.contains("height") && args["height"].is_number_integer())
                    height = args["height"].get<int>();
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->request_game_viewport_capture(id, width, height);
            // If deferred, do NOT send response now -- it will come via _capture callback
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;  // Special marker
            }
            // Error case (not connected, already pending)
            return mcp::create_tool_result(id, result);
        }

        if (tool_name == "get_game_bridge_status") {
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"connected", false}, {"error", "Game bridge not initialized"}});
            }
            return mcp::create_tool_result(id, game_bridge->get_bridge_status_tool());
        }

        // --- Phase 12: Input Injection Enhancement tools ---

        if (tool_name == "click_node") {
            std::string node_path;
            bool has_node_path = false;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string()) {
                    node_path = args["node_path"].get<std::string>();
                    has_node_path = true;
                }
            }
            if (!has_node_path) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: node_path");
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->click_node_tool(id, node_path);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return mcp::create_tool_result(id, result);
        }

        if (tool_name == "get_node_rect") {
            std::string node_path;
            bool has_node_path = false;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string()) {
                    node_path = args["node_path"].get<std::string>();
                    has_node_path = true;
                }
            }
            if (!has_node_path) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: node_path");
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->get_node_rect_tool(id, node_path);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return mcp::create_tool_result(id, result);
        }

        // --- Phase 13: Runtime State Query tools ---

        if (tool_name == "get_game_node_property") {
            std::string node_path, property;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("node_path") && args["node_path"].is_string())
                    node_path = args["node_path"].get<std::string>();
                if (args.contains("property") && args["property"].is_string())
                    property = args["property"].get<std::string>();
            }
            if (property.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: property");
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->get_game_node_property_tool(id, node_path, property);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return mcp::create_tool_result(id, result);
        }

        if (tool_name == "eval_in_game") {
            std::string expression;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("expression") && args["expression"].is_string())
                    expression = args["expression"].get<std::string>();
            }
            if (expression.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: expression");
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->eval_in_game_tool(id, expression);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return mcp::create_tool_result(id, result);
        }

        if (tool_name == "get_game_scene_tree") {
            int max_depth = -1;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("max_depth") && args["max_depth"].is_number_integer())
                    max_depth = args["max_depth"].get<int>();
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->get_game_scene_tree_tool(id, max_depth);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return mcp::create_tool_result(id, result);
        }

        // --- Phase 15: Integration Testing Toolkit ---

        if (tool_name == "run_test_sequence") {
            nlohmann::json steps;
            if (params.contains("arguments") && params["arguments"].is_object()) {
                auto& args = params["arguments"];
                if (args.contains("steps") && args["steps"].is_array())
                    steps = args["steps"];
            }
            if (steps.empty() || !steps.is_array()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing required parameter: steps (must be a non-empty array)");
            }
            if (!game_bridge) {
                return mcp::create_tool_result(id, {{"error", "Game bridge not initialized"}});
            }
            auto result = game_bridge->run_test_sequence_tool(id, steps);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return mcp::create_tool_result(id, result);
        }

        return mcp::create_tool_not_found_error(id, tool_name);
    }

    return mcp::create_error_response(id, mcp::METHOD_NOT_FOUND, "Method not found: " + method);
}
