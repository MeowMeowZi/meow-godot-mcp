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

#endif // MEOW_GODOT_MCP_GODOT_ENABLED

#endif // MEOW_GODOT_MCP_COMPOSITE_TOOLS_H
