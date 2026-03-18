#include "scene_file_tools.h"
#include "script_tools.h"  // For validate_res_path

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

// SCNF-01 + SCNF-05: Save current scene to disk
nlohmann::json save_scene(const std::string& path) {
    EditorInterface* ei = EditorInterface::get_singleton();
    Node* root = ei->get_edited_scene_root();
    if (!root) {
        return {{"error", "No scene open"}};
    }

    if (path.empty()) {
        // Overwrite mode -- save to existing file path
        String scene_path = root->get_scene_file_path();
        if (scene_path.is_empty()) {
            return {{"error", "Scene has no file path. Provide a path parameter to save as new file."}};
        }
        Error err = ei->save_scene();
        if (err != OK) {
            return {{"error", "Failed to save scene (error: " + std::to_string((int)err) + ")"}};
        }
        return {{"success", true}, {"path", std::string(scene_path.utf8().get_data())}};
    }

    // Save-as mode -- save to new location
    std::string error_out;
    if (!validate_res_path(path, error_out)) {
        return {{"error", error_out}};
    }

    ei->save_scene_as(String(path.c_str()), true);

    // Verify file exists after save (save_scene_as returns void, cannot detect errors)
    if (!FileAccess::file_exists(String(path.c_str()))) {
        return {{"error", "Failed to save scene to: " + path}};
    }

    return {{"success", true}, {"path", path}};
}

// SCNF-02: Open an existing scene file in editor
nlohmann::json open_scene(const std::string& path) {
    std::string error_out;
    if (!validate_res_path(path, error_out)) {
        return {{"error", error_out}};
    }

    if (!FileAccess::file_exists(String(path.c_str()))) {
        return {{"error", "Scene file not found: " + path}};
    }

    EditorInterface::get_singleton()->open_scene_from_path(String(path.c_str()));
    return {{"success", true}, {"path", path}};
}

// SCNF-03: List all currently open scenes
nlohmann::json list_open_scenes() {
    EditorInterface* ei = EditorInterface::get_singleton();
    PackedStringArray scenes = ei->get_open_scenes();
    Node* active_root = ei->get_edited_scene_root();
    String active_path = active_root ? active_root->get_scene_file_path() : String();

    nlohmann::json result = nlohmann::json::array();
    for (int i = 0; i < scenes.size(); i++) {
        String scene_path = scenes[i];
        std::string path_str(scene_path.utf8().get_data());

        // Extract title (filename) from path
        std::string title;
        size_t slash_pos = path_str.rfind('/');
        if (slash_pos != std::string::npos) {
            title = path_str.substr(slash_pos + 1);
        } else {
            title = path_str;
        }

        bool is_active = (scene_path == active_path);
        result.push_back({
            {"path", path_str},
            {"title", title},
            {"is_active", is_active}
        });
    }

    return {{"success", true}, {"scenes", result}, {"count", (int)result.size()}};
}

// SCNF-04: Create a new scene with specified root node type
nlohmann::json create_scene(const std::string& root_type,
                             const std::string& path,
                             const std::string& root_name) {
    // Validate class exists
    if (!ClassDB::class_exists(StringName(root_type.c_str()))) {
        return {{"error", "Unknown class: " + root_type}};
    }

    // Validate it is a Node subclass
    if (!ClassDB::is_parent_class(StringName(root_type.c_str()), StringName("Node"))) {
        return {{"error", root_type + " is not a Node type"}};
    }

    // Validate path
    std::string error_out;
    if (!validate_res_path(path, error_out)) {
        return {{"error", error_out}};
    }

    // Instantiate root node via ClassDB
    Variant instance = ClassDB::instantiate(StringName(root_type.c_str()));
    Node* root = Object::cast_to<Node>(instance.operator Object*());
    if (!root) {
        return {{"error", "Failed to instantiate: " + root_type}};
    }

    // Set name
    root->set_name(String(root_name.empty() ? root_type.c_str() : root_name.c_str()));

    // Pack into PackedScene
    Ref<PackedScene> packed;
    packed.instantiate();
    Error pack_err = packed->pack(root);

    // CRITICAL: Free temporary node with memdelete (NOT queue_free -- node is not in scene tree)
    memdelete(root);

    if (pack_err != OK) {
        return {{"error", "Failed to pack scene (error: " + std::to_string((int)pack_err) + ")"}};
    }

    // Save to disk
    Error save_err = ResourceSaver::get_singleton()->save(packed, String(path.c_str()));
    if (save_err != OK) {
        return {{"error", "Failed to save scene file (error: " + std::to_string((int)save_err) + ")"}};
    }

    // Open in editor
    EditorInterface::get_singleton()->open_scene_from_path(String(path.c_str()));

    return {{"success", true}, {"path", path}, {"root_type", root_type}};
}

// SCNF-06: Instantiate a PackedScene as a child node with undo/redo
nlohmann::json instantiate_scene(const std::string& scene_path,
                                  const std::string& parent_path,
                                  const std::string& name,
                                  EditorUndoRedoManager* undo_redo) {
    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Get scene root
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    // Validate scene_path
    std::string error_out;
    if (!validate_res_path(scene_path, error_out)) {
        return {{"error", error_out}};
    }

    // Check file exists
    if (!FileAccess::file_exists(String(scene_path.c_str()))) {
        return {{"error", "Scene file not found: " + scene_path}};
    }

    // Load PackedScene
    Ref<Resource> res = ResourceLoader::get_singleton()->load(String(scene_path.c_str()), "PackedScene");
    Ref<PackedScene> packed = res;
    if (!packed.is_valid()) {
        return {{"error", "Failed to load scene: " + scene_path}};
    }

    // Instantiate with editor state
    Node* instance = packed->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
    if (!instance) {
        return {{"error", "Failed to instantiate scene: " + scene_path}};
    }

    // Optional name override
    if (!name.empty()) {
        instance->set_name(String(name.c_str()));
    }

    // Find parent node
    Node* parent = scene_root;
    if (!parent_path.empty()) {
        parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
        if (!parent) {
            memdelete(instance);
            return {{"error", "Parent not found: " + parent_path}};
        }
    }

    // UndoRedo (same pattern as create_node in scene_mutation.cpp)
    undo_redo->create_action(String("MCP: Instantiate scene"));
    undo_redo->add_do_method(parent, "add_child", instance, true);
    undo_redo->add_do_method(instance, "set_owner", scene_root);
    undo_redo->add_do_reference(instance);
    undo_redo->add_undo_method(parent, "remove_child", instance);
    undo_redo->commit_action();

    // Build node path using manual path construction (avoids get_path_to null errors)
    std::string node_name = std::string(String(instance->get_name()).utf8().get_data());
    std::string actual_path;
    if (parent == scene_root) {
        actual_path = node_name;
    } else {
        actual_path = parent_path + "/" + node_name;
    }

    return {{"success", true}, {"path", actual_path}, {"scene_path", scene_path}};
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
