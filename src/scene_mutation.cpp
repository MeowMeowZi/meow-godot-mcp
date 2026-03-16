// GODOT_MCP_MEOW_GODOT_ENABLED is defined by the SCons build system
// to enable Godot-dependent code paths (e.g., parse_variant in variant_parser.h)
#include "scene_mutation.h"
#include "variant_parser.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

nlohmann::json create_node(const std::string& type, const std::string& parent_path,
                            const std::string& name, const nlohmann::json& properties,
                            EditorUndoRedoManager* undo_redo) {
    // Get the currently edited scene root
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Validate class exists
    if (!ClassDB::class_exists(StringName(type.c_str()))) {
        return {{"error", "Unknown class: " + type}};
    }

    // Validate it is a Node subclass
    if (!ClassDB::is_parent_class(StringName(type.c_str()), StringName("Node"))) {
        return {{"error", type + " is not a Node type"}};
    }

    // Find parent node
    Node* parent = scene_root;
    if (!parent_path.empty()) {
        parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
        if (!parent) {
            return {{"error", "Parent not found: " + parent_path}};
        }
    }

    // Instantiate the node via ClassDB
    Variant instance = ClassDB::instantiate(StringName(type.c_str()));
    Node* new_node = Object::cast_to<Node>(instance.operator Object*());
    if (!new_node) {
        return {{"error", "Failed to instantiate: " + type}};
    }

    // Set name (use type as default if name not provided)
    if (!name.empty()) {
        new_node->set_name(String(name.c_str()));
    } else {
        new_node->set_name(String(type.c_str()));
    }

    // Build UndoRedo action
    undo_redo->create_action(String("MCP: Create ") + String(type.c_str()));
    undo_redo->add_do_method(parent, "add_child", new_node, true);
    undo_redo->add_do_method(new_node, "set_owner", scene_root);
    undo_redo->add_do_reference(new_node);
    undo_redo->add_undo_method(parent, "remove_child", new_node);

    // Set initial properties within the same UndoRedo action
    // Properties are set AFTER add_child/set_owner because some properties
    // may only work correctly on nodes that are in the tree.
    if (properties.is_object() && !properties.empty()) {
        for (auto& [key, val] : properties.items()) {
            std::string val_str;
            if (val.is_string()) {
                val_str = val.get<std::string>();
            } else {
                val_str = val.dump();
            }
            Variant parsed_val = parse_variant(val_str, new_node, key);
            Variant old_val = new_node->get(StringName(key.c_str()));
            undo_redo->add_do_property(new_node, StringName(key.c_str()), parsed_val);
            undo_redo->add_undo_property(new_node, StringName(key.c_str()), old_val);
        }
    }

    undo_redo->commit_action();

    // Return the actual path (Godot may have auto-renamed for uniqueness)
    std::string actual_path = std::string(String(scene_root->get_path_to(new_node)).utf8().get_data());
    return {{"success", true}, {"path", actual_path}, {"type", type}};
}

nlohmann::json set_node_property(const std::string& node_path, const std::string& property,
                                  const std::string& value_str,
                                  EditorUndoRedoManager* undo_redo) {
    // Get the currently edited scene root
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    // Find the target node
    Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    if (!node) {
        return {{"error", "Node not found: " + node_path}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Parse the new value using variant_parser
    Variant new_value = parse_variant(value_str, node, property);

    // Get old value for undo
    Variant old_value = node->get(StringName(property.c_str()));

    // Build UndoRedo action
    undo_redo->create_action(String("MCP: Set ") + String(property.c_str()));
    undo_redo->add_do_property(node, StringName(property.c_str()), new_value);
    undo_redo->add_undo_property(node, StringName(property.c_str()), old_value);
    undo_redo->commit_action();

    return {{"success", true}};
}

nlohmann::json delete_node(const std::string& node_path,
                            EditorUndoRedoManager* undo_redo) {
    // Get the currently edited scene root
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    // Find the target node
    Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    if (!node) {
        return {{"error", "Node not found: " + node_path}};
    }

    // Cannot delete scene root
    if (node == scene_root) {
        return {{"error", "Cannot delete scene root"}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Get parent for undo restoration
    Node* parent = node->get_parent();

    // Build UndoRedo action
    undo_redo->create_action(String("MCP: Delete ") + String(node->get_name()));
    undo_redo->add_do_method(parent, "remove_child", node);
    undo_redo->add_undo_method(parent, "add_child", node, true);
    undo_redo->add_undo_method(node, "set_owner", scene_root);
    undo_redo->add_undo_reference(node);
    undo_redo->commit_action();

    return {{"success", true}};
}
