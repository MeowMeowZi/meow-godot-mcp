#ifndef MEOW_GODOT_MCP_SCENE_MUTATION_H
#define MEOW_GODOT_MCP_SCENE_MUTATION_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

// Create a new node in the scene tree with undo/redo support
// type: Node class name (e.g., "Sprite2D", "CharacterBody3D")
// parent_path: path to parent node (empty = scene root)
// name: desired node name (Godot may auto-rename for uniqueness)
// properties: optional key-value pairs to set after creation
// Returns JSON: {"success": true, "path": "Parent/NewNode", "type": "..."} or {"error": "..."}
nlohmann::json create_node(const std::string& type, const std::string& parent_path,
                            const std::string& name, const nlohmann::json& properties,
                            godot::EditorUndoRedoManager* undo_redo);

// Set a property on an existing node with undo/redo support
// node_path: path to target node (relative to scene root)
// property: property name in snake_case (e.g., "position", "visible", "modulate")
// value_str: property value as string (auto-parsed by variant_parser)
// Returns JSON: {"success": true} or {"error": "..."}
nlohmann::json set_node_property(const std::string& node_path, const std::string& property,
                                  const std::string& value_str,
                                  godot::EditorUndoRedoManager* undo_redo);

// Delete a node from the scene tree with undo/redo support
// node_path: path to the node to delete (relative to scene root)
// Cannot delete the scene root.
// Returns JSON: {"success": true} or {"error": "..."}
nlohmann::json delete_node(const std::string& node_path,
                            godot::EditorUndoRedoManager* undo_redo);

#endif // MEOW_GODOT_MCP_SCENE_MUTATION_H
