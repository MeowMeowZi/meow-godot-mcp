#ifndef MEOW_GODOT_MCP_ANIMATION_TOOLS_H
#define MEOW_GODOT_MCP_ANIMATION_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// ANIM-01: Create AnimationPlayer + AnimationLibrary + Animation
nlohmann::json create_animation(const std::string& animation_name,
                                 const std::string& player_path,
                                 const std::string& parent_path,
                                 const std::string& node_name,
                                 godot::EditorUndoRedoManager* undo_redo);

// ANIM-02: Add typed track to Animation
nlohmann::json add_animation_track(const std::string& player_path,
                                    const std::string& animation_name,
                                    const std::string& track_type,
                                    const std::string& track_path);

// ANIM-03: Insert/update/remove keyframes
nlohmann::json set_keyframe(const std::string& player_path,
                             const std::string& animation_name,
                             int track_index,
                             double time,
                             const std::string& action,
                             const std::string& value,
                             double transition);

// ANIM-04: Query animation info
nlohmann::json get_animation_info(const std::string& player_path,
                                   const std::string& animation_name);

// ANIM-05: Set animation properties (length, loop_mode, step)
nlohmann::json set_animation_properties(const std::string& player_path,
                                         const std::string& animation_name,
                                         const nlohmann::json& props,
                                         godot::EditorUndoRedoManager* undo_redo);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
#endif // MEOW_GODOT_MCP_ANIMATION_TOOLS_H
