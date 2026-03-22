#ifndef MEOW_GODOT_MCP_PHYSICS_TOOLS_H
#define MEOW_GODOT_MCP_PHYSICS_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// Create a CollisionShape2D or CollisionShape3D with a pre-configured shape in one step
nlohmann::json create_collision_shape(const std::string& parent_path,
                                       const std::string& shape_type,
                                       const nlohmann::json& shape_params,
                                       const std::string& name,
                                       godot::EditorUndoRedoManager* undo_redo);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
#endif // MEOW_GODOT_MCP_PHYSICS_TOOLS_H
