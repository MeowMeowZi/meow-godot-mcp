#ifndef MEOW_GODOT_MCP_REGISTER_TYPES_H
#define MEOW_GODOT_MCP_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_mcp_module(ModuleInitializationLevel p_level);
void uninitialize_mcp_module(ModuleInitializationLevel p_level);

#endif // MEOW_GODOT_MCP_REGISTER_TYPES_H
