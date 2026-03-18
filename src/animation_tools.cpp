#include "animation_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include "variant_parser.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/core/object.hpp>

#include <unordered_map>

using namespace godot;

// ============================================================================
// Helpers
// ============================================================================

// --- Helper 1: AnimationPlayer lookup ---

struct AnimPlayerLookupResult {
    bool success;
    nlohmann::json error;
    AnimationPlayer* player;
};

static AnimPlayerLookupResult lookup_animation_player(const std::string& player_path) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {false, {{"error", "No scene open"}}, nullptr};
    }
    Node* node = scene_root->get_node_or_null(NodePath(player_path.c_str()));
    if (!node) {
        return {false, {{"error", "Node not found: " + player_path}}, nullptr};
    }
    AnimationPlayer* player = Object::cast_to<AnimationPlayer>(node);
    if (!player) {
        return {false, {{"error", "Node is not an AnimationPlayer: " + player_path}}, nullptr};
    }
    return {true, {}, player};
}

// --- Helper 2: Find animation by name ---

static Ref<Animation> find_animation(AnimationPlayer* player, const std::string& anim_name) {
    StringName sn(anim_name.c_str());
    if (!player->has_animation(sn)) {
        return Ref<Animation>();
    }
    return player->get_animation(sn);
}

// --- Helper 3: Track type string <-> enum mapping ---

static const std::unordered_map<std::string, Animation::TrackType> track_type_map = {
    {"value",       Animation::TYPE_VALUE},
    {"position_3d", Animation::TYPE_POSITION_3D},
    {"rotation_3d", Animation::TYPE_ROTATION_3D},
    {"scale_3d",    Animation::TYPE_SCALE_3D},
};

// --- Helper 4: Track type enum -> string ---

static std::string track_type_to_string(Animation::TrackType type) {
    switch (type) {
        case Animation::TYPE_VALUE:       return "value";
        case Animation::TYPE_POSITION_3D: return "position_3d";
        case Animation::TYPE_ROTATION_3D: return "rotation_3d";
        case Animation::TYPE_SCALE_3D:    return "scale_3d";
        default:                          return "unknown";
    }
}

// --- Helper 5: Loop mode string <-> enum ---

static int loop_mode_from_string(const std::string& mode) {
    if (mode == "none")     return Animation::LOOP_NONE;
    if (mode == "linear")   return Animation::LOOP_LINEAR;
    if (mode == "pingpong") return Animation::LOOP_PINGPONG;
    return -1;
}

static std::string loop_mode_to_string(Animation::LoopMode mode) {
    switch (mode) {
        case Animation::LOOP_NONE:     return "none";
        case Animation::LOOP_LINEAR:   return "linear";
        case Animation::LOOP_PINGPONG: return "pingpong";
        default:                       return "unknown";
    }
}

// --- Helper 6: Variant to string for query output ---

static std::string variant_to_string(const Variant& val) {
    String godot_str = UtilityFunctions::var_to_str(val);
    return std::string(godot_str.utf8().get_data());
}

// ============================================================================
// 1. create_animation (ANIM-01)
// ============================================================================

nlohmann::json create_animation(const std::string& animation_name,
                                 const std::string& player_path,
                                 const std::string& parent_path,
                                 const std::string& node_name,
                                 EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }
    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    AnimationPlayer* player = nullptr;

    if (!player_path.empty()) {
        // Use existing AnimationPlayer
        Node* node = scene_root->get_node_or_null(NodePath(player_path.c_str()));
        if (!node) {
            return {{"error", "Node not found: " + player_path}};
        }
        player = Object::cast_to<AnimationPlayer>(node);
        if (!player) {
            return {{"error", "Node is not an AnimationPlayer: " + player_path}};
        }
    } else {
        // Create new AnimationPlayer node (follows scene_mutation.cpp create_node pattern)
        Variant instance = ClassDB::instantiate(StringName("AnimationPlayer"));
        player = Object::cast_to<AnimationPlayer>(instance.operator Object*());
        if (!player) {
            return {{"error", "Failed to instantiate AnimationPlayer"}};
        }

        // Set name
        player->set_name(String(node_name.empty() ? "AnimationPlayer" : node_name.c_str()));

        // Find parent
        Node* parent = scene_root;
        if (!parent_path.empty()) {
            parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
            if (!parent) {
                memdelete(player);
                return {{"error", "Parent not found: " + parent_path}};
            }
        }

        // UndoRedo: add node to scene tree
        undo_redo->create_action(String("MCP: Create AnimationPlayer"));
        undo_redo->add_do_method(parent, "add_child", player, true);
        undo_redo->add_do_method(player, "set_owner", scene_root);
        undo_redo->add_do_reference(player);
        undo_redo->add_undo_method(parent, "remove_child", player);
        undo_redo->commit_action();
    }

    // Ensure default library exists
    StringName lib_name("");
    if (!player->has_animation_library(lib_name)) {
        Ref<AnimationLibrary> library;
        library.instantiate();
        player->add_animation_library(lib_name, library);
    }

    // Check if animation already exists
    Ref<AnimationLibrary> lib = player->get_animation_library(lib_name);
    StringName anim_sn(animation_name.c_str());
    if (lib->has_animation(anim_sn)) {
        return {{"error", "Animation already exists: " + animation_name}};
    }

    // Create animation
    Ref<Animation> anim;
    anim.instantiate();
    anim->set_length(1.0);  // 1 second default
    lib->add_animation(anim_sn, anim);

    // Compute player path relative to scene root
    String computed_path = scene_root->get_path_to(player);
    std::string result_path(computed_path.utf8().get_data());

    return {{"success", true},
            {"player_path", result_path},
            {"animation_name", animation_name}};
}

// ============================================================================
// 2. add_animation_track (ANIM-02)
// ============================================================================

nlohmann::json add_animation_track(const std::string& player_path,
                                    const std::string& animation_name,
                                    const std::string& track_type,
                                    const std::string& track_path) {
    auto lookup = lookup_animation_player(player_path);
    if (!lookup.success) return lookup.error;

    Ref<Animation> anim = find_animation(lookup.player, animation_name);
    if (!anim.is_valid()) {
        return {{"error", "Animation not found: " + animation_name}};
    }

    auto it = track_type_map.find(track_type);
    if (it == track_type_map.end()) {
        return {{"error", "Unknown track type: " + track_type +
                          ". Valid: value, position_3d, rotation_3d, scale_3d"}};
    }

    int track_idx = anim->add_track(it->second);
    anim->track_set_path(track_idx, NodePath(track_path.c_str()));

    return {{"success", true},
            {"track_index", track_idx},
            {"track_type", track_type},
            {"track_path", track_path}};
}

// ============================================================================
// 3. set_keyframe (ANIM-03)
// ============================================================================

nlohmann::json set_keyframe(const std::string& player_path,
                             const std::string& animation_name,
                             int track_index,
                             double time,
                             const std::string& action,
                             const std::string& value,
                             double transition) {
    auto lookup = lookup_animation_player(player_path);
    if (!lookup.success) return lookup.error;

    Ref<Animation> anim = find_animation(lookup.player, animation_name);
    if (!anim.is_valid()) {
        return {{"error", "Animation not found: " + animation_name}};
    }

    // Validate track index bounds
    if (track_index < 0 || track_index >= anim->get_track_count()) {
        return {{"error", "Track index out of range: " + std::to_string(track_index) +
                          " (track count: " + std::to_string(anim->get_track_count()) + ")"}};
    }

    Animation::TrackType type = anim->track_get_type(track_index);

    if (action == "insert") {
        if (value.empty()) {
            return {{"error", "Missing required parameter: value (for insert action)"}};
        }
        Variant parsed_value = parse_variant(value, nullptr, "");

        switch (type) {
            case Animation::TYPE_VALUE:
                anim->track_insert_key(track_index, time, parsed_value, transition);
                break;
            case Animation::TYPE_POSITION_3D:
                anim->position_track_insert_key(track_index, time, Vector3(parsed_value));
                break;
            case Animation::TYPE_ROTATION_3D:
                anim->rotation_track_insert_key(track_index, time, Quaternion(parsed_value));
                break;
            case Animation::TYPE_SCALE_3D:
                anim->scale_track_insert_key(track_index, time, Vector3(parsed_value));
                break;
            default:
                return {{"error", "Unsupported track type for keyframe insertion"}};
        }

        return {{"success", true},
                {"action", "insert"},
                {"track_index", track_index},
                {"time", time}};
    }

    if (action == "update") {
        if (value.empty()) {
            return {{"error", "Missing required parameter: value (for update action)"}};
        }
        Variant parsed_value = parse_variant(value, nullptr, "");

        int key_idx = anim->track_find_key(track_index, time, Animation::FIND_MODE_APPROX);
        if (key_idx == -1) {
            return {{"error", "No keyframe found at time " + std::to_string(time) +
                              " on track " + std::to_string(track_index)}};
        }

        anim->track_set_key_value(track_index, key_idx, parsed_value);

        // Update transition if not default
        if (transition != 1.0) {
            anim->track_set_key_transition(track_index, key_idx, transition);
        }

        return {{"success", true},
                {"action", "update"},
                {"track_index", track_index},
                {"time", time}};
    }

    if (action == "remove") {
        int key_idx = anim->track_find_key(track_index, time, Animation::FIND_MODE_APPROX);
        if (key_idx == -1) {
            return {{"error", "No keyframe found at time " + std::to_string(time) +
                              " on track " + std::to_string(track_index)}};
        }

        anim->track_remove_key(track_index, key_idx);

        return {{"success", true},
                {"action", "remove"},
                {"track_index", track_index},
                {"time", time}};
    }

    return {{"error", "Unknown action: " + action + ". Valid: insert, update, remove"}};
}

// ============================================================================
// 4. get_animation_info (ANIM-04)
// ============================================================================

nlohmann::json get_animation_info(const std::string& player_path,
                                   const std::string& animation_name) {
    auto lookup = lookup_animation_player(player_path);
    if (!lookup.success) return lookup.error;

    AnimationPlayer* player = lookup.player;
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    String computed_path = scene_root->get_path_to(player);
    std::string result_path(computed_path.utf8().get_data());

    nlohmann::json animations_json = nlohmann::json::array();

    if (!animation_name.empty()) {
        // Query single animation
        Ref<Animation> anim = find_animation(player, animation_name);
        if (!anim.is_valid()) {
            return {{"error", "Animation not found: " + animation_name}};
        }

        nlohmann::json anim_json;
        anim_json["name"] = animation_name;
        anim_json["length"] = anim->get_length();
        anim_json["loop_mode"] = loop_mode_to_string(anim->get_loop_mode());
        anim_json["step"] = anim->get_step();
        anim_json["track_count"] = anim->get_track_count();

        nlohmann::json tracks_json = nlohmann::json::array();
        for (int i = 0; i < anim->get_track_count(); i++) {
            nlohmann::json track_json;
            track_json["index"] = i;
            track_json["type"] = track_type_to_string(anim->track_get_type(i));
            track_json["path"] = std::string(String(anim->track_get_path(i)).utf8().get_data());
            track_json["key_count"] = anim->track_get_key_count(i);

            nlohmann::json keys_json = nlohmann::json::array();
            for (int k = 0; k < anim->track_get_key_count(i); k++) {
                nlohmann::json key_json;
                key_json["time"] = anim->track_get_key_time(i, k);
                key_json["value"] = variant_to_string(anim->track_get_key_value(i, k));
                key_json["transition"] = anim->track_get_key_transition(i, k);
                keys_json.push_back(key_json);
            }
            track_json["keys"] = keys_json;
            tracks_json.push_back(track_json);
        }
        anim_json["tracks"] = tracks_json;
        animations_json.push_back(anim_json);
    } else {
        // Query all animations
        PackedStringArray anim_list = player->get_animation_list();
        for (int a = 0; a < anim_list.size(); a++) {
            String anim_name_godot = anim_list[a];
            std::string name_str(anim_name_godot.utf8().get_data());

            Ref<Animation> anim = player->get_animation(StringName(anim_name_godot));
            if (!anim.is_valid()) continue;

            nlohmann::json anim_json;
            anim_json["name"] = name_str;
            anim_json["length"] = anim->get_length();
            anim_json["loop_mode"] = loop_mode_to_string(anim->get_loop_mode());
            anim_json["step"] = anim->get_step();
            anim_json["track_count"] = anim->get_track_count();

            nlohmann::json tracks_json = nlohmann::json::array();
            for (int i = 0; i < anim->get_track_count(); i++) {
                nlohmann::json track_json;
                track_json["index"] = i;
                track_json["type"] = track_type_to_string(anim->track_get_type(i));
                track_json["path"] = std::string(String(anim->track_get_path(i)).utf8().get_data());
                track_json["key_count"] = anim->track_get_key_count(i);

                nlohmann::json keys_json = nlohmann::json::array();
                for (int k = 0; k < anim->track_get_key_count(i); k++) {
                    nlohmann::json key_json;
                    key_json["time"] = anim->track_get_key_time(i, k);
                    key_json["value"] = variant_to_string(anim->track_get_key_value(i, k));
                    key_json["transition"] = anim->track_get_key_transition(i, k);
                    keys_json.push_back(key_json);
                }
                track_json["keys"] = keys_json;
                tracks_json.push_back(track_json);
            }
            anim_json["tracks"] = tracks_json;
            animations_json.push_back(anim_json);
        }
    }

    return {{"success", true},
            {"player_path", result_path},
            {"animations", animations_json}};
}

// ============================================================================
// 5. set_animation_properties (ANIM-05)
// ============================================================================

nlohmann::json set_animation_properties(const std::string& player_path,
                                         const std::string& animation_name,
                                         const nlohmann::json& props,
                                         EditorUndoRedoManager* undo_redo) {
    auto lookup = lookup_animation_player(player_path);
    if (!lookup.success) return lookup.error;

    Ref<Animation> anim = find_animation(lookup.player, animation_name);
    if (!anim.is_valid()) {
        return {{"error", "Animation not found: " + animation_name}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    undo_redo->create_action(String("MCP: Set Animation Properties"));

    nlohmann::json changes = nlohmann::json::object();

    // Length
    if (props.contains("length") && props["length"].is_number()) {
        double old_length = anim->get_length();
        double new_length = props["length"].get<double>();
        undo_redo->add_do_method(anim.ptr(), "set_length", new_length);
        undo_redo->add_undo_method(anim.ptr(), "set_length", old_length);
        changes["length"] = new_length;
    }

    // Loop mode
    if (props.contains("loop_mode") && props["loop_mode"].is_string()) {
        std::string mode_str = props["loop_mode"].get<std::string>();
        int new_mode = loop_mode_from_string(mode_str);
        if (new_mode == -1) {
            undo_redo->commit_action();
            return {{"error", "Unknown loop_mode: " + mode_str + ". Valid: none, linear, pingpong"}};
        }
        int old_mode = (int)anim->get_loop_mode();
        undo_redo->add_do_method(anim.ptr(), "set_loop_mode", new_mode);
        undo_redo->add_undo_method(anim.ptr(), "set_loop_mode", old_mode);
        changes["loop_mode"] = mode_str;
    }

    // Step
    if (props.contains("step") && props["step"].is_number()) {
        double old_step = anim->get_step();
        double new_step = props["step"].get<double>();
        undo_redo->add_do_method(anim.ptr(), "set_step", new_step);
        undo_redo->add_undo_method(anim.ptr(), "set_step", old_step);
        changes["step"] = new_step;
    }

    undo_redo->commit_action();

    return {{"success", true},
            {"animation_name", animation_name},
            {"changes", changes}};
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
