#include "mcp_server.h"
#include "mcp_protocol.h"
#include "mcp_prompts.h"
#include "error_enrichment.h"
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
#include "resource_tools.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include "tilemap_tools.h"
#include "physics_tools.h"
#include "composite_tools.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <chrono>

using namespace godot;

// Helper: extract arguments object from params, returns empty object if missing
static inline const nlohmann::json& get_args(const nlohmann::json& params) {
    static const nlohmann::json empty_obj = nlohmann::json::object();
    if (params.contains("arguments") && params["arguments"].is_object()) {
        return params["arguments"];
    }
    return empty_obj;
}

// Helper: extract string parameter from JSON, returns empty string if missing/wrong type
static inline std::string get_string(const nlohmann::json& obj, const char* key) {
    if (obj.contains(key) && obj[key].is_string()) {
        return obj[key].get<std::string>();
    }
    return {};
}

// Helper: extract int parameter from JSON, returns default_val if missing/wrong type
static inline int get_int(const nlohmann::json& obj, const char* key, int default_val = 0) {
    if (obj.contains(key) && obj[key].is_number_integer()) {
        return obj[key].get<int>();
    }
    return default_val;
}

// Helper: extract bool parameter from JSON, returns default_val if missing/wrong type
static inline bool get_bool(const nlohmann::json& obj, const char* key, bool default_val = false) {
    if (obj.contains(key) && obj[key].is_boolean()) {
        return obj[key].get<bool>();
    }
    return default_val;
}

// Helper: extract double parameter from JSON, returns default_val if missing/wrong type
static inline double get_double(const nlohmann::json& obj, const char* key, double default_val = 0.0) {
    if (obj.contains(key) && obj[key].is_number()) {
        return obj[key].get<double>();
    }
    return default_val;
}

// Helper: create enriched INVALID_PARAMS error with parameter format examples (ERR-04)
static nlohmann::json make_params_error(const nlohmann::json& id,
                                         const std::string& message,
                                         const std::string& tool_name) {
    std::string enriched = enrich_missing_params(message, tool_name);
    return mcp::create_error_response(id, mcp::INVALID_PARAMS, enriched);
}

// Helper: wrap tool result with error enrichment
// If result contains "error", enriches the message and returns isError:true response.
// Otherwise returns standard isError:false response.
static nlohmann::json make_tool_response(const nlohmann::json& id,
                                          const nlohmann::json& result,
                                          const std::string& tool_name) {
    if (result.contains("error")) {
        std::string error_msg = result["error"].get<std::string>();
        std::string enriched = enrich_error_with_context(error_msg, tool_name);
        return mcp::create_tool_error_result(id, enriched);
    }
    return mcp::create_tool_result(id, result);
}

// Helper: serialize JSON and send over TCP peer
static void send_json(const Ref<StreamPeerTCP>& peer, const nlohmann::json& json_data) {
    std::string json_str = json_data.dump() + "\n";
    PackedByteArray data;
    data.resize(static_cast<int64_t>(json_str.size()));
    memcpy(data.ptrw(), json_str.data(), json_str.size());
    peer->put_data(data);
}

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
    // TIMEOUT-03: Discard stale responses from timed-out requests
    {
        std::lock_guard<std::recursive_mutex> lock(queue_mutex);
        if (io_pending_request_id.is_null()) {
            // No request being waited on -- IO thread already timed out and moved on
            return;
        }
        if (response.contains("id") && response["id"] != io_pending_request_id) {
            // Response ID does not match current pending request -- stale, discard
            return;
        }
    }

    nlohmann::json enriched_response = response;

    // Check if this is a tool result with an error in the content
    if (response.contains("result") && response["result"].contains("content")) {
        auto& content = response["result"]["content"];
        if (content.is_array() && !content.empty() && content[0].contains("text")) {
            std::string text = content[0]["text"].get<std::string>();
            // Try to parse the text as JSON to check for error field
            auto parsed = nlohmann::json::parse(text, nullptr, false);
            if (!parsed.is_discarded() && parsed.contains("error")) {
                std::string error_msg = parsed["error"].get<std::string>();
                std::string enriched = enrich_error_with_context(error_msg, "game_bridge");
                enriched_response = mcp::create_tool_error_result(
                    response["id"], enriched);
            }
        }
    }

    std::lock_guard<std::recursive_mutex> lock(queue_mutex);
    response_queue.push({enriched_response});
    response_cv.notify_one();
}

bool MCPServer::has_client() const {
    return client_connected.load();
}

int MCPServer::start(int p_port) {
    port = p_port;
    tcp_server.instantiate();
    godot::Error err = tcp_server->listen(port);
    if (err != godot::OK) {
        UtilityFunctions::printerr("MCP Meow: Failed to start TCP server on port ", port, " (error: ", (int)err, ")");
        tcp_server.unref();
        return 0;
    }
    running.store(true);
    io_thread = std::thread(&MCPServer::io_thread_func, this);
    UtilityFunctions::print(String::utf8("MCP Meow: TCP 服务监听端口 "), port, String::utf8(" (IO 线程已启动)"));
    return port;
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

    // Clear queues (no lock needed -- IO thread already joined)
    while (!request_queue.empty()) request_queue.pop();
    while (!response_queue.empty()) response_queue.pop();
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
                    send_json(client_peer, resp.response);
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
                    // Request was queued -- wait for main thread response (30s timeout per TIMEOUT-01)
                    std::unique_lock<std::recursive_mutex> lock(queue_mutex);
                    bool got_response = response_cv.wait_for(lock, std::chrono::seconds(30),
                        [this]{ return !response_queue.empty() || !running.load(); });
                    if (!running.load()) break;

                    if (!got_response) {
                        // Timeout: main thread did not respond within 30s
                        auto timeout_error = mcp::create_error_response(
                            io_pending_request_id, -32001,
                            "Tool execution timed out (30s)");
                        send_json(client_peer, timeout_error);
                        io_pending_request_id = nullptr;  // Clear tracked ID
                        continue;
                    }

                    // Send all pending responses
                    while (!response_queue.empty()) {
                        auto resp = response_queue.front();
                        response_queue.pop();
                        send_json(client_peer, resp.response);
                    }
                    io_pending_request_id = nullptr;  // Clear tracked ID after successful send
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
            send_json(client_peer, result.error_response);
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
        io_pending_request_id = result.message.id;  // Track for timeout/stale detection
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

    // Check deferred game bridge timeout (TIMEOUT-02)
    if (game_bridge && game_bridge->has_pending_timeout()) {
        game_bridge->expire_pending();
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
            {"description", "Scene tree with inline scripts (source code, truncated at 100 lines), signal connections (outgoing + incoming), @export properties, transforms, and visibility. Depth 3, 10KB limit."},
            {"mimeType", "application/json"}
        });
        resources.push_back({
            {"uri", "godot://project_files"},
            {"name", "Project Files"},
            {"description", "Project files with size (bytes), type classification (scene/script/resource/image/audio/other), and modification timestamps"},
            {"mimeType", "application/json"}
        });
        return mcp::create_resources_list_response(id, resources);
    }

    if (method == "resources/templates/list") {
        return mcp::create_resource_templates_list_response(id);
    }

    if (method == "resources/read") {
        std::string uri;
        if (params.contains("uri") && params["uri"].is_string()) {
            uri = params["uri"].get<std::string>();
        }
        // Exact-match static resources
        if (uri == "godot://scene_tree") {
            nlohmann::json tree = get_enriched_scene_tree();
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", tree.dump()}});
            return mcp::create_resource_read_response(id, contents);
        }
        if (uri == "godot://project_files") {
            nlohmann::json files = get_enriched_project_files();
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", files.dump()}});
            return mcp::create_resource_read_response(id, contents);
        }
        // URI template matching (godot://node/{path}, godot://script/{path}, godot://signals/{path})
        const std::string node_prefix = "godot://node/";
        if (uri.substr(0, node_prefix.size()) == node_prefix) {
            std::string node_path = uri.substr(node_prefix.size());
            if (node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing node path in URI. Example: godot://node/Player/Sprite2D");
            }
            nlohmann::json detail = enrich_node_detail(node_path);
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", detail.dump()}});
            return mcp::create_resource_read_response(id, contents);
        }
        const std::string script_prefix = "godot://script/";
        if (uri.substr(0, script_prefix.size()) == script_prefix) {
            std::string script_path = uri.substr(script_prefix.size());
            if (script_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing script path in URI. Example: godot://script/res://player.gd");
            }
            nlohmann::json result = read_script(script_path);
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", result.dump()}});
            return mcp::create_resource_read_response(id, contents);
        }
        const std::string signals_prefix = "godot://signals/";
        if (uri.substr(0, signals_prefix.size()) == signals_prefix) {
            std::string sig_node_path = uri.substr(signals_prefix.size());
            if (sig_node_path.empty()) {
                return mcp::create_error_response(id, mcp::INVALID_PARAMS,
                    "Missing node path in URI. Example: godot://signals/Player");
            }
            nlohmann::json result = get_node_signals(sig_node_path);
            nlohmann::json contents = nlohmann::json::array();
            contents.push_back({{"uri", uri}, {"mimeType", "application/json"}, {"text", result.dump()}});
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
            auto& args = get_args(params);
            int max_depth = get_int(args, "max_depth", -1);
            bool include_properties = get_bool(args, "include_properties", true);
            std::string root_path = get_string(args, "root_path");
            nlohmann::json result = get_scene_tree(max_depth, include_properties, root_path);
            return make_tool_response(id, result, tool_name);
        }

        if (tool_name == "create_node") {
            auto& args = get_args(params);
            std::string type = get_string(args, "type");
            std::string parent_path = get_string(args, "parent_path");
            std::string node_name = get_string(args, "name");
            nlohmann::json properties;
            if (args.contains("properties") && args["properties"].is_object())
                properties = args["properties"];
            if (type.empty()) {
                return make_params_error(id, "Missing required parameter: type", tool_name);
            }
            return make_tool_response(id, create_node(type, parent_path, node_name, properties, undo_redo), tool_name);
        }

        if (tool_name == "set_node_property") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            bool has_node_path = args.contains("node_path") && args["node_path"].is_string();
            std::string property = get_string(args, "property");
            std::string value = get_string(args, "value");
            if (!has_node_path || property.empty() || value.empty()) {
                return make_params_error(id, "Missing required parameters: node_path, property, value", tool_name);
            }
            return make_tool_response(id, set_node_property(node_path, property, value, undo_redo), tool_name);
        }

        if (tool_name == "delete_node") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            bool has_node_path = args.contains("node_path") && args["node_path"].is_string();
            if (!has_node_path) {
                return make_params_error(id, "Missing required parameter: node_path", tool_name);
            }
            return make_tool_response(id, delete_node(node_path, undo_redo), tool_name);
        }

        if (tool_name == "read_script") {
            auto& args = get_args(params);
            std::string path = get_string(args, "path");
            if (path.empty()) {
                return make_params_error(id, "Missing required parameter: path", tool_name);
            }
            return make_tool_response(id, read_script(path), tool_name);
        }

        if (tool_name == "write_script") {
            auto& args = get_args(params);
            std::string path = get_string(args, "path");
            std::string content = get_string(args, "content");
            if (path.empty() || content.empty()) {
                return make_params_error(id, "Missing required parameters: path, content", tool_name);
            }
            return make_tool_response(id, write_script(path, content), tool_name);
        }

        if (tool_name == "edit_script") {
            auto& args = get_args(params);
            std::string path = get_string(args, "path");
            std::string operation = get_string(args, "operation");
            int line = get_int(args, "line");
            std::string content = get_string(args, "content");
            int end_line = get_int(args, "end_line", -1);
            if (path.empty() || operation.empty() || line == 0) {
                return make_params_error(id, "Missing required parameters: path, operation, line", tool_name);
            }
            return make_tool_response(id, edit_script(path, operation, line, content, end_line), tool_name);
        }

        if (tool_name == "attach_script") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            bool has_node_path = args.contains("node_path") && args["node_path"].is_string();
            std::string script_path = get_string(args, "script_path");
            if (!has_node_path || script_path.empty()) {
                return make_params_error(id, "Missing required parameters: node_path, script_path", tool_name);
            }
            return make_tool_response(id, attach_script(node_path, script_path, undo_redo), tool_name);
        }

        if (tool_name == "list_project_files") {
            return make_tool_response(id, list_project_files(), tool_name);
        }

        if (tool_name == "run_game") {
            auto& args = get_args(params);
            std::string mode = get_string(args, "mode");
            std::string scene_path = get_string(args, "scene_path");
            bool wait_for_bridge = get_bool(args, "wait_for_bridge");
            int timeout_ms = get_int(args, "timeout", 10000);
            if (mode.empty()) {
                return make_params_error(id, "Missing required parameter: mode", tool_name);
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

            return make_tool_response(id, result, tool_name);
        }

        if (tool_name == "stop_game") {
            return make_tool_response(id, stop_game(), tool_name);
        }

        if (tool_name == "get_game_output") {
            auto& args = get_args(params);
            bool clear_after_read = get_bool(args, "clear_after_read", true);
            std::string level = get_string(args, "level");
            int64_t since = 0;
            if (args.contains("since") && args["since"].is_number_integer())
                since = args["since"].get<int64_t>();
            std::string keyword = get_string(args, "keyword");
            // Use debugger-channel buffer when bridge is active (companion forwards log data)
            if (game_bridge && game_bridge->is_game_connected()) {
                auto result = game_bridge->get_buffered_game_output(clear_after_read, level, since, keyword);
                return make_tool_response(id, result, tool_name);
            }
            // File-based fallback only when bridge is not available
            auto file_result = get_game_output(clear_after_read);
            nlohmann::json structured = nlohmann::json::array();
            if (file_result.contains("lines")) {
                for (auto& line : file_result["lines"]) {
                    std::string text = line.is_string() ? line.get<std::string>() : "";
                    std::string inferred_level = "info";
                    if (text.find("ERROR:") == 0 || text.find("   at:") == 0 ||
                        text.find("   GDScript backtrace") == 0 ||
                        text.find("       [") == 0) {
                        inferred_level = "error";
                    } else if (text.find("WARNING:") == 0) {
                        inferred_level = "warning";
                    }
                    if (!level.empty() && inferred_level != level) continue;
                    if (!keyword.empty() && text.find(keyword) == std::string::npos) continue;
                    structured.push_back({{"text", text}, {"level", inferred_level}, {"timestamp_ms", 0}});
                }
            }
            file_result["lines"] = structured;
            file_result["count"] = structured.size();
            file_result["total_buffered"] = file_result.value("count", 0);
            return make_tool_response(id, file_result, tool_name);
        }

        if (tool_name == "connect_signal") {
            auto& args = get_args(params);
            std::string source_path = get_string(args, "source_path");
            std::string signal_name = get_string(args, "signal_name");
            std::string target_path = get_string(args, "target_path");
            std::string method_name = get_string(args, "method_name");
            if (source_path.empty() || signal_name.empty() || target_path.empty() || method_name.empty()) {
                return make_params_error(id,
                    "Missing required parameters: source_path, signal_name, target_path, method_name", tool_name);
            }
            return make_tool_response(id, connect_signal(source_path, signal_name, target_path, method_name), tool_name);
        }

        if (tool_name == "save_scene") {
            auto& args = get_args(params);
            std::string path = get_string(args, "path");
            return make_tool_response(id, save_scene(path), tool_name);
        }

        if (tool_name == "open_scene") {
            auto& args = get_args(params);
            std::string path = get_string(args, "path");
            if (path.empty()) {
                return make_params_error(id, "Missing required parameter: path", tool_name);
            }
            return make_tool_response(id, open_scene(path), tool_name);
        }

        if (tool_name == "create_scene") {
            auto& args = get_args(params);
            std::string root_type = get_string(args, "root_type");
            std::string path = get_string(args, "path");
            std::string root_name = get_string(args, "root_name");
            if (root_type.empty() || path.empty()) {
                return make_params_error(id, "Missing required parameters: root_type, path", tool_name);
            }
            return make_tool_response(id, create_scene(root_type, path, root_name), tool_name);
        }

        // --- Phase 9: Viewport Screenshot tools ---

        if (tool_name == "capture_viewport") {
            auto& args = get_args(params);
            std::string viewport_type = get_string(args, "viewport_type");
            if (viewport_type.empty()) viewport_type = "2d";
            int width = get_int(args, "width");
            int height = get_int(args, "height");
            auto result = capture_viewport(viewport_type, width, height);
            // If error, return as enriched error with isError:true
            if (result.contains("error")) {
                return make_tool_response(id, result, tool_name);
            }
            // Success: return as MCP ImageContent
            return mcp::create_image_tool_result(id,
                result["data"].get<std::string>(),
                result["mimeType"].get<std::string>(),
                result.value("metadata", nlohmann::json()));
        }

        // --- Phase 10: Game Bridge tools ---

        if (tool_name == "inject_input") {
            auto& args = get_args(params);
            if (!args.contains("type") || !args["type"].is_string()) {
                return make_params_error(id,
                    "Missing required parameter: type", tool_name);
            }
            if (!game_bridge) {
                return make_tool_response(id, {{"error", "Game bridge not initialized"}}, tool_name);
            }
            return make_tool_response(id, game_bridge->inject_input_tool(args), tool_name);
        }

        if (tool_name == "inject_input_sequence") {
            auto& args = get_args(params);
            if (!args.contains("steps") || !args["steps"].is_array()) {
                return make_params_error(id,
                    "Missing required parameter: steps (array of input steps)", tool_name);
            }
            if (!game_bridge) {
                return make_tool_response(id, {{"error", "Game bridge not initialized"}}, tool_name);
            }
            auto result = game_bridge->inject_input_sequence_tool(id, args["steps"]);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return make_tool_response(id, result, tool_name);
        }

        if (tool_name == "inject_text") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            std::string text = get_string(args, "text");
            if (node_path.empty()) {
                return make_params_error(id,
                    "Missing required parameter: node_path", tool_name);
            }
            if (!game_bridge) {
                return make_tool_response(id, {{"error", "Game bridge not initialized"}}, tool_name);
            }
            auto result = game_bridge->inject_text_tool(id, node_path, text);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return make_tool_response(id, result, tool_name);
        }

        if (tool_name == "capture_game_viewport") {
            auto& args = get_args(params);
            int width = get_int(args, "width");
            int height = get_int(args, "height");
            if (!game_bridge) {
                return make_tool_response(id, {{"error", "Game bridge not initialized"}}, tool_name);
            }
            auto result = game_bridge->request_game_viewport_capture(id, width, height);
            // If deferred, do NOT send response now -- it will come via _capture callback
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;  // Special marker
            }
            // Error case (not connected, already pending)
            return make_tool_response(id, result, tool_name);
        }

        // --- Phase 13: Runtime State Query tools ---

        if (tool_name == "get_game_node_property") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            std::string property = get_string(args, "property");
            if (property.empty()) {
                return make_params_error(id,
                    "Missing required parameter: property", tool_name);
            }
            if (!game_bridge) {
                return make_tool_response(id, {{"error", "Game bridge not initialized"}}, tool_name);
            }
            auto result = game_bridge->get_game_node_property_tool(id, node_path, property);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return make_tool_response(id, result, tool_name);
        }

        if (tool_name == "eval_in_game") {
            auto& args = get_args(params);
            std::string expression = get_string(args, "expression");
            if (expression.empty()) {
                return make_params_error(id,
                    "Missing required parameter: expression", tool_name);
            }
            if (!game_bridge) {
                return make_tool_response(id, {{"error", "Game bridge not initialized"}}, tool_name);
            }
            auto result = game_bridge->eval_in_game_tool(id, expression);
            if (result.contains("__deferred") && result["__deferred"].get<bool>()) {
                return result;
            }
            return make_tool_response(id, result, tool_name);
        }

        // --- Phase 20: TileMap Operations ---

        if (tool_name == "set_tilemap_cells") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            bool has_node_path = args.contains("node_path") && args["node_path"].is_string();
            nlohmann::json cells;
            if (args.contains("cells") && args["cells"].is_array())
                cells = args["cells"];
            if (!has_node_path || cells.empty()) {
                return make_params_error(id,
                    "Missing required parameters: node_path, cells", tool_name);
            }
            return make_tool_response(id, set_tilemap_cells(node_path, cells, undo_redo), tool_name);
        }

        if (tool_name == "erase_tilemap_cells") {
            auto& args = get_args(params);
            std::string node_path = get_string(args, "node_path");
            bool has_node_path = args.contains("node_path") && args["node_path"].is_string();
            nlohmann::json coords;
            if (args.contains("coords") && args["coords"].is_array())
                coords = args["coords"];
            if (!has_node_path || coords.empty()) {
                return make_params_error(id,
                    "Missing required parameters: node_path, coords", tool_name);
            }
            return make_tool_response(id, erase_tilemap_cells(node_path, coords, undo_redo), tool_name);
        }

        if (tool_name == "batch_set_property") {
            auto& args = get_args(params);
            nlohmann::json node_paths = args.contains("node_paths") && args["node_paths"].is_array() ? args["node_paths"] : nlohmann::json();
            std::string type_filter = get_string(args, "type_filter");
            std::string property = get_string(args, "property");
            std::string value = get_string(args, "value");
            if (property.empty() || value.empty()) {
                return make_params_error(id,
                    "Missing required parameters: property, value", "batch_set_property");
            }
            if (node_paths.empty() && type_filter.empty()) {
                return make_params_error(id,
                    "Must provide node_paths array or type_filter", "batch_set_property");
            }
            return make_tool_response(id, batch_set_property(node_paths, type_filter, property, value, undo_redo), "batch_set_property");
        }

        if (tool_name == "duplicate_node") {
            auto& args = get_args(params);
            std::string source_path = get_string(args, "source_path");
            std::string target_parent_path = get_string(args, "target_parent_path");
            std::string new_name = get_string(args, "new_name");
            if (source_path.empty()) {
                return make_params_error(id, "Missing required parameter: source_path", tool_name);
            }
            return make_tool_response(id, duplicate_node(source_path, target_parent_path, new_name, undo_redo), tool_name);
        }

        if (tool_name == "validate_scripts") {
            return make_tool_response(id, validate_scripts(), tool_name);
        }

        if (tool_name == "create_node_tree") {
            auto& args = get_args(params);
            if (!args.contains("spec") || !args["spec"].is_object()) {
                return make_params_error(id, "Missing required parameter: spec (object)", tool_name);
            }
            std::string parent_path = get_string(args, "parent_path");
            return make_tool_response(id, create_node_tree(args["spec"], parent_path, undo_redo), tool_name);
        }

        if (tool_name == "restart_editor") {
            auto& args = get_args(params);
            bool save = get_bool(args, "save", true);
            // Send response before restarting
            auto response = mcp::create_tool_result(id, {{"success", true}, {"message", "Editor restarting..."}});
            EditorInterface::get_singleton()->restart_editor(save);
            return response;
        }

        return mcp::create_tool_not_found_error(id, tool_name);
    }

    return mcp::create_error_response(id, mcp::METHOD_NOT_FOUND, "Method not found: " + method);
}
