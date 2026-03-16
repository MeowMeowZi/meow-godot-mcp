#include "scene_tools.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

nlohmann::json serialize_node(Node* node, int current_depth, int max_depth, bool include_properties) {
    nlohmann::json result;

    // Core fields (always present)
    // StringName and NodePath need explicit conversion to String before .utf8()
    result["name"] = std::string(String(node->get_name()).utf8().get_data());
    result["type"] = std::string(String(node->get_class()).utf8().get_data());
    result["path"] = std::string(String(node->get_path()).utf8().get_data());

    // Extended properties (optional)
    if (include_properties) {
        // Transform: Node2D and Node3D have different transform APIs
        Node2D* node2d = Object::cast_to<Node2D>(node);
        Node3D* node3d = Object::cast_to<Node3D>(node);

        if (node2d) {
            Vector2 pos = node2d->get_position();
            double rot = node2d->get_rotation();
            Vector2 scl = node2d->get_scale();
            result["transform"] = {
                {"position", {{"x", pos.x}, {"y", pos.y}}},
                {"rotation", rot},
                {"scale", {{"x", scl.x}, {"y", scl.y}}}
            };
        } else if (node3d) {
            Vector3 pos = node3d->get_position();
            Vector3 rot = node3d->get_rotation();
            Vector3 scl = node3d->get_scale();
            result["transform"] = {
                {"position", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}},
                {"rotation", {{"x", rot.x}, {"y", rot.y}, {"z", rot.z}}},
                {"scale", {{"x", scl.x}, {"y", scl.y}, {"z", scl.z}}}
            };
        }
        // For other node types: omit transform field

        // Visibility: CanvasItem (2D) and Node3D
        CanvasItem* canvas_item = Object::cast_to<CanvasItem>(node);
        if (canvas_item) {
            result["visible"] = canvas_item->is_visible();
        } else if (node3d) {
            result["visible"] = node3d->is_visible();
        }
        // For other node types: omit visible field

        // Script info
        Variant script_var = node->get_script();
        if (script_var.get_type() != Variant::NIL) {
            Ref<Script> script = script_var;
            result["has_script"] = true;
            if (script.is_valid()) {
                result["script_path"] = std::string(script->get_path().utf8().get_data());
            }
        } else {
            result["has_script"] = false;
        }
    }

    // Children: recursive traversal with depth limiting
    int child_count = node->get_child_count();
    if (max_depth == -1 || current_depth < max_depth) {
        nlohmann::json children = nlohmann::json::array();
        for (int i = 0; i < child_count; i++) {
            Node* child = node->get_child(i);
            if (child) {
                children.push_back(serialize_node(child, current_depth + 1, max_depth, include_properties));
            }
        }
        result["children"] = children;
    } else if (child_count > 0) {
        // Depth limit reached but children exist
        result["children_truncated"] = true;
        result["child_count"] = child_count;
    } else {
        result["children"] = nlohmann::json::array();
    }

    return result;
}

nlohmann::json get_scene_tree(int max_depth, bool include_properties, const std::string& root_path) {
    Node* root = EditorInterface::get_singleton()->get_edited_scene_root();

    if (root == nullptr) {
        // No scene currently open -- return success with null tree (not an error)
        return {
            {"tree", nullptr},
            {"message", "No scene currently open"}
        };
    }

    Node* start_node = root;

    // If root_path specified, find that node
    if (!root_path.empty()) {
        NodePath path(root_path.c_str());
        start_node = root->get_node_or_null(path);
        if (start_node == nullptr) {
            return {
                {"error", "Node not found at path: " + root_path}
            };
        }
    }

    return serialize_node(start_node, 0, max_depth, include_properties);
}
