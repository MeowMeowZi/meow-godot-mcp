#include "physics_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/capsule_shape2d.hpp>
#include <godot_cpp/classes/segment_shape2d.hpp>
#include <godot_cpp/classes/world_boundary_shape2d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <unordered_map>

using namespace godot;

// Shape type info: maps user-facing name to {Shape class name, CollisionShape class name, is_3d}
struct ShapeTypeInfo {
    const char* shape_class;
    const char* collision_class;
    bool is_3d;
};

static const std::unordered_map<std::string, ShapeTypeInfo>& get_shape_types() {
    static const std::unordered_map<std::string, ShapeTypeInfo> types = {
        // 2D shapes
        {"rectangle",       {"RectangleShape2D",    "CollisionShape2D", false}},
        {"circle",          {"CircleShape2D",       "CollisionShape2D", false}},
        {"capsule",         {"CapsuleShape2D",      "CollisionShape2D", false}},
        {"segment",         {"SegmentShape2D",      "CollisionShape2D", false}},
        {"world_boundary",  {"WorldBoundaryShape2D","CollisionShape2D", false}},
        // 3D shapes
        {"box",             {"BoxShape3D",          "CollisionShape3D", true}},
        {"sphere",          {"SphereShape3D",       "CollisionShape3D", true}},
        {"capsule_3d",      {"CapsuleShape3D",      "CollisionShape3D", true}},
        {"cylinder",        {"CylinderShape3D",     "CollisionShape3D", true}},
    };
    return types;
}

nlohmann::json create_collision_shape(const std::string& parent_path,
                                       const std::string& shape_type,
                                       const nlohmann::json& shape_params,
                                       const std::string& name,
                                       EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Look up shape type
    auto& shape_types = get_shape_types();
    auto it = shape_types.find(shape_type);
    if (it == shape_types.end()) {
        std::string valid_types;
        for (auto& [k, v] : shape_types) {
            if (!valid_types.empty()) valid_types += ", ";
            valid_types += k;
        }
        return {{"error", "Unknown shape_type: " + shape_type + ". Valid types: " + valid_types}};
    }

    auto& info = it->second;

    // Find parent node
    Node* parent = scene_root;
    if (!parent_path.empty() && parent_path != ".") {
        parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
        if (!parent) {
            return {{"error", "Parent not found: " + parent_path}};
        }
    }

    // Create collision shape node
    Variant cs_instance = ClassDB::instantiate(StringName(info.collision_class));
    Node* cs_node = Object::cast_to<Node>(cs_instance.operator Object*());
    if (!cs_node) {
        return {{"error", std::string("Failed to instantiate ") + info.collision_class}};
    }

    // Set node name
    if (!name.empty()) {
        cs_node->set_name(String(name.c_str()));
    } else {
        cs_node->set_name(String(info.collision_class));
    }

    // Create shape resource
    Variant shape_instance = ClassDB::instantiate(StringName(info.shape_class));
    Object* shape_obj = shape_instance.operator Object*();
    if (!shape_obj) {
        memdelete(cs_node);
        return {{"error", std::string("Failed to instantiate shape: ") + info.shape_class}};
    }

    // Configure shape properties based on type
    if (shape_type == "rectangle") {
        auto* rect = Object::cast_to<RectangleShape2D>(shape_obj);
        if (rect && shape_params.is_object()) {
            double w = shape_params.value("width", 20.0);
            double h = shape_params.value("height", 20.0);
            rect->set_size(Vector2(w, h));
        }
    } else if (shape_type == "circle") {
        auto* circle = Object::cast_to<CircleShape2D>(shape_obj);
        if (circle && shape_params.is_object()) {
            double r = shape_params.value("radius", 10.0);
            circle->set_radius(r);
        }
    } else if (shape_type == "capsule") {
        auto* capsule = Object::cast_to<CapsuleShape2D>(shape_obj);
        if (capsule && shape_params.is_object()) {
            double r = shape_params.value("radius", 10.0);
            double h = shape_params.value("height", 30.0);
            capsule->set_radius(r);
            capsule->set_height(h);
        }
    } else if (shape_type == "segment") {
        auto* seg = Object::cast_to<SegmentShape2D>(shape_obj);
        if (seg && shape_params.is_object()) {
            double ax = shape_params.value("ax", 0.0);
            double ay = shape_params.value("ay", 0.0);
            double bx = shape_params.value("bx", 0.0);
            double by = shape_params.value("by", 20.0);
            seg->set_a(Vector2(ax, ay));
            seg->set_b(Vector2(bx, by));
        }
    } else if (shape_type == "world_boundary") {
        auto* wb = Object::cast_to<WorldBoundaryShape2D>(shape_obj);
        if (wb && shape_params.is_object()) {
            double nx = shape_params.value("normal_x", 0.0);
            double ny = shape_params.value("normal_y", -1.0);
            double dist = shape_params.value("distance", 0.0);
            wb->set_normal(Vector2(nx, ny));
            wb->set_distance(dist);
        }
    } else if (shape_type == "box") {
        auto* box = Object::cast_to<BoxShape3D>(shape_obj);
        if (box && shape_params.is_object()) {
            double sx = shape_params.value("width", 1.0);
            double sy = shape_params.value("height", 1.0);
            double sz = shape_params.value("depth", 1.0);
            box->set_size(Vector3(sx, sy, sz));
        }
    } else if (shape_type == "sphere") {
        auto* sphere = Object::cast_to<SphereShape3D>(shape_obj);
        if (sphere && shape_params.is_object()) {
            double r = shape_params.value("radius", 0.5);
            sphere->set_radius(r);
        }
    } else if (shape_type == "capsule_3d") {
        auto* capsule = Object::cast_to<CapsuleShape3D>(shape_obj);
        if (capsule && shape_params.is_object()) {
            double r = shape_params.value("radius", 0.5);
            double h = shape_params.value("height", 2.0);
            capsule->set_radius(r);
            capsule->set_height(h);
        }
    } else if (shape_type == "cylinder") {
        auto* cyl = Object::cast_to<CylinderShape3D>(shape_obj);
        if (cyl && shape_params.is_object()) {
            double r = shape_params.value("radius", 0.5);
            double h = shape_params.value("height", 2.0);
            cyl->set_radius(r);
            cyl->set_height(h);
        }
    }

    // Wrap shape in Ref and assign to CollisionShape node
    if (info.is_3d) {
        auto* cs3d = Object::cast_to<CollisionShape3D>(cs_node);
        Ref<Shape3D> shape_ref(Object::cast_to<Shape3D>(shape_obj));
        if (cs3d && shape_ref.is_valid()) {
            cs3d->set_shape(shape_ref);
        }
    } else {
        auto* cs2d = Object::cast_to<CollisionShape2D>(cs_node);
        Ref<Shape2D> shape_ref(Object::cast_to<Shape2D>(shape_obj));
        if (cs2d && shape_ref.is_valid()) {
            cs2d->set_shape(shape_ref);
        }
    }

    // Add to tree via UndoRedo
    undo_redo->create_action(String("MCP: Create CollisionShape (") + String(shape_type.c_str()) + String(")"));
    undo_redo->add_do_method(parent, "add_child", cs_node, true);
    undo_redo->add_do_method(cs_node, "set_owner", scene_root);
    undo_redo->add_do_reference(cs_node);
    undo_redo->add_undo_method(parent, "remove_child", cs_node);
    undo_redo->commit_action();

    // Build result path
    std::string node_name_str(String(cs_node->get_name()).utf8().get_data());
    std::string actual_path;
    if (parent_path.empty() || parent_path == ".") {
        actual_path = node_name_str;
    } else {
        actual_path = parent_path + "/" + node_name_str;
    }

    return {
        {"success", true},
        {"path", actual_path},
        {"shape_type", shape_type},
        {"collision_node_type", info.collision_class},
        {"shape_class", info.shape_class}
    };
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
