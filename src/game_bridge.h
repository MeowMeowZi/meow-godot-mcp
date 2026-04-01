#ifndef MEOW_GODOT_MCP_GAME_BRIDGE_H
#define MEOW_GODOT_MCP_GAME_BRIDGE_H

#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <chrono>

struct LogEntry {
    std::string message;
    std::string level;    // "info", "warning", "error"
    int64_t timestamp_ms; // milliseconds since epoch (steady_clock)
};

enum class PendingType { NONE, VIEWPORT_CAPTURE, CLICK_NODE, GET_NODE_RECT, GET_NODE_PROPERTY, EVAL_IN_GAME, GET_GAME_SCENE_TREE, RUN_TEST_SEQUENCE, INPUT_SEQUENCE, INJECT_TEXT };

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
    nlohmann::json click_node_tool(const nlohmann::json& id, const std::string& node_path);
    nlohmann::json get_node_rect_tool(const nlohmann::json& id, const std::string& node_path);

    // Phase 13: Runtime State Query tools
    nlohmann::json get_game_node_property_tool(const nlohmann::json& id, const std::string& node_path, const std::string& property);
    nlohmann::json eval_in_game_tool(const nlohmann::json& id, const std::string& expression);
    nlohmann::json get_game_scene_tree_tool(const nlohmann::json& id, int max_depth);

    // Deferred capture: initiates request, returns special marker
    // When response arrives via _capture, calls the deferred_callback
    nlohmann::json request_game_viewport_capture(const nlohmann::json& id, int width, int height);

    // Phase 15: Integration Testing Toolkit
    nlohmann::json run_test_sequence_tool(const nlohmann::json& id, const nlohmann::json& steps);

    // Input sequence and text injection
    nlohmann::json inject_input_sequence_tool(const nlohmann::json& id, const nlohmann::json& steps);
    nlohmann::json inject_text_tool(const nlohmann::json& id, const std::string& node_path, const std::string& text);

    // Phase 14: Log buffer methods
    nlohmann::json get_buffered_game_output(bool clear_after_read, const std::string& level_filter,
                                             int64_t since_ms, const std::string& keyword);
    void clear_log_buffer();

    // State queries
    bool is_game_connected() const;

    // Deferred request timeout (TIMEOUT-02)
    bool has_pending_timeout() const;
    void expire_pending();

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
    std::string last_exit_reason;  // Populated when game disconnects unexpectedly

    // Phase 14: Log buffer for captured game output
    std::vector<LogEntry> log_buffer;
    int64_t log_buffer_read_pos = 0;

    // Pending deferred request (viewport capture, click_node, get_node_rect)
    PendingType pending_type = PendingType::NONE;
    nlohmann::json pending_id;          // MCP request id for any pending deferred request
    std::chrono::steady_clock::time_point pending_deadline;  // Deadline for deferred request timeout
    int pending_capture_width = 0;      // For viewport capture resize
    int pending_capture_height = 0;     // For viewport capture resize

    DeferredCallback deferred_callback;

    // Phase 15: Test sequence state machine
    void _execute_test_step(size_t index);
    void _advance_test_sequence(const nlohmann::json& step_result);
    nlohmann::json _evaluate_assertion(const nlohmann::json& assert_def, const nlohmann::json& result);

    std::vector<nlohmann::json> test_steps;
    size_t test_step_index = 0;
    nlohmann::json test_results;
    nlohmann::json test_sequence_id;
};

#endif // MEOW_GODOT_MCP_GAME_BRIDGE_H
