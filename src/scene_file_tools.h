#ifndef MEOW_GODOT_MCP_SCENE_FILE_TOOLS_H
#define MEOW_GODOT_MCP_SCENE_FILE_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// SCNF-01 + SCNF-05: Save current scene to disk
// path empty = overwrite current file, path non-empty = save-as to new location
nlohmann::json save_scene(const std::string& path);

// SCNF-02: Open an existing scene file in editor (adds tab, does not close current)
nlohmann::json open_scene(const std::string& path);

// SCNF-03: List all currently open scenes with paths, titles, and active status
nlohmann::json list_open_scenes();

// SCNF-04: Create a new scene with specified root node type, save to disk, open in editor
nlohmann::json create_scene(const std::string& root_type,
                             const std::string& path,
                             const std::string& root_name);

// SCNF-06: Instantiate a PackedScene as a child node with undo/redo support
nlohmann::json instantiate_scene(const std::string& scene_path,
                                  const std::string& parent_path,
                                  const std::string& name,
                                  godot::EditorUndoRedoManager* undo_redo);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
#endif // MEOW_GODOT_MCP_SCENE_FILE_TOOLS_H
