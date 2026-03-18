#ifndef MEOW_GODOT_MCP_MCP_SERVER_H
#define MEOW_GODOT_MCP_MCP_SERVER_H

#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "mcp_tool_registry.h"

#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace godot {
class EditorUndoRedoManager;
}

struct PendingRequest {
    std::string method;
    nlohmann::json id;
    nlohmann::json params;
};

struct PendingResponse {
    nlohmann::json response;
};

// Plain C++ class (NOT a Godot Object) -- owned by MCPPlugin
// Two-thread architecture:
//   IO thread: TCP accept/read/write, JSON-RPC parse, enqueue requests, send responses
//   Main thread (poll): dequeue requests, execute Godot API calls, enqueue responses
class MCPServer {
public:
    MCPServer();
    ~MCPServer();

    void start(int port = 6800);
    void stop();
    void poll();  // Called from _process on main thread
    bool is_running() const;
    bool has_client() const;

    void set_undo_redo(godot::EditorUndoRedoManager* ur);
    void set_godot_version(const GodotVersion& v);

private:
    // Request handling (main thread only)
    nlohmann::json handle_request(const std::string& method, const nlohmann::json& id, const nlohmann::json& params);

    // IO thread function
    void io_thread_func();

    // Process a complete JSON-RPC line (called from IO thread)
    // Returns true if handled inline (notification/error), false if queued for main thread
    bool process_message_io(const std::string& line);

    // TCP state (accessed from IO thread only after start)
    godot::Ref<godot::TCPServer> tcp_server;
    godot::Ref<godot::StreamPeerTCP> client_peer;
    std::string read_buffer;
    bool initialized;
    int port;

    // Threading
    std::thread io_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> client_connected{false};
    std::mutex queue_mutex;
    std::queue<PendingRequest> request_queue;
    std::queue<PendingResponse> response_queue;
    std::condition_variable response_cv;

    // Godot resources (main thread only)
    godot::EditorUndoRedoManager* undo_redo = nullptr;
    GodotVersion godot_version{4, 3, 0};
};

#endif // MEOW_GODOT_MCP_MCP_SERVER_H
