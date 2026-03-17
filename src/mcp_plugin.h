#ifndef GODOT_MCP_MEOW_MCP_PLUGIN_H
#define GODOT_MCP_MEOW_MCP_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "mcp_tool_registry.h"

#include <string>

class MCPServer;
class MCPDock;

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

    MCPServer* server;
    MCPDock* dock;
    int port;

    // Status polling
    double status_timer;

    // Version detection
    GodotVersion detected_version;
    std::string version_string;
    int tool_count;
};

#endif // GODOT_MCP_MEOW_MCP_PLUGIN_H
