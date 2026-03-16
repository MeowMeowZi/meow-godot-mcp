#ifndef GODOT_MCP_MEOW_MCP_PLUGIN_H
#define GODOT_MCP_MEOW_MCP_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>

class MCPServer;

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
    MCPServer* server;
    int port;
};

#endif // GODOT_MCP_MEOW_MCP_PLUGIN_H
