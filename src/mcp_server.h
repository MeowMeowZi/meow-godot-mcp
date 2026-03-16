#ifndef GODOT_MCP_MEOW_MCP_SERVER_H
#define GODOT_MCP_MEOW_MCP_SERVER_H

#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

// Plain C++ class (NOT a Godot Object) -- owned by MCPPlugin
class MCPServer {
public:
    MCPServer();
    ~MCPServer();

    void start(int port = 6800);
    void stop();
    void poll();
    bool is_running() const;

    void set_undo_redo(godot::EditorUndoRedoManager* ur);

private:
    void process_message(const std::string& line);
    void send_response(const nlohmann::json& response);
    nlohmann::json handle_request(const std::string& method, const nlohmann::json& id, const nlohmann::json& params);

    godot::Ref<godot::TCPServer> tcp_server;
    godot::Ref<godot::StreamPeerTCP> client_peer;
    std::string read_buffer;
    bool initialized;
    int port;
    bool running;
    godot::EditorUndoRedoManager* undo_redo = nullptr;
};

#endif // GODOT_MCP_MEOW_MCP_SERVER_H
