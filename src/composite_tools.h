#ifndef MEOW_GODOT_MCP_COMPOSITE_TOOLS_H
#define MEOW_GODOT_MCP_COMPOSITE_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

// Pure C++ function (testable without Godot)
// Glob-style matching supporting * wildcards. Case-insensitive.
// If pattern has no *, do substring match. Empty pattern matches all.
bool find_nodes_match_name(const std::string& pattern, const std::string& name);

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

namespace godot {
class EditorUndoRedoManager;
}

// Search the scene tree for nodes matching criteria (type, name pattern, property value)
// All filter params are optional; omitted/empty means "don't filter on this"
// Returns {"nodes": [...], "count": N} where each entry has "path" and "type"
nlohmann::json find_nodes(const std::string& type, const std::string& name_pattern,
                           const std::string& property_name, const std::string& property_value,
                           const std::string& root_path);

// Set a property on multiple nodes in one atomic operation
// Two modes: explicit node_paths array, or type_filter to match nodes by class
// Wraps all changes in a single UndoRedo action (Ctrl+Z undoes all)
// Returns {"modified": N, "errors": [...]}
nlohmann::json batch_set_property(const nlohmann::json& node_paths, const std::string& type_filter,
                                   const std::string& property_name, const std::string& value_str,
                                   godot::EditorUndoRedoManager* undo_redo);

// COMP-03: Create complete character (CharacterBody + CollisionShape + optional Sprite + optional script)
// Single UndoRedo action for entire hierarchy. Ctrl+Z undoes everything.
nlohmann::json create_character(const std::string& name, const std::string& char_type,
                                 const std::string& shape_type, const std::string& parent_path,
                                 const std::string& sprite_texture,
                                 const std::string& script_template,
                                 godot::EditorUndoRedoManager* undo_redo);

// COMP-04: Create UI panel from declarative JSON spec
// Creates container + children + StyleBoxFlat styling in single UndoRedo action.
nlohmann::json create_ui_panel(const nlohmann::json& spec, const std::string& parent_path,
                                godot::EditorUndoRedoManager* undo_redo);

// COMP-05: Deep-copy node subtree to new parent
// Uses Godot's Node::duplicate() for deep copy, sets owner recursively.
nlohmann::json duplicate_node(const std::string& source_path, const std::string& target_parent_path,
                               const std::string& new_name,
                               godot::EditorUndoRedoManager* undo_redo);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED

#endif // MEOW_GODOT_MCP_COMPOSITE_TOOLS_H
