#ifndef MEOW_GODOT_MCP_MCP_PLUGIN_H
#define MEOW_GODOT_MCP_MCP_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "mcp_tool_registry.h"

#include <string>

class MCPServer;
class MCPDock;
class MeowDebuggerPlugin;

class MCPPlugin : public godot::EditorPlugin {
    GDCLASS(MCPPlugin, godot::EditorPlugin);

public:
    MCPPlugin();
    ~MCPPlugin();

    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;

protected:
    static void _bind_methods();

private:
    // Button callbacks (signals connect to MCPPlugin since MCPDock is not a Godot Object)
    void _on_toggle_pressed();
    void _on_restart_pressed();
    void _on_configure_mcp_pressed();
    void _on_port_changed(double new_port);
    void _on_tool_toggled(bool pressed);

    MCPServer* server;
    MCPDock* dock;
    godot::Ref<MeowDebuggerPlugin> debugger_plugin;
    int port;

    // Status polling
    double status_timer;

    // Version detection
    GodotVersion detected_version;
    std::string version_string;
    int tool_count;
};

#endif // MEOW_GODOT_MCP_MCP_PLUGIN_H
