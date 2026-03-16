#include "mcp_server.h"
#include "mcp_protocol.h"
#include "scene_tools.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

using namespace godot;

MCPServer::MCPServer()
    : initialized(false), port(6800), running(false) {
}

MCPServer::~MCPServer() {
    stop();
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
    running = true;
    UtilityFunctions::print("MCP Meow: TCP server listening on port ", port);
}

void MCPServer::stop() {
    if (client_peer.is_valid()) {
        client_peer->disconnect_from_host();
        client_peer.unref();
    }
    if (tcp_server.is_valid()) {
        tcp_server->stop();
        tcp_server.unref();
    }
    running = false;
    initialized = false;
    read_buffer.clear();
}

bool MCPServer::is_running() const {
    return running && tcp_server.is_valid();
}

void MCPServer::poll() {
    if (!tcp_server.is_valid()) {
        return;
    }

    // Accept new connections (one client at a time)
    if (tcp_server->is_connection_available()) {
        // Disconnect previous client if any
        if (client_peer.is_valid()) {
            client_peer->disconnect_from_host();
            client_peer.unref();
            read_buffer.clear();
        }
        client_peer = tcp_server->take_connection();
        initialized = false;
        UtilityFunctions::print("MCP Meow: Client connected");
    }

    // Read data from connected client
    if (!client_peer.is_valid()) {
        return;
    }

    // Poll the peer to update its connection state
    client_peer->poll();

    StreamPeerTCP::Status status = client_peer->get_status();
    if (status == StreamPeerTCP::STATUS_NONE || status == StreamPeerTCP::STATUS_ERROR) {
        UtilityFunctions::print("MCP Meow: Client disconnected");
        client_peer.unref();
        read_buffer.clear();
        initialized = false;
        return;
    }

    if (status != StreamPeerTCP::STATUS_CONNECTED) {
        return;  // Still connecting
    }

    int available = client_peer->get_available_bytes();
    if (available <= 0) {
        return;
    }

    // Read available data
    Array get_result = client_peer->get_partial_data(available);
    if (get_result.size() < 2) {
        return;
    }
    int64_t err_code = get_result[0];
    if (err_code != 0) {
        return;
    }
    PackedByteArray data = get_result[1];
    if (data.size() == 0) {
        return;
    }

    // Append to read buffer
    read_buffer.append(reinterpret_cast<const char*>(data.ptr()), data.size());

    // Process complete newline-delimited messages
    size_t pos;
    while ((pos = read_buffer.find('\n')) != std::string::npos) {
        std::string line = read_buffer.substr(0, pos);
        read_buffer.erase(0, pos + 1);

        // Skip empty lines
        if (line.empty() || (line.size() == 1 && line[0] == '\r')) {
            continue;
        }

        // Remove trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            try {
                process_message(line);
            } catch (const std::exception& e) {
                UtilityFunctions::printerr("MCP Meow: Error processing message: ", e.what());
            }
        }
    }
}

void MCPServer::process_message(const std::string& line) {
    auto result = mcp::parse_jsonrpc(line);

    if (!result.success) {
        send_response(result.error_response);
        return;
    }

    // Handle notifications (no response expected)
    if (result.message.is_notification) {
        if (result.message.method == "notifications/initialized") {
            initialized = true;
            UtilityFunctions::print("MCP Meow: Client initialized");
        }
        // Notifications do not receive responses
        return;
    }

    // Handle requests
    nlohmann::json response = handle_request(result.message.method, result.message.id, result.message.params);
    send_response(response);
}

nlohmann::json MCPServer::handle_request(const std::string& method, const nlohmann::json& id, const nlohmann::json& params) {
    if (method == "initialize") {
        return mcp::create_initialize_response(id);
    }

    if (method == "tools/list") {
        return mcp::create_tools_list_response(id);
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

        return mcp::create_tool_not_found_error(id, tool_name);
    }

    return mcp::create_error_response(id, mcp::METHOD_NOT_FOUND, "Method not found: " + method);
}

void MCPServer::send_response(const nlohmann::json& response) {
    if (!client_peer.is_valid()) {
        return;
    }

    // Serialize without pretty-printing for wire format + newline delimiter
    std::string json_str = response.dump() + "\n";

    PackedByteArray data;
    data.resize(static_cast<int64_t>(json_str.size()));
    memcpy(data.ptrw(), json_str.data(), json_str.size());

    client_peer->put_data(data);
}
