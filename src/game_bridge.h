#ifndef MEOW_GODOT_MCP_GAME_BRIDGE_H
#define MEOW_GODOT_MCP_GAME_BRIDGE_H

#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <functional>

class MeowDebuggerPlugin : public godot::EditorDebuggerPlugin {
    GDCLASS(MeowDebuggerPlugin, godot::EditorDebuggerPlugin);

public:
    // EditorDebuggerPlugin virtual overrides
    void _setup_session(int32_t p_session_id) override;
    bool _has_capture(const godot::String &p_capture) const override;
    bool _capture(const godot::String &p_message, const godot::Array &p_data,
                  int32_t p_session_id) override;

    // Tool functions called by MCPServer dispatch
    nlohmann::json inject_input_tool(const nlohmann::json& args);
    nlohmann::json get_bridge_status_tool();

    // Deferred capture: initiates request, returns special marker
    // When response arrives via _capture, calls the deferred_callback
    nlohmann::json request_game_viewport_capture(const nlohmann::json& id, int width, int height);

    // State queries
    bool is_game_connected() const;

    // Deferred response callback (set by MCPServer)
    // Called from _capture on main thread when viewport data arrives
    using DeferredCallback = std::function<void(const nlohmann::json& response)>;
    void set_deferred_response_callback(DeferredCallback cb);

protected:
    static void _bind_methods();

private:
    void _on_session_started(int32_t p_session_id);
    void _on_session_stopped(int32_t p_session_id);

    void send_to_game(const godot::String &message, const godot::Array &data);

    int active_session_id = -1;
    bool game_connected = false;

    // Pending viewport capture
    nlohmann::json pending_capture_id;  // MCP request id waiting for response
    int pending_capture_width = 0;
    int pending_capture_height = 0;
    bool has_pending_capture = false;

    DeferredCallback deferred_callback;
};

#endif // MEOW_GODOT_MCP_GAME_BRIDGE_H
