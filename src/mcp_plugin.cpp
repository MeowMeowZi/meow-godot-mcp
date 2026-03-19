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

    // Create and start server
    server = new MCPServer();
    server->set_undo_redo(get_undo_redo());
    server->set_godot_version(detected_version);
    server->start(port);

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

    // Add dock to editor (right-bottom panel)
    add_control_to_dock(DOCK_SLOT_RIGHT_BL, dock->get_root());

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
        server->start(port);
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
    server->start(port);

    // Immediately update dock state
    if (dock) {
        bool running = server->is_running();
        bool connected = server->has_client();
        dock->update_status(running, connected, running ? port : 0, version_string, tool_count);
        dock->update_buttons(running);
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
    // Normalize to forward slashes for JSON
    bridge_abs = bridge_abs.replace("\\", "/");

    // Build .mcp.json content
    String json_content = String::utf8(
        "{\n"
        "  \"mcpServers\": {\n"
        "    \"godot\": {\n"
        "      \"type\": \"stdio\",\n"
        "      \"command\": \"") + bridge_abs + String::utf8("\",\n"
        "      \"args\": []\n"
        "    }\n"
        "  }\n"
        "}\n");

    // Write .mcp.json to BOTH project root and parent directory
    // Claude Code may run from either location
    String project_root = ps->globalize_path("res://");
    // Normalize path separators
    project_root = project_root.replace("\\", "/");
    // Ensure trailing slash
    if (!project_root.ends_with("/")) project_root += "/";

    // Parent directory (repo root, where Claude Code typically runs)
    String parent_dir = project_root.substr(0, project_root.length() - 1);
    int last_slash = parent_dir.rfind("/");
    if (last_slash >= 0) {
        parent_dir = parent_dir.substr(0, last_slash + 1);
    }

    // Write to both locations
    String paths[] = { project_root + ".mcp.json", parent_dir + ".mcp.json" };
    for (int i = 0; i < 2; i++) {
        // Convert back to OS path for FileAccess
        String os_path = paths[i].replace("/", "\\");
        Ref<FileAccess> file = FileAccess::open(paths[i], FileAccess::WRITE);
        if (file.is_valid()) {
            file->store_string(json_content);
            file->close();
            UtilityFunctions::print(String::utf8("MCP Meow: 已写入 "), paths[i]);
        }
    }
    UtilityFunctions::print(String::utf8("MCP Meow: Claude Code MCP 配置完成，重启 Claude Code 生效"));
}

void MCPPlugin::_bind_methods() {
    // No exposed properties yet
}
