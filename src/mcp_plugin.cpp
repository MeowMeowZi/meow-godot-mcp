#include "mcp_plugin.h"
#include "mcp_server.h"
#include "mcp_dock.h"
#include "mcp_tool_registry.h"
#include "game_bridge.h"

#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

using namespace godot;

MCPPlugin::MCPPlugin()
    : server(nullptr), dock(nullptr), port(6800),
      status_timer(0.0), detected_version{4, 3, 0},
      version_string("4.3.0"), tool_count(0) {
}

MCPPlugin::~MCPPlugin() {
    if (dock) {
        delete dock;
        dock = nullptr;
    }
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
}

void MCPPlugin::_enter_tree() {
    // Detect Godot version via Engine singleton
    Engine* engine = Engine::get_singleton();
    if (engine) {
        Dictionary info = engine->get_version_info();
        detected_version.major = static_cast<int>(info["major"]);
        detected_version.minor = static_cast<int>(info["minor"]);
        detected_version.patch = static_cast<int>(info["patch"]);
        version_string = std::to_string(detected_version.major) + "."
                       + std::to_string(detected_version.minor) + "."
                       + std::to_string(detected_version.patch);
    }

    // Compute available tool count for detected version
    tool_count = get_tool_count(detected_version);

    // Read port from ProjectSettings (per-project configurable)
    {
        auto* ps = ProjectSettings::get_singleton();
        if (ps) {
            // Register setting with default value if not exists
            if (!ps->has_setting("meow_mcp/server/port")) {
                ps->set_setting("meow_mcp/server/port", 6800);
            }
            // Set property info for editor display
            Dictionary port_info;
            port_info["name"] = "meow_mcp/server/port";
            port_info["type"] = Variant::INT;
            port_info["hint"] = PROPERTY_HINT_RANGE;
            port_info["hint_string"] = "1024,65535,1";
            ps->add_property_info(port_info);
            // Set initial value (makes it appear in ProjectSettings UI)
            ps->set_initial_value("meow_mcp/server/port", 6800);

            port = ps->get_setting("meow_mcp/server/port");
        }
    }

    // Create and start server
    server = new MCPServer();
    server->set_undo_redo(get_undo_redo());
    server->set_godot_version(detected_version);

    // Try configured port, auto-increment on conflict (up to +10)
    int configured_port = port;
    int actual_port = 0;
    int max_attempts = 10;
    for (int i = 0; i < max_attempts; i++) {
        actual_port = server->start(port + i);
        if (actual_port > 0) {
            port = actual_port;
            break;
        }
    }
    if (actual_port == 0) {
        UtilityFunctions::printerr("MCP Meow: Failed to start on ports ",
            configured_port, "-", configured_port + max_attempts - 1);
    } else if (actual_port != configured_port) {
        UtilityFunctions::push_warning(
            String::utf8("MCP Meow: 端口 ") + String::num_int64(configured_port)
            + String::utf8(" 被占用，自动切换到 ") + String::num_int64(actual_port));
    }

    // Register game bridge debugger plugin
    debugger_plugin.instantiate();
    add_debugger_plugin(debugger_plugin);

    // Register companion autoload for game-side bridge
    // DISABLED: add_autoload_singleton crashes run_game in Godot 4.6
    // Autoload must be pre-configured in project.godot manually
    // add_autoload_singleton("MeowMCPBridge",
    //     "res://addons/meow_godot_mcp/companion/meow_mcp_bridge.gd");

    // Connect game bridge to server for deferred responses
    server->set_game_bridge(debugger_plugin.ptr());

    // Create dock panel
    dock = new MCPDock();

    // Connect button signals TO MCPPlugin (which IS a Godot Object)
    dock->get_toggle_button()->connect("pressed",
        callable_mp(this, &MCPPlugin::_on_toggle_pressed));
    dock->get_restart_button()->connect("pressed",
        callable_mp(this, &MCPPlugin::_on_restart_pressed));
    dock->get_configure_mcp_button()->connect("pressed",
        callable_mp(this, &MCPPlugin::_on_configure_mcp_pressed));
    dock->get_port_spinbox()->connect("value_changed",
        callable_mp(this, &MCPPlugin::_on_port_changed));

    // Initialize spinbox with configured port
    dock->get_port_spinbox()->set_value_no_signal(port);

    // Add dock to editor (right-bottom panel)
    add_control_to_dock(DOCK_SLOT_RIGHT_BL, dock->get_root());

    // Build tool category checkboxes
    dock->build_tool_checkboxes();
    dock->set_tool_toggle_callback([this](const std::string& tool_name, bool enabled) {
        // Update tool count display when tools are toggled
        tool_count = get_tool_count(detected_version);
        if (dock) {
            bool running = server && server->is_running();
            bool connected = server && server->has_client();
            dock->update_status(running, connected, port, version_string, tool_count);
        }
    });

    // Connect checkbox toggled signals
    for (auto* cb : dock->get_tool_checkboxes()) {
        cb->connect("toggled", callable_mp(this, &MCPPlugin::_on_tool_toggled));
    }

    // Initial dock state
    dock->update_status(true, false, port, version_string, tool_count);
    dock->update_buttons(true);

    // Check if game bridge autoload is configured
    bool autoload_missing = true;
    auto* ps = ProjectSettings::get_singleton();
    if (ps && (ps->has_setting("autoload/MeowMCPBridge") || ps->has_setting("autoload/MeowMcpBridge"))) {
        autoload_missing = false;
    }
    dock->set_autoload_warning(autoload_missing);

    set_process(true);

    UtilityFunctions::print(String::utf8("MCP Meow: 服务已启动，端口 "),
                            port,
                            " (Godot ", version_string.c_str(),
                            String::utf8(", 工具数 "), tool_count, ")");
}

void MCPPlugin::_exit_tree() {
    // Remove companion autoload
    // DISABLED: matching add_autoload_singleton is also disabled
    // remove_autoload_singleton("MeowMCPBridge");

    // Remove debugger plugin
    if (debugger_plugin.is_valid()) {
        remove_debugger_plugin(debugger_plugin);
        debugger_plugin.unref();
    }

    // Dock cleanup (order matters: remove from docks, then memdelete root, then delete wrapper)
    if (dock) {
        remove_control_from_docks(dock->get_root());
        memdelete(dock->get_root());
        delete dock;
        dock = nullptr;
    }

    // Server cleanup
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }

    UtilityFunctions::print(String::utf8("MCP Meow: 服务已停止"));
}

void MCPPlugin::_process(double delta) {
    // Poll server for pending requests (existing behavior)
    if (server && server->is_running()) {
        server->poll();
    }

    // Tick feedback auto-hide timer (hides on timeout or MCP client connect)
    if (dock) {
        bool connected = server && server->has_client();
        dock->tick_feedback(delta, connected);
    }

    // Status polling timer: update dock every ~1 second
    status_timer += delta;
    if (status_timer >= 1.0 && dock) {
        status_timer = 0.0;
        bool running = server && server->is_running();
        bool connected = server && server->has_client();
        int current_port = running ? port : 0;
        dock->update_status(running, connected, current_port, version_string, tool_count);
        dock->update_buttons(running);

        // Re-check autoload status (user may add/remove it at any time)
        auto* ps = ProjectSettings::get_singleton();
        bool autoload_missing = !ps || (!ps->has_setting("autoload/MeowMCPBridge") && !ps->has_setting("autoload/MeowMcpBridge"));
        dock->set_autoload_warning(autoload_missing);
    }
}

void MCPPlugin::_on_toggle_pressed() {
    if (!server) return;

    if (server->is_running()) {
        server->stop();
    } else {
        // Re-read port from settings in case user changed it
        auto* ps = ProjectSettings::get_singleton();
        if (ps && ps->has_setting("meow_mcp/server/port")) {
            port = ps->get_setting("meow_mcp/server/port");
        }
        int actual_port = 0;
        for (int i = 0; i < 10; i++) {
            actual_port = server->start(port + i);
            if (actual_port > 0) { port = actual_port; break; }
        }
    }

    // Immediately update dock state
    if (dock) {
        bool running = server->is_running();
        bool connected = server->has_client();
        dock->update_status(running, connected, running ? port : 0, version_string, tool_count);
        dock->update_buttons(running);
    }
}

void MCPPlugin::_on_restart_pressed() {
    if (!server) return;

    server->stop();

    // Re-read port from settings in case user changed it
    auto* ps = ProjectSettings::get_singleton();
    if (ps && ps->has_setting("meow_mcp/server/port")) {
        port = ps->get_setting("meow_mcp/server/port");
    }
    int actual_port = 0;
    for (int i = 0; i < 10; i++) {
        actual_port = server->start(port + i);
        if (actual_port > 0) { port = actual_port; break; }
    }

    // Immediately update dock state
    if (dock) {
        bool running = server->is_running();
        bool connected = server->has_client();
        dock->update_status(running, connected, running ? port : 0, version_string, tool_count);
        dock->update_buttons(running);
    }
}

void MCPPlugin::_on_port_changed(double new_port) {
    int new_port_int = static_cast<int>(new_port);

    // Save to ProjectSettings
    auto* ps = ProjectSettings::get_singleton();
    if (ps) {
        ps->set_setting("meow_mcp/server/port", new_port_int);
    }

    // Restart server on the new port
    if (server && server->is_running()) {
        server->stop();
        int actual_port = 0;
        for (int i = 0; i < 10; i++) {
            actual_port = server->start(new_port_int + i);
            if (actual_port > 0) { port = actual_port; break; }
        }
        if (actual_port == 0) {
            UtilityFunctions::printerr("MCP Meow: Failed to restart on port ", new_port_int);
        }
    } else {
        port = new_port_int;
    }

    // Update dock
    if (dock) {
        bool running = server && server->is_running();
        bool connected = server && server->has_client();
        dock->update_status(running, connected, running ? port : 0, version_string, tool_count);
    }
}

void MCPPlugin::_on_configure_mcp_pressed() {
    auto* ps = ProjectSettings::get_singleton();
    if (!ps) return;

    // Get absolute path to bridge executable
    String bridge_res = "res://addons/meow_godot_mcp/bin/godot-mcp-bridge";
#ifdef _WIN32
    bridge_res += ".exe";
#endif
    String bridge_abs = ps->globalize_path(bridge_res);
    // Normalize to forward slashes
    bridge_abs = bridge_abs.replace("\\", "/");

    // Build the claude mcp add command (include --port for multi-instance support)
    String command = String("claude mcp add --transport stdio --scope project godot -- \"")
        + bridge_abs + String("\" --port ") + String::num_int64(port);

    // Copy to clipboard
    DisplayServer::get_singleton()->clipboard_set(command);

    // Show feedback in dock panel
    if (dock) {
        dock->show_configure_feedback(command);
    }

    UtilityFunctions::print(String::utf8("MCP Meow: 配置命令已复制到剪贴板"));
}

void MCPPlugin::_on_tool_toggled(bool pressed) {
    // Find which checkbox was toggled (the sender)
    // In Godot 4.x, we don't have get_sender() in C++, so we iterate checkboxes
    if (!dock) return;
    for (auto* cb : dock->get_tool_checkboxes()) {
        if (cb->has_meta("tool_name")) {
            bool is_pressed = cb->is_pressed();
            String tool_name_str = cb->get_meta("tool_name");
            std::string tool_name = std::string(tool_name_str.utf8().get_data());
            set_tool_disabled(tool_name, !is_pressed);
        }
    }
    // Update tool count
    tool_count = get_tool_count(detected_version);
    if (dock) {
        bool running = server && server->is_running();
        bool connected = server && server->has_client();
        dock->update_status(running, connected, port, version_string, tool_count);
    }
}

void MCPPlugin::_bind_methods() {
    // No exposed properties yet
}
