#ifndef MEOW_GODOT_MCP_UI_TOOLS_H
#define MEOW_GODOT_MCP_UI_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// UISYS-01: Set Control layout preset (anchors+offsets atomically)
nlohmann::json set_layout_preset(const std::string& node_path,
                                  const std::string& preset,
                                  godot::EditorUndoRedoManager* undo_redo);

// UISYS-02: Batch set theme overrides (colors, font sizes, constants)
nlohmann::json set_theme_override(const std::string& node_path,
                                   const nlohmann::json& overrides,
                                   godot::EditorUndoRedoManager* undo_redo);

// UISYS-03: Create StyleBoxFlat and auto-apply as theme override
nlohmann::json create_stylebox(const std::string& node_path,
                                const std::string& override_name,
                                const nlohmann::json& properties,
                                godot::EditorUndoRedoManager* undo_redo);

// UISYS-04: Query Control UI-specific properties
nlohmann::json get_ui_properties(const std::string& node_path);

// UISYS-05: Configure Container layout parameters + optional child size_flags
nlohmann::json set_container_layout(const std::string& node_path,
                                     const nlohmann::json& params,
                                     godot::EditorUndoRedoManager* undo_redo);

// UISYS-06 support: Query all theme overrides on a Control
nlohmann::json get_theme_overrides(const std::string& node_path);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
#endif // MEOW_GODOT_MCP_UI_TOOLS_H
