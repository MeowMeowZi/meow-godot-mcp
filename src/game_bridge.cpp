#include "game_bridge.h"
#include "mcp_protocol.h"

#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

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
    UtilityFunctions::print("MCP Meow: Game debugger session started (session ", p_session_id, ")");
}

void MeowDebuggerPlugin::_on_session_stopped(int32_t p_session_id) {
    if (p_session_id == active_session_id) {
        active_session_id = -1;
        game_connected = false;

        // If there's a pending viewport capture, deliver error via deferred callback
        if (has_pending_capture && deferred_callback) {
            auto error_response = mcp::create_tool_result(pending_capture_id,
                {{"error", "Game disconnected during viewport capture"}});
            deferred_callback(error_response);
            has_pending_capture = false;
        }

        UtilityFunctions::print("MCP Meow: Game debugger session stopped (session ", p_session_id, ")");
    }
}

bool MeowDebuggerPlugin::_has_capture(const String &p_capture) const {
    return p_capture == "meow_mcp";
}

bool MeowDebuggerPlugin::_capture(const String &p_message, const Array &p_data,
                                    int32_t p_session_id) {
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
        UtilityFunctions::print("MCP Meow: Game bridge connected (session ", p_session_id, ")");
        return true;
    }

    if (action == "viewport_data") {
        if (has_pending_capture && deferred_callback) {
            String base64_godot = p_data[0];
            int img_width = p_data[1];
            int img_height = p_data[2];

            std::string base64_str(base64_godot.utf8().get_data());

            if (base64_str.empty()) {
                // Game viewport capture failed
                auto error_response = mcp::create_tool_result(pending_capture_id,
                    {{"error", "Game viewport capture failed"}});
                deferred_callback(error_response);
                has_pending_capture = false;
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
                pending_capture_id, base64_str, "image/png", metadata);
            deferred_callback(response);
            has_pending_capture = false;
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
            bool pressed = true;
            if (args.contains("button") && args["button"].is_string())
                button = args["button"].get<std::string>();
            if (args.contains("pressed") && args["pressed"].is_boolean())
                pressed = args["pressed"].get<bool>();

            Array data;
            data.push_back(x);
            data.push_back(y);
            data.push_back(String(button.c_str()));
            data.push_back(pressed);
            send_to_game("meow_mcp:inject_mouse_click", data);

            return {{"success", true}, {"type", "mouse"}, {"mouse_action", "click"},
                    {"position", {{"x", x}, {"y", y}}}, {"button", button}, {"pressed", pressed}};
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
    if (has_pending_capture) {
        return {{"error", "Another viewport capture is already pending"}};
    }

    pending_capture_id = id;
    pending_capture_width = width;
    pending_capture_height = height;
    has_pending_capture = true;

    // Send capture request to the game companion
    Array data;
    send_to_game("meow_mcp:capture_viewport", data);

    // Return deferred marker -- MCPServer will NOT send response yet
    return {{"__deferred", true}};
}
