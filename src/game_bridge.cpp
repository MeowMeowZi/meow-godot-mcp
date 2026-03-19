#include "game_bridge.h"
#include "mcp_protocol.h"

#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <chrono>

using namespace godot;

void MeowDebuggerPlugin::_bind_methods() {
    // Required for GDCLASS -- no properties to expose
}

void MeowDebuggerPlugin::_setup_session(int32_t p_session_id) {
    Ref<EditorDebuggerSession> session = get_session(p_session_id);
    if (session.is_valid()) {
        session->connect("started",
            callable_mp(this, &MeowDebuggerPlugin::_on_session_started).bind(p_session_id));
        session->connect("stopped",
            callable_mp(this, &MeowDebuggerPlugin::_on_session_stopped).bind(p_session_id));
    }
}

void MeowDebuggerPlugin::_on_session_started(int32_t p_session_id) {
    active_session_id = p_session_id;
    log_buffer.clear();
    log_buffer_read_pos = 0;
    UtilityFunctions::print(String::utf8("MCP Meow: 游戏调试会话已启动 (会话 "), p_session_id, ")");
}

void MeowDebuggerPlugin::_on_session_stopped(int32_t p_session_id) {
    if (p_session_id == active_session_id) {
        active_session_id = -1;
        game_connected = false;
        log_buffer.clear();
        log_buffer_read_pos = 0;

        // If there's a pending deferred request, deliver error via deferred callback
        if (pending_type != PendingType::NONE && deferred_callback) {
            std::string error_msg;
            switch (pending_type) {
                case PendingType::VIEWPORT_CAPTURE:
                    error_msg = "Game disconnected during viewport capture";
                    break;
                case PendingType::CLICK_NODE:
                    error_msg = "Game disconnected during click_node";
                    break;
                case PendingType::GET_NODE_RECT:
                    error_msg = "Game disconnected during get_node_rect";
                    break;
                case PendingType::GET_NODE_PROPERTY:
                    error_msg = "Game disconnected during get_game_node_property";
                    break;
                case PendingType::EVAL_IN_GAME:
                    error_msg = "Game disconnected during eval_in_game";
                    break;
                case PendingType::GET_GAME_SCENE_TREE:
                    error_msg = "Game disconnected during get_game_scene_tree";
                    break;
                case PendingType::RUN_TEST_SEQUENCE:
                    error_msg = "Game disconnected during test sequence";
                    break;
                default:
                    error_msg = "Game disconnected during pending operation";
                    break;
            }
            auto error_response = mcp::create_tool_result(pending_id, {{"error", error_msg}});
            deferred_callback(error_response);
            pending_type = PendingType::NONE;
        }

        UtilityFunctions::print(String::utf8("MCP Meow: 游戏调试会话已停止 (会话 "), p_session_id, ")");
    }
}

bool MeowDebuggerPlugin::_has_capture(const String &p_capture) const {
    return p_capture == "meow_mcp" || p_capture == "output";
}

bool MeowDebuggerPlugin::_capture(const String &p_message, const Array &p_data,
                                    int32_t p_session_id) {
    // Phase 14: Capture output messages from the running game (print, push_error, push_warning)
    // Godot routes "output" prefix messages to plugins that return true for _has_capture("output")
    if (p_message.begins_with("output")) {
        if (p_data.size() >= 2) {
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            if (p_data[0].get_type() == godot::Variant::STRING) {
                // Single text+type pair: data[0]=text, data[1]=type_int
                String text = p_data[0];
                int type_val = p_data[1];
                std::string level = "info";
                if (type_val == 1) level = "error";
                else if (type_val == 2) level = "warning";

                std::string msg(text.utf8().get_data());
                if (!msg.empty() && msg.back() == '\n') msg.pop_back();
                if (!msg.empty()) {
                    log_buffer.push_back({msg, level, now_ms});
                }
            } else if (p_data[0].get_type() == godot::Variant::PACKED_STRING_ARRAY ||
                       p_data[0].get_type() == godot::Variant::ARRAY) {
                // Array format: data[0]=Array<String>, data[1]=Array<int>
                Array texts = p_data[0];
                Array types = p_data[1];
                for (int i = 0; i < texts.size(); i++) {
                    String text = texts[i];
                    int type_val = (i < types.size()) ? (int)types[i] : 0;
                    std::string level = "info";
                    if (type_val == 1) level = "error";
                    else if (type_val == 2) level = "warning";
                    std::string msg(text.utf8().get_data());
                    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
                    if (!msg.empty()) {
                        log_buffer.push_back({msg, level, now_ms});
                    }
                }
            }
        }
        return true;
    }

    // p_message is full string like "meow_mcp:ready"
    // Extract action after the colon
    int colon_pos = p_message.find(":");
    if (colon_pos < 0) {
        return false;
    }
    String action = p_message.substr(colon_pos + 1);

    if (action == "ready") {
        game_connected = true;
        active_session_id = p_session_id;
        UtilityFunctions::print(String::utf8("MCP Meow: 游戏桥接已连接 (会话 "), p_session_id, ")");
        return true;
    }

    if (action == "viewport_data") {
        if (pending_type == PendingType::VIEWPORT_CAPTURE && deferred_callback) {
            String base64_godot = p_data[0];
            int img_width = p_data[1];
            int img_height = p_data[2];

            std::string base64_str(base64_godot.utf8().get_data());

            if (base64_str.empty()) {
                // Game viewport capture failed
                auto error_response = mcp::create_tool_result(pending_id,
                    {{"error", "Game viewport capture failed"}});
                deferred_callback(error_response);
                pending_type = PendingType::NONE;
                return true;
            }

            // Optional resize if requested
            if (pending_capture_width > 0 || pending_capture_height > 0) {
                PackedByteArray raw = Marshalls::get_singleton()->base64_to_raw(base64_godot);
                Ref<Image> image;
                image.instantiate();
                image->load_png_from_buffer(raw);

                if (image.is_valid() && !image->is_empty()) {
                    int orig_w = image->get_width();
                    int orig_h = image->get_height();

                    if (pending_capture_width > 0 && pending_capture_height > 0) {
                        image->resize(pending_capture_width, pending_capture_height,
                                      Image::INTERPOLATE_LANCZOS);
                    } else if (pending_capture_width > 0) {
                        double scale = static_cast<double>(pending_capture_width) / orig_w;
                        int scaled_h = static_cast<int>(orig_h * scale);
                        image->resize(pending_capture_width, scaled_h,
                                      Image::INTERPOLATE_LANCZOS);
                    } else if (pending_capture_height > 0) {
                        double scale = static_cast<double>(pending_capture_height) / orig_h;
                        int scaled_w = static_cast<int>(orig_w * scale);
                        image->resize(scaled_w, pending_capture_height,
                                      Image::INTERPOLATE_LANCZOS);
                    }

                    // Re-encode to PNG and base64
                    PackedByteArray png_data = image->save_png_to_buffer();
                    String resized_b64 = Marshalls::get_singleton()->raw_to_base64(png_data);
                    base64_str = std::string(resized_b64.utf8().get_data());
                    img_width = image->get_width();
                    img_height = image->get_height();
                }
            }

            nlohmann::json metadata = {
                {"viewport_type", "game"},
                {"width", img_width},
                {"height", img_height}
            };

            auto response = mcp::create_image_tool_result(
                pending_id, base64_str, "image/png", metadata);
            deferred_callback(response);
            pending_type = PendingType::NONE;
        }
        return true;
    }

    if (action == "click_node_result") {
        bool success = p_data[0];
        String error_msg = p_data[1];
        double cx = p_data[2];
        double cy = p_data[3];

        nlohmann::json result;
        if (success) {
            result = {{"success", true}, {"clicked_position", {{"x", cx}, {"y", cy}}}};
        } else {
            result = {{"error", std::string(error_msg.utf8().get_data())}};
        }

        if (pending_type == PendingType::RUN_TEST_SEQUENCE) {
            _advance_test_sequence(result);
        } else if (pending_type == PendingType::CLICK_NODE && deferred_callback) {
            auto response = mcp::create_tool_result(pending_id, result);
            deferred_callback(response);
            pending_type = PendingType::NONE;
        }
        return true;
    }

    if (action == "node_rect_result") {
        bool success = p_data[0];
        String error_msg = p_data[1];

        nlohmann::json result;
        if (success) {
            double px = p_data[2];
            double py = p_data[3];
            double sx = p_data[4];
            double sy = p_data[5];
            double gx = p_data[6];
            double gy = p_data[7];
            result = {
                {"position", {{"x", px}, {"y", py}}},
                {"size", {{"width", sx}, {"height", sy}}},
                {"global_position", {{"x", gx}, {"y", gy}}},
                {"center", {{"x", px + sx / 2.0}, {"y", py + sy / 2.0}}},
                {"visible", true}
            };
        } else {
            result = {{"error", std::string(error_msg.utf8().get_data())}};
        }

        if (pending_type == PendingType::RUN_TEST_SEQUENCE) {
            _advance_test_sequence(result);
        } else if (pending_type == PendingType::GET_NODE_RECT && deferred_callback) {
            auto response = mcp::create_tool_result(pending_id, result);
            deferred_callback(response);
            pending_type = PendingType::NONE;
        }
        return true;
    }

    // --- Phase 13: Runtime State Query result handlers ---

    if (action == "node_property_result") {
        bool success = p_data[0];
        String message = p_data[1];
        String value_str = p_data[2];
        String type_str = p_data[3];

        nlohmann::json result;
        if (success) {
            result = {
                {"value", std::string(value_str.utf8().get_data())},
                {"type", std::string(type_str.utf8().get_data())}
            };
        } else {
            result = {{"error", std::string(message.utf8().get_data())}};
        }

        if (pending_type == PendingType::RUN_TEST_SEQUENCE) {
            _advance_test_sequence(result);
        } else if (pending_type == PendingType::GET_NODE_PROPERTY && deferred_callback) {
            auto response = mcp::create_tool_result(pending_id, result);
            deferred_callback(response);
            pending_type = PendingType::NONE;
        }
        return true;
    }

    if (action == "eval_result") {
        bool success = p_data[0];
        String message = p_data[1];
        String value_str = p_data[2];

        nlohmann::json result;
        if (success) {
            result = {{"result", std::string(value_str.utf8().get_data())}};
        } else {
            result = {{"error", std::string(message.utf8().get_data())}};
        }

        if (pending_type == PendingType::RUN_TEST_SEQUENCE) {
            _advance_test_sequence(result);
        } else if (pending_type == PendingType::EVAL_IN_GAME && deferred_callback) {
            auto response = mcp::create_tool_result(pending_id, result);
            deferred_callback(response);
            pending_type = PendingType::NONE;
        }
        return true;
    }

    if (action == "game_scene_tree_result") {
        bool success = p_data[0];
        String message = p_data[1];
        String json_str = p_data[2];

        nlohmann::json result;
        if (success) {
            try {
                result = nlohmann::json::parse(std::string(json_str.utf8().get_data()));
            } catch (...) {
                result = {{"error", "Failed to parse scene tree JSON from game"}};
            }
        } else {
            result = {{"error", std::string(message.utf8().get_data())}};
        }

        if (pending_type == PendingType::RUN_TEST_SEQUENCE) {
            _advance_test_sequence(result);
        } else if (pending_type == PendingType::GET_GAME_SCENE_TREE && deferred_callback) {
            auto response = mcp::create_tool_result(pending_id, result);
            deferred_callback(response);
            pending_type = PendingType::NONE;
        }
        return true;
    }

    if (action == "input_result") {
        // Fire-and-forget acknowledgement; log errors if any
        if (p_data.size() >= 2) {
            bool success = p_data[0];
            String error_msg = p_data[1];
            if (!success && !error_msg.is_empty()) {
                UtilityFunctions::printerr("MCP Meow: Game input injection error: ", error_msg);
            }
        }
        return true;
    }

    return false;
}

void MeowDebuggerPlugin::send_to_game(const String &message, const Array &data) {
    if (active_session_id < 0 || !game_connected) {
        return;
    }
    Ref<EditorDebuggerSession> session = get_session(active_session_id);
    if (session.is_valid()) {
        session->send_message(message, data);
    }
}

bool MeowDebuggerPlugin::is_game_connected() const {
    return game_connected;
}

void MeowDebuggerPlugin::set_deferred_response_callback(DeferredCallback cb) {
    deferred_callback = cb;
}

// --- Phase 14: Log buffer methods ---

nlohmann::json MeowDebuggerPlugin::get_buffered_game_output(
    bool clear_after_read, const std::string& level_filter,
    int64_t since_ms, const std::string& keyword) {

    nlohmann::json lines = nlohmann::json::array();
    size_t start = static_cast<size_t>(log_buffer_read_pos);

    for (size_t i = start; i < log_buffer.size(); i++) {
        const auto& entry = log_buffer[i];

        // Filter by level
        if (!level_filter.empty() && entry.level != level_filter) continue;

        // Filter by time
        if (since_ms > 0 && entry.timestamp_ms < since_ms) continue;

        // Filter by keyword
        if (!keyword.empty() && entry.message.find(keyword) == std::string::npos) continue;

        lines.push_back({
            {"text", entry.message},
            {"level", entry.level},
            {"timestamp_ms", entry.timestamp_ms}
        });
    }

    if (clear_after_read) {
        log_buffer_read_pos = static_cast<int64_t>(log_buffer.size());
    }

    return {
        {"success", true},
        {"lines", lines},
        {"count", lines.size()},
        {"total_buffered", log_buffer.size()},
        {"game_running", game_connected}
    };
}

void MeowDebuggerPlugin::clear_log_buffer() {
    log_buffer.clear();
    log_buffer_read_pos = 0;
}

nlohmann::json MeowDebuggerPlugin::inject_input_tool(const nlohmann::json& args) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }

    if (!args.contains("type") || !args["type"].is_string()) {
        return {{"error", "Missing required parameter: type (key, mouse, or action)"}};
    }
    std::string type = args["type"].get<std::string>();

    if (type == "key") {
        std::string keycode;
        bool pressed = true;
        if (args.contains("keycode") && args["keycode"].is_string()) {
            keycode = args["keycode"].get<std::string>();
        }
        if (keycode.empty()) {
            return {{"error", "Missing required parameter: keycode (for type 'key')"}};
        }
        if (args.contains("pressed") && args["pressed"].is_boolean()) {
            pressed = args["pressed"].get<bool>();
        }

        Array data;
        data.push_back(String(keycode.c_str()));
        data.push_back(pressed);
        send_to_game("meow_mcp:inject_key", data);

        return {{"success", true}, {"type", "key"}, {"keycode", keycode}, {"pressed", pressed}};
    }

    if (type == "mouse") {
        std::string mouse_action;
        double x = 0.0, y = 0.0;
        if (args.contains("mouse_action") && args["mouse_action"].is_string()) {
            mouse_action = args["mouse_action"].get<std::string>();
        }
        if (mouse_action.empty()) {
            return {{"error", "Missing required parameter: mouse_action (for type 'mouse')"}};
        }
        if (args.contains("position") && args["position"].is_object()) {
            auto& pos = args["position"];
            if (pos.contains("x") && pos["x"].is_number()) x = pos["x"].get<double>();
            if (pos.contains("y") && pos["y"].is_number()) y = pos["y"].get<double>();
        }

        if (mouse_action == "click") {
            std::string button = "left";
            if (args.contains("button") && args["button"].is_string())
                button = args["button"].get<std::string>();

            // Check if pressed is explicitly provided
            bool explicit_pressed = args.contains("pressed") && args["pressed"].is_boolean();

            if (!explicit_pressed) {
                // Auto-cycle mode: tell game to do press+delay+release
                Array data;
                data.push_back(x);
                data.push_back(y);
                data.push_back(String(button.c_str()));
                data.push_back(true);  // auto_cycle flag
                send_to_game("meow_mcp:inject_mouse_click", data);

                return {{"success", true}, {"type", "mouse"}, {"mouse_action", "click"},
                        {"position", {{"x", x}, {"y", y}}}, {"button", button}, {"auto_cycle", true}};
            } else {
                // Backward-compatible single-fire behavior
                bool pressed = args["pressed"].get<bool>();
                Array data;
                data.push_back(x);
                data.push_back(y);
                data.push_back(String(button.c_str()));
                data.push_back(pressed);
                send_to_game("meow_mcp:inject_mouse_click", data);

                return {{"success", true}, {"type", "mouse"}, {"mouse_action", "click"},
                        {"position", {{"x", x}, {"y", y}}}, {"button", button}, {"pressed", pressed}};
            }
        }

        if (mouse_action == "move") {
            Array data;
            data.push_back(x);
            data.push_back(y);
            send_to_game("meow_mcp:inject_mouse_move", data);

            return {{"success", true}, {"type", "mouse"}, {"mouse_action", "move"},
                    {"position", {{"x", x}, {"y", y}}}};
        }

        if (mouse_action == "scroll") {
            std::string direction = "up";
            if (args.contains("direction") && args["direction"].is_string())
                direction = args["direction"].get<std::string>();

            Array data;
            data.push_back(x);
            data.push_back(y);
            data.push_back(String(direction.c_str()));
            send_to_game("meow_mcp:inject_mouse_scroll", data);

            return {{"success", true}, {"type", "mouse"}, {"mouse_action", "scroll"},
                    {"position", {{"x", x}, {"y", y}}}, {"direction", direction}};
        }

        return {{"error", "Unknown mouse_action: " + mouse_action + ". Valid: click, move, scroll"}};
    }

    if (type == "action") {
        std::string action_name;
        bool pressed = true;
        if (args.contains("action_name") && args["action_name"].is_string()) {
            action_name = args["action_name"].get<std::string>();
        }
        if (action_name.empty()) {
            return {{"error", "Missing required parameter: action_name (for type 'action')"}};
        }
        if (args.contains("pressed") && args["pressed"].is_boolean()) {
            pressed = args["pressed"].get<bool>();
        }

        Array data;
        data.push_back(String(action_name.c_str()));
        data.push_back(pressed);
        send_to_game("meow_mcp:inject_action", data);

        return {{"success", true}, {"type", "action"}, {"action_name", action_name}, {"pressed", pressed}};
    }

    return {{"error", "Unknown input type: " + type + ". Valid: key, mouse, action"}};
}

nlohmann::json MeowDebuggerPlugin::get_bridge_status_tool() {
    return {{"connected", game_connected}, {"session_id", active_session_id}};
}

nlohmann::json MeowDebuggerPlugin::request_game_viewport_capture(
    const nlohmann::json& id, int width, int height) {

    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }

    pending_id = id;
    pending_capture_width = width;
    pending_capture_height = height;
    pending_type = PendingType::VIEWPORT_CAPTURE;

    // Send capture request to the game companion
    Array data;
    send_to_game("meow_mcp:capture_viewport", data);

    // Return deferred marker -- MCPServer will NOT send response yet
    return {{"__deferred", true}};
}

nlohmann::json MeowDebuggerPlugin::click_node_tool(const nlohmann::json& id, const std::string& node_path) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }

    pending_type = PendingType::CLICK_NODE;
    pending_id = id;

    Array data;
    data.push_back(String(node_path.c_str()));
    send_to_game("meow_mcp:click_node", data);

    return {{"__deferred", true}};
}

nlohmann::json MeowDebuggerPlugin::get_node_rect_tool(const nlohmann::json& id, const std::string& node_path) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }

    pending_type = PendingType::GET_NODE_RECT;
    pending_id = id;

    Array data;
    data.push_back(String(node_path.c_str()));
    send_to_game("meow_mcp:get_node_rect", data);

    return {{"__deferred", true}};
}

// --- Phase 13: Runtime State Query tools ---

nlohmann::json MeowDebuggerPlugin::get_game_node_property_tool(
    const nlohmann::json& id, const std::string& node_path, const std::string& property) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }

    pending_type = PendingType::GET_NODE_PROPERTY;
    pending_id = id;

    Array data;
    data.push_back(String(node_path.c_str()));
    data.push_back(String(property.c_str()));
    send_to_game("meow_mcp:get_node_property", data);

    return {{"__deferred", true}};
}

nlohmann::json MeowDebuggerPlugin::eval_in_game_tool(
    const nlohmann::json& id, const std::string& expression) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }

    pending_type = PendingType::EVAL_IN_GAME;
    pending_id = id;

    Array data;
    data.push_back(String(expression.c_str()));
    send_to_game("meow_mcp:eval_in_game", data);

    return {{"__deferred", true}};
}

nlohmann::json MeowDebuggerPlugin::get_game_scene_tree_tool(
    const nlohmann::json& id, int max_depth) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }

    pending_type = PendingType::GET_GAME_SCENE_TREE;
    pending_id = id;

    Array data;
    data.push_back(max_depth);
    send_to_game("meow_mcp:get_game_scene_tree", data);

    return {{"__deferred", true}};
}

// --- Phase 15: Integration Testing Toolkit ---

nlohmann::json MeowDebuggerPlugin::run_test_sequence_tool(
    const nlohmann::json& id, const nlohmann::json& steps) {
    if (!is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    if (pending_type != PendingType::NONE) {
        return {{"error", "Another deferred request is already pending"}};
    }
    if (!steps.is_array() || steps.empty()) {
        return {{"error", "steps must be a non-empty array"}};
    }

    // Initialize test sequence state
    test_steps.clear();
    for (const auto& step : steps) {
        test_steps.push_back(step);
    }
    test_step_index = 0;
    test_results = nlohmann::json::array();
    test_sequence_id = id;
    pending_type = PendingType::RUN_TEST_SEQUENCE;

    // Start executing the first step
    _execute_test_step(0);

    return {{"__deferred", true}};
}

void MeowDebuggerPlugin::_execute_test_step(size_t index) {
    if (index >= test_steps.size()) {
        // Should not happen -- _advance_test_sequence handles completion
        _advance_test_sequence({{"error", "Step index out of range"}});
        return;
    }

    auto& step = test_steps[index];
    std::string action = step.value("action", "");
    nlohmann::json args = step.value("args", nlohmann::json::object());

    // --- Deferred actions (send message to game, wait for _capture response) ---

    if (action == "click_node") {
        std::string node_path = args.value("node_path", "");
        if (node_path.empty()) {
            _advance_test_sequence({{"error", "click_node requires node_path"}});
            return;
        }
        Array data;
        data.push_back(String(node_path.c_str()));
        send_to_game("meow_mcp:click_node", data);
        return; // Wait for click_node_result in _capture
    }

    if (action == "get_game_node_property") {
        std::string node_path = args.value("node_path", "");
        std::string property = args.value("property", "");
        if (property.empty()) {
            _advance_test_sequence({{"error", "get_game_node_property requires property"}});
            return;
        }
        Array data;
        data.push_back(String(node_path.c_str()));
        data.push_back(String(property.c_str()));
        send_to_game("meow_mcp:get_node_property", data);
        return; // Wait for node_property_result in _capture
    }

    if (action == "eval_in_game") {
        std::string expression = args.value("expression", "");
        if (expression.empty()) {
            _advance_test_sequence({{"error", "eval_in_game requires expression"}});
            return;
        }
        Array data;
        data.push_back(String(expression.c_str()));
        send_to_game("meow_mcp:eval_in_game", data);
        return; // Wait for eval_result in _capture
    }

    if (action == "get_game_scene_tree") {
        int max_depth = args.value("max_depth", -1);
        Array data;
        data.push_back(max_depth);
        send_to_game("meow_mcp:get_game_scene_tree", data);
        return; // Wait for game_scene_tree_result in _capture
    }

    if (action == "get_node_rect") {
        std::string node_path = args.value("node_path", "");
        if (node_path.empty()) {
            _advance_test_sequence({{"error", "get_node_rect requires node_path"}});
            return;
        }
        Array data;
        data.push_back(String(node_path.c_str()));
        send_to_game("meow_mcp:get_node_rect", data);
        return; // Wait for node_rect_result in _capture
    }

    // --- Synchronous actions (no deferred wait needed) ---

    if (action == "inject_input") {
        auto result = inject_input_tool(args);
        _advance_test_sequence(result);
        return;
    }

    if (action == "get_game_output") {
        bool clear = args.value("clear_after_read", true);
        std::string level = args.value("level", "");
        int64_t since = args.value("since", 0);
        std::string keyword = args.value("keyword", "");
        auto result = get_buffered_game_output(clear, level, since, keyword);
        _advance_test_sequence(result);
        return;
    }

    if (action == "wait") {
        int ms = args.value("duration_ms", 500);
        // Block main thread briefly (acceptable for test tool, typical 100-500ms)
        OS::get_singleton()->delay_usec(ms * 1000);
        _advance_test_sequence({{"waited_ms", ms}});
        return;
    }

    _advance_test_sequence({{"error", "Unknown action: " + action}});
}

void MeowDebuggerPlugin::_advance_test_sequence(const nlohmann::json& step_result) {
    // Record result for current step
    auto& step = test_steps[test_step_index];
    nlohmann::json step_report;
    step_report["step"] = test_step_index;
    step_report["action"] = step.value("action", "unknown");
    step_report["description"] = step.value("description", "");
    step_report["result"] = step_result;

    // Evaluate assertion if present
    if (step.contains("assert") && step["assert"].is_object()) {
        auto assertion_result = _evaluate_assertion(step["assert"], step_result);
        step_report["assertion"] = assertion_result;
        step_report["passed"] = assertion_result.value("passed", false);
    } else {
        // No assertion -- pass if no error
        step_report["passed"] = !step_result.contains("error");
    }

    test_results.push_back(step_report);
    test_step_index++;

    // Execute next step or finish
    if (test_step_index < test_steps.size()) {
        _execute_test_step(test_step_index);
    } else {
        // All steps done -- build final report
        int total = static_cast<int>(test_results.size());
        int passed = 0;
        for (auto& r : test_results) {
            if (r.value("passed", false)) passed++;
        }
        nlohmann::json final_report = {
            {"success", true},
            {"total_steps", total},
            {"passed", passed},
            {"failed", total - passed},
            {"all_passed", passed == total},
            {"steps", test_results}
        };

        auto response = mcp::create_tool_result(test_sequence_id, final_report);
        if (deferred_callback) {
            deferred_callback(response);
        }
        pending_type = PendingType::NONE;
        test_steps.clear();
        test_results = nlohmann::json::array();
    }
}

nlohmann::json MeowDebuggerPlugin::_evaluate_assertion(
    const nlohmann::json& assert_def, const nlohmann::json& result) {

    std::string property = assert_def.value("property", "");
    nlohmann::json assertion_result = {{"passed", false}};

    // Get the value to check
    std::string actual_value;
    bool has_property = false;
    if (property.empty()) {
        // Check the entire result as string
        actual_value = result.dump();
        has_property = true;
    } else if (result.contains(property)) {
        has_property = true;
        if (result[property].is_string()) {
            actual_value = result[property].get<std::string>();
        } else {
            actual_value = result[property].dump();
        }
    }

    assertion_result["actual"] = actual_value;
    assertion_result["has_property"] = has_property;

    if (assert_def.contains("equals")) {
        std::string expected = assert_def["equals"].get<std::string>();
        assertion_result["operator"] = "equals";
        assertion_result["expected"] = expected;
        assertion_result["passed"] = (actual_value == expected);
    } else if (assert_def.contains("contains")) {
        std::string expected = assert_def["contains"].get<std::string>();
        assertion_result["operator"] = "contains";
        assertion_result["expected"] = expected;
        assertion_result["passed"] = (actual_value.find(expected) != std::string::npos);
    } else if (assert_def.contains("not_empty")) {
        assertion_result["operator"] = "not_empty";
        assertion_result["passed"] = has_property && !actual_value.empty();
    } else {
        // No operator specified -- just check property exists
        assertion_result["operator"] = "exists";
        assertion_result["passed"] = has_property;
    }

    return assertion_result;
}
