#include "mcp_plugin.h"
#include "mcp_server.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

MCPPlugin::MCPPlugin()
    : server(nullptr), port(6800) {
}

MCPPlugin::~MCPPlugin() {
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
}

void MCPPlugin::_enter_tree() {
    server = new MCPServer();
    server->start(port);
    set_process(true);
    UtilityFunctions::print("MCP Meow: Server started on port ", port);
}

void MCPPlugin::_exit_tree() {
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
    UtilityFunctions::print("MCP Meow: Server stopped");
}

void MCPPlugin::_process(double delta) {
    if (server && server->is_running()) {
        server->poll();
    }
}

void MCPPlugin::_bind_methods() {
    // No exposed properties yet
}
