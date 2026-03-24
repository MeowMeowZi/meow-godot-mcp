#include "composite_tools.h"

#include <algorithm>
#include <cctype>
#include <vector>

// Helper: convert string to lowercase
static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Glob-style matching supporting * wildcards. Case-insensitive.
// If pattern has no *, do substring match. Empty pattern matches all.
bool find_nodes_match_name(const std::string& pattern, const std::string& name) {
    // Empty pattern matches everything
    if (pattern.empty()) {
        return true;
    }

    std::string p = to_lower(pattern);
    std::string n = to_lower(name);

    // No wildcards: substring match
    if (p.find('*') == std::string::npos) {
        return n.find(p) != std::string::npos;
    }

    // Split pattern on '*' into parts
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= p.size()) {
        size_t pos = p.find('*', start);
        if (pos == std::string::npos) {
            parts.push_back(p.substr(start));
            break;
        }
        parts.push_back(p.substr(start, pos - start));
        start = pos + 1;
    }

    // Check if name starts with first part (unless pattern starts with *)
    size_t name_pos = 0;
    bool starts_with_star = (p[0] == '*');
    bool ends_with_star = (p.back() == '*');

    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& part = parts[i];
        if (part.empty()) {
            continue;  // consecutive or leading/trailing *
        }

        if (i == 0 && !starts_with_star) {
            // First part must match at the beginning
            if (n.compare(0, part.size(), part) != 0) {
                return false;
            }
            name_pos = part.size();
        } else if (i == parts.size() - 1 && !ends_with_star) {
            // Last part must match at the end
            if (n.size() < part.size()) {
                return false;
            }
            size_t end_pos = n.size() - part.size();
            if (end_pos < name_pos) {
                return false;
            }
            if (n.compare(end_pos, part.size(), part) != 0) {
                return false;
            }
            name_pos = n.size();
        } else {
            // Middle or after-star part: find next occurrence
            size_t found = n.find(part, name_pos);
            if (found == std::string::npos) {
                return false;
            }
            name_pos = found + part.size();
        }
    }

    return true;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include "variant_parser.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/rectangle_shape2d.hpp>
#include <godot_cpp/classes/circle_shape2d.hpp>
#include <godot_cpp/classes/capsule_shape2d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/sprite3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

using namespace godot;

// Recursive helper for find_nodes
static void find_nodes_recursive(Node* node, Node* scene_root,
                                  const std::string& type, const std::string& name_pattern,
                                  const std::string& property_name, const std::string& property_value,
                                  nlohmann::json& results) {
    bool matches = true;

    // Type filter: check class inheritance
    if (!type.empty()) {
        StringName node_class = node->get_class();
        StringName filter_class(type.c_str());
        if (node_class != filter_class &&
            !ClassDB::is_parent_class(node_class, filter_class)) {
            matches = false;
        }
    }

    // Name pattern filter
    if (matches && !name_pattern.empty()) {
        std::string node_name(String(node->get_name()).utf8().get_data());
        if (!find_nodes_match_name(name_pattern, node_name)) {
            matches = false;
        }
    }

    // Property value filter
    if (matches && !property_name.empty()) {
        Variant val = node->get(StringName(property_name.c_str()));
        std::string val_str(String(val).utf8().get_data());
        if (!property_value.empty() && val_str != property_value) {
            matches = false;
        }
    }

    if (matches) {
        nlohmann::json entry;
        if (node == scene_root) {
            entry["path"] = std::string(String(node->get_name()).utf8().get_data());
        } else {
            entry["path"] = std::string(String(scene_root->get_path_to(node)).utf8().get_data());
        }
        entry["type"] = std::string(String(node->get_class()).utf8().get_data());
        results.push_back(entry);
    }

    // Recurse into children
    int child_count = node->get_child_count();
    for (int i = 0; i < child_count; ++i) {
        Node* child = node->get_child(i);
        if (child) {
            find_nodes_recursive(child, scene_root, type, name_pattern,
                                  property_name, property_value, results);
        }
    }
}

nlohmann::json find_nodes(const std::string& type, const std::string& name_pattern,
                           const std::string& property_name, const std::string& property_value,
                           const std::string& root_path) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    Node* start_node = scene_root;
    if (!root_path.empty()) {
        start_node = scene_root->get_node_or_null(NodePath(root_path.c_str()));
        if (!start_node) {
            return {{"error", "Node not found at path: " + root_path}};
        }
    }

    nlohmann::json results = nlohmann::json::array();
    find_nodes_recursive(start_node, scene_root, type, name_pattern,
                          property_name, property_value, results);

    return {{"nodes", results}, {"count", results.size()}};
}

nlohmann::json batch_set_property(const nlohmann::json& node_paths, const std::string& type_filter,
                                   const std::string& property_name, const std::string& value_str,
                                   EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Collect target nodes
    std::vector<Node*> targets;
    nlohmann::json errors = nlohmann::json::array();

    if (node_paths.is_array() && !node_paths.empty()) {
        // Mode 1: explicit node paths
        for (const auto& path_val : node_paths) {
            if (!path_val.is_string()) continue;
            std::string path = path_val.get<std::string>();
            Node* node = scene_root->get_node_or_null(NodePath(path.c_str()));
            if (node) {
                targets.push_back(node);
            } else {
                errors.push_back({{"path", path}, {"error", "Node not found"}});
            }
        }
    } else if (!type_filter.empty()) {
        // Mode 2: find by type filter
        nlohmann::json found = find_nodes(type_filter, "", "", "", "");
        if (found.contains("nodes") && found["nodes"].is_array()) {
            for (const auto& entry : found["nodes"]) {
                if (entry.contains("path") && entry["path"].is_string()) {
                    std::string path = entry["path"].get<std::string>();
                    Node* node = scene_root->get_node_or_null(NodePath(path.c_str()));
                    if (node) {
                        targets.push_back(node);
                    }
                }
            }
        }
    } else {
        return {{"error", "Must provide node_paths array or type_filter"}};
    }

    if (targets.empty() && errors.empty()) {
        return {{"modified", 0}, {"errors", errors}, {"message", "No matching nodes found"}};
    }

    // Apply property to all targets in a single UndoRedo action
    undo_redo->create_action(String("MCP: Batch Set Property"));

    int modified = 0;
    for (Node* node : targets) {
        Variant new_value = parse_variant(value_str, node, property_name);
        Variant old_value = node->get(StringName(property_name.c_str()));

        undo_redo->add_do_method(node, "set", StringName(property_name.c_str()), new_value);
        undo_redo->add_undo_method(node, "set", StringName(property_name.c_str()), old_value);
        ++modified;
    }

    undo_redo->commit_action();

    nlohmann::json result = {{"modified", modified}, {"errors", errors}};
    return result;
}

// --- COMP-03: create_character ---

nlohmann::json create_character(const std::string& name, const std::string& char_type,
                                 const std::string& shape_type, const std::string& parent_path,
                                 const std::string& sprite_texture,
                                 const std::string& script_template,
                                 EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }
    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    bool is_3d = (char_type == "3d");
    std::string body_class = is_3d ? "CharacterBody3D" : "CharacterBody2D";

    // Find parent node
    Node* parent = scene_root;
    if (!parent_path.empty() && parent_path != ".") {
        parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
        if (!parent) {
            return {{"error", "Parent not found: " + parent_path}};
        }
    }

    // Create CharacterBody node
    Variant body_instance = ClassDB::instantiate(StringName(body_class.c_str()));
    Node* body_node = Object::cast_to<Node>(body_instance.operator Object*());
    if (!body_node) {
        return {{"error", "Failed to instantiate: " + body_class}};
    }
    body_node->set_name(String(name.c_str()));

    // Determine collision shape type
    std::string actual_shape_type = shape_type;
    if (actual_shape_type.empty()) {
        actual_shape_type = is_3d ? "capsule_3d" : "rectangle";
    }

    // Create CollisionShape node
    std::string cs_class = is_3d ? "CollisionShape3D" : "CollisionShape2D";
    Variant cs_instance = ClassDB::instantiate(StringName(cs_class.c_str()));
    Node* cs_node = Object::cast_to<Node>(cs_instance.operator Object*());
    if (!cs_node) {
        memdelete(body_node);
        return {{"error", "Failed to instantiate: " + cs_class}};
    }
    cs_node->set_name(String("CollisionShape"));

    // Create and configure shape resource
    if (is_3d) {
        Ref<Shape3D> shape_ref;
        if (actual_shape_type == "capsule_3d") {
            Ref<CapsuleShape3D> capsule;
            capsule.instantiate();
            capsule->set_radius(0.5);
            capsule->set_height(2.0);
            shape_ref = capsule;
        } else if (actual_shape_type == "box") {
            Ref<BoxShape3D> box;
            box.instantiate();
            box->set_size(Vector3(1.0, 1.0, 1.0));
            shape_ref = box;
        } else if (actual_shape_type == "sphere") {
            Ref<SphereShape3D> sphere;
            sphere.instantiate();
            sphere->set_radius(0.5);
            shape_ref = sphere;
        } else {
            // Default to capsule for unknown 3D shape
            Ref<CapsuleShape3D> capsule;
            capsule.instantiate();
            capsule->set_radius(0.5);
            capsule->set_height(2.0);
            shape_ref = capsule;
        }
        auto* cs3d = Object::cast_to<CollisionShape3D>(cs_node);
        if (cs3d && shape_ref.is_valid()) {
            cs3d->set_shape(shape_ref);
        }
    } else {
        Ref<Shape2D> shape_ref;
        if (actual_shape_type == "rectangle") {
            Ref<RectangleShape2D> rect;
            rect.instantiate();
            rect->set_size(Vector2(30, 30));
            shape_ref = rect;
        } else if (actual_shape_type == "circle") {
            Ref<CircleShape2D> circle;
            circle.instantiate();
            circle->set_radius(15.0);
            shape_ref = circle;
        } else if (actual_shape_type == "capsule") {
            Ref<CapsuleShape2D> capsule;
            capsule.instantiate();
            capsule->set_radius(10.0);
            capsule->set_height(30.0);
            shape_ref = capsule;
        } else {
            // Default to rectangle for unknown 2D shape
            Ref<RectangleShape2D> rect;
            rect.instantiate();
            rect->set_size(Vector2(30, 30));
            shape_ref = rect;
        }
        auto* cs2d = Object::cast_to<CollisionShape2D>(cs_node);
        if (cs2d && shape_ref.is_valid()) {
            cs2d->set_shape(shape_ref);
        }
    }

    // Optional: Create Sprite node
    Node* sprite_node = nullptr;
    if (!sprite_texture.empty()) {
        std::string sprite_class = is_3d ? "Sprite3D" : "Sprite2D";
        Variant sprite_instance = ClassDB::instantiate(StringName(sprite_class.c_str()));
        sprite_node = Object::cast_to<Node>(sprite_instance.operator Object*());
        if (sprite_node) {
            sprite_node->set_name(String("Sprite"));
            Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(String(sprite_texture.c_str()));
            if (tex.is_valid()) {
                if (is_3d) {
                    auto* s3d = Object::cast_to<Sprite3D>(sprite_node);
                    if (s3d) s3d->set_texture(tex);
                } else {
                    auto* s2d = Object::cast_to<Sprite2D>(sprite_node);
                    if (s2d) s2d->set_texture(tex);
                }
            }
        }
    }

    // Optional: Create movement script
    Ref<GDScript> script;
    std::string script_path_str;
    if (script_template == "basic_movement") {
        std::string code;
        if (is_3d) {
            code = "extends CharacterBody3D\n\nconst SPEED = 5.0\nconst JUMP_VELOCITY = 4.5\n\nfunc _physics_process(delta: float) -> void:\n\tif not is_on_floor():\n\t\tvelocity += get_gravity() * delta\n\tvar input_dir = Input.get_vector(\"ui_left\", \"ui_right\", \"ui_up\", \"ui_down\")\n\tvar direction = (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()\n\tif direction:\n\t\tvelocity.x = direction.x * SPEED\n\t\tvelocity.z = direction.z * SPEED\n\telse:\n\t\tvelocity.x = move_toward(velocity.x, 0, SPEED)\n\t\tvelocity.z = move_toward(velocity.z, 0, SPEED)\n\tif Input.is_action_just_pressed(\"ui_accept\") and is_on_floor():\n\t\tvelocity.y = JUMP_VELOCITY\n\tmove_and_slide()\n";
        } else {
            code = "extends CharacterBody2D\n\nconst SPEED = 300.0\nconst JUMP_VELOCITY = -400.0\n\nfunc _physics_process(delta: float) -> void:\n\tvar velocity_x = Input.get_axis(\"ui_left\", \"ui_right\") * SPEED\n\tvelocity.x = velocity_x\n\tif not is_on_floor():\n\t\tvelocity += get_gravity() * delta\n\tif Input.is_action_just_pressed(\"ui_accept\") and is_on_floor():\n\t\tvelocity.y = JUMP_VELOCITY\n\tmove_and_slide()\n";
        }
        // Generate script path from character name
        std::string safe_name = name;
        std::transform(safe_name.begin(), safe_name.end(), safe_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::replace(safe_name.begin(), safe_name.end(), ' ', '_');
        script_path_str = "res://" + safe_name + "_movement.gd";

        // Write script file to disk first
        Ref<FileAccess> f = FileAccess::open(String(script_path_str.c_str()), FileAccess::WRITE);
        if (f.is_valid()) {
            f->store_string(String(code.c_str()));
            f->close();
        }

        // Manual GDScript construction (avoid ResourceLoader::load crash on new files)
        script.instantiate();
        script->set_source_code(String(code.c_str()));
        script->set_path(String(script_path_str.c_str()));
        script->reload();
    }

    // Build single UndoRedo action
    undo_redo->create_action(String("MCP: Create Character"));

    // Add body to parent
    undo_redo->add_do_method(parent, "add_child", body_node, true);
    undo_redo->add_do_method(body_node, "set_owner", scene_root);
    undo_redo->add_do_reference(body_node);
    undo_redo->add_undo_method(parent, "remove_child", body_node);

    // Add collision shape to body
    undo_redo->add_do_method(body_node, "add_child", cs_node, true);
    undo_redo->add_do_method(cs_node, "set_owner", scene_root);
    undo_redo->add_do_reference(cs_node);
    undo_redo->add_undo_method(body_node, "remove_child", cs_node);

    // Track created node names
    nlohmann::json nodes_created = nlohmann::json::array();
    nodes_created.push_back(body_class);
    nodes_created.push_back(cs_class);

    // Add sprite if created
    if (sprite_node) {
        undo_redo->add_do_method(body_node, "add_child", sprite_node, true);
        undo_redo->add_do_method(sprite_node, "set_owner", scene_root);
        undo_redo->add_do_reference(sprite_node);
        undo_redo->add_undo_method(body_node, "remove_child", sprite_node);
        nodes_created.push_back(is_3d ? "Sprite3D" : "Sprite2D");
    }

    // Attach script if created
    if (script.is_valid()) {
        undo_redo->add_do_method(body_node, "set_script", script);
        undo_redo->add_undo_method(body_node, "set_script", Variant());
    }

    undo_redo->commit_action();

    // Build result path
    std::string node_name_str(String(body_node->get_name()).utf8().get_data());
    std::string actual_path;
    if (parent_path.empty() || parent_path == ".") {
        actual_path = node_name_str;
    } else {
        actual_path = parent_path + "/" + node_name_str;
    }

    nlohmann::json result = {
        {"success", true},
        {"root_path", actual_path},
        {"nodes_created", nodes_created},
        {"type", char_type}
    };
    if (!script_path_str.empty()) {
        result["script"] = script_path_str;
    }
    return result;
}

// --- COMP-04: create_ui_panel ---

nlohmann::json create_ui_panel(const nlohmann::json& spec, const std::string& parent_path,
                                EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }
    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Parse spec
    if (!spec.contains("root_type") || !spec["root_type"].is_string()) {
        return {{"error", "spec.root_type is required"}};
    }
    std::string root_type = spec["root_type"].get<std::string>();

    // Validate root_type is a Node subclass
    if (!ClassDB::class_exists(StringName(root_type.c_str()))) {
        return {{"error", "Unknown class: " + root_type}};
    }
    if (!ClassDB::is_parent_class(StringName(root_type.c_str()), StringName("Control"))) {
        return {{"error", root_type + " is not a Control subclass"}};
    }

    // Find parent node
    Node* parent = scene_root;
    if (!parent_path.empty() && parent_path != ".") {
        parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
        if (!parent) {
            return {{"error", "Parent not found: " + parent_path}};
        }
    }

    // Determine root name
    std::string root_name = root_type;
    if (spec.contains("name") && spec["name"].is_string()) {
        root_name = spec["name"].get<std::string>();
    }

    // Create root container
    Variant root_instance = ClassDB::instantiate(StringName(root_type.c_str()));
    Node* root_node = Object::cast_to<Node>(root_instance.operator Object*());
    if (!root_node) {
        return {{"error", "Failed to instantiate: " + root_type}};
    }
    root_node->set_name(String(root_name.c_str()));

    // Begin single UndoRedo action
    undo_redo->create_action(String("MCP: Create UI Panel"));

    // Add root to parent
    undo_redo->add_do_method(parent, "add_child", root_node, true);
    undo_redo->add_do_method(root_node, "set_owner", scene_root);
    undo_redo->add_do_reference(root_node);
    undo_redo->add_undo_method(parent, "remove_child", root_node);

    // Apply style (StyleBoxFlat) if present
    if (spec.contains("style") && spec["style"].is_object()) {
        auto& style = spec["style"];
        Ref<StyleBoxFlat> stylebox;
        stylebox.instantiate();

        if (style.contains("bg_color") && style["bg_color"].is_string()) {
            std::string bg = style["bg_color"].get<std::string>();
            stylebox->set_bg_color(Color::html(String(bg.c_str())));
        }
        if (style.contains("corner_radius") && style["corner_radius"].is_number_integer()) {
            int r = style["corner_radius"].get<int>();
            stylebox->set_corner_radius_all(r);
        }
        if (style.contains("border_width") && style["border_width"].is_number_integer()) {
            int w = style["border_width"].get<int>();
            stylebox->set_border_width_all(w);
        }
        if (style.contains("border_color") && style["border_color"].is_string()) {
            std::string bc = style["border_color"].get<std::string>();
            stylebox->set_border_color(Color::html(String(bc.c_str())));
        }
        if (style.contains("content_margin_left") && style["content_margin_left"].is_number()) {
            stylebox->set_content_margin(SIDE_LEFT, style["content_margin_left"].get<double>());
        }
        if (style.contains("content_margin_right") && style["content_margin_right"].is_number()) {
            stylebox->set_content_margin(SIDE_RIGHT, style["content_margin_right"].get<double>());
        }
        if (style.contains("content_margin_top") && style["content_margin_top"].is_number()) {
            stylebox->set_content_margin(SIDE_TOP, style["content_margin_top"].get<double>());
        }
        if (style.contains("content_margin_bottom") && style["content_margin_bottom"].is_number()) {
            stylebox->set_content_margin(SIDE_BOTTOM, style["content_margin_bottom"].get<double>());
        }

        Control* root_ctrl = Object::cast_to<Control>(root_node);
        if (root_ctrl) {
            undo_redo->add_do_method(root_ctrl, "add_theme_stylebox_override",
                                      StringName("panel"), stylebox);
            undo_redo->add_undo_method(root_ctrl, "remove_theme_stylebox_override",
                                        StringName("panel"));
        }
    }

    // Create children
    int children_created = 0;
    nlohmann::json warnings = nlohmann::json::array();

    if (spec.contains("children") && spec["children"].is_array()) {
        for (const auto& child_spec : spec["children"]) {
            if (!child_spec.is_object()) continue;
            if (!child_spec.contains("type") || !child_spec["type"].is_string()) continue;

            std::string child_type = child_spec["type"].get<std::string>();

            // Validate child type
            if (!ClassDB::class_exists(StringName(child_type.c_str()))) {
                warnings.push_back("Unknown class: " + child_type + ", skipped");
                continue;
            }

            // Warn about nested children (max 2 levels)
            if (child_spec.contains("children")) {
                warnings.push_back("Nested children in '" + child_type + "' skipped (max 2 levels)");
            }

            Variant child_instance = ClassDB::instantiate(StringName(child_type.c_str()));
            Node* child_node = Object::cast_to<Node>(child_instance.operator Object*());
            if (!child_node) {
                warnings.push_back("Failed to instantiate: " + child_type);
                continue;
            }

            // Set child name
            std::string child_name = child_type;
            if (child_spec.contains("name") && child_spec["name"].is_string()) {
                child_name = child_spec["name"].get<std::string>();
            }
            child_node->set_name(String(child_name.c_str()));

            // Add child to root via undo_redo
            undo_redo->add_do_method(root_node, "add_child", child_node, true);
            undo_redo->add_do_method(child_node, "set_owner", scene_root);
            undo_redo->add_do_reference(child_node);
            undo_redo->add_undo_method(root_node, "remove_child", child_node);

            // Set known properties on child node
            const char* known_props[] = {"text", "placeholder_text", "min_value", "max_value",
                                          "value", "button_pressed", "alignment"};
            for (const char* prop : known_props) {
                if (child_spec.contains(prop)) {
                    Variant val;
                    if (child_spec[prop].is_string()) {
                        val = String(child_spec[prop].get<std::string>().c_str());
                    } else if (child_spec[prop].is_number_integer()) {
                        val = child_spec[prop].get<int>();
                    } else if (child_spec[prop].is_number_float()) {
                        val = child_spec[prop].get<double>();
                    } else if (child_spec[prop].is_boolean()) {
                        val = child_spec[prop].get<bool>();
                    }
                    undo_redo->add_do_method(child_node, "set", StringName(prop), val);
                }
            }

            // Apply theme overrides on child if present
            if (child_spec.contains("theme_overrides") && child_spec["theme_overrides"].is_object()) {
                Control* child_ctrl = Object::cast_to<Control>(child_node);
                if (child_ctrl) {
                    for (auto& [key, value] : child_spec["theme_overrides"].items()) {
                        if (key.find("font_size") != std::string::npos ||
                            key.find("outline_size") != std::string::npos ||
                            key.find("separation") != std::string::npos) {
                            // Integer theme override (font_size, constants)
                            if (value.is_number_integer()) {
                                undo_redo->add_do_method(child_ctrl, "add_theme_constant_override",
                                                          StringName(key.c_str()), value.get<int>());
                                undo_redo->add_undo_method(child_ctrl, "remove_theme_constant_override",
                                                            StringName(key.c_str()));
                            }
                        } else if (key.find("color") != std::string::npos) {
                            // Color theme override
                            if (value.is_string()) {
                                std::string color_str = value.get<std::string>();
                                Color c = Color::html(String(color_str.c_str()));
                                undo_redo->add_do_method(child_ctrl, "add_theme_color_override",
                                                          StringName(key.c_str()), c);
                                undo_redo->add_undo_method(child_ctrl, "remove_theme_color_override",
                                                            StringName(key.c_str()));
                            }
                        }
                    }
                }
            }

            children_created++;
        }
    }

    undo_redo->commit_action();

    // Build result path
    std::string node_name_out(String(root_node->get_name()).utf8().get_data());
    std::string actual_path;
    if (parent_path.empty() || parent_path == ".") {
        actual_path = node_name_out;
    } else {
        actual_path = parent_path + "/" + node_name_out;
    }

    nlohmann::json result = {
        {"success", true},
        {"root_path", actual_path},
        {"children_created", children_created}
    };
    if (!warnings.empty()) {
        result["warnings"] = warnings;
    }
    return result;
}

// --- COMP-05: duplicate_node ---

// Helper: recursively set owner on all descendants
static void set_owner_recursive(Node* node, Node* owner, EditorUndoRedoManager* undo_redo) {
    int child_count = node->get_child_count();
    for (int i = 0; i < child_count; ++i) {
        Node* child = node->get_child(i);
        if (child) {
            undo_redo->add_do_method(child, "set_owner", owner);
            set_owner_recursive(child, owner, undo_redo);
        }
    }
}

// Helper: count all nodes in a subtree (including root)
static int count_nodes_recursive(Node* node) {
    int count = 1;
    int child_count = node->get_child_count();
    for (int i = 0; i < child_count; ++i) {
        Node* child = node->get_child(i);
        if (child) {
            count += count_nodes_recursive(child);
        }
    }
    return count;
}

nlohmann::json duplicate_node(const std::string& source_path, const std::string& target_parent_path,
                               const std::string& new_name,
                               EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }
    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Find source node
    Node* source_node = scene_root->get_node_or_null(NodePath(source_path.c_str()));
    if (!source_node) {
        return {{"error", "Node not found: " + source_path}};
    }

    // Cannot duplicate scene root
    if (source_node == scene_root) {
        return {{"error", "Cannot duplicate scene root"}};
    }

    // Find target parent
    Node* target_parent = source_node->get_parent();
    if (!target_parent_path.empty() && target_parent_path != ".") {
        target_parent = scene_root->get_node_or_null(NodePath(target_parent_path.c_str()));
        if (!target_parent) {
            return {{"error", "Parent not found: " + target_parent_path}};
        }
    } else if (target_parent_path == ".") {
        target_parent = scene_root;
    }

    // Deep-copy via Godot's Node::duplicate()
    Node* duplicated = source_node->duplicate();
    if (!duplicated) {
        return {{"error", "Failed to duplicate node: " + source_path}};
    }

    // Optionally rename
    if (!new_name.empty()) {
        duplicated->set_name(String(new_name.c_str()));
    }

    int node_count = count_nodes_recursive(duplicated);

    // Build single UndoRedo action
    undo_redo->create_action(String("MCP: Duplicate Node"));

    undo_redo->add_do_method(target_parent, "add_child", duplicated, true);
    undo_redo->add_do_method(duplicated, "set_owner", scene_root);
    undo_redo->add_do_reference(duplicated);
    undo_redo->add_undo_method(target_parent, "remove_child", duplicated);

    // Recursively set owner on all descendants
    set_owner_recursive(duplicated, scene_root, undo_redo);

    undo_redo->commit_action();

    // Build result path
    std::string dup_name(String(duplicated->get_name()).utf8().get_data());
    std::string target_path_str;
    if (target_parent == scene_root) {
        target_path_str = dup_name;
    } else {
        std::string parent_rel(String(scene_root->get_path_to(target_parent)).utf8().get_data());
        target_path_str = parent_rel + "/" + dup_name;
    }

    return {
        {"success", true},
        {"source_path", source_path},
        {"new_path", target_path_str},
        {"nodes_duplicated", node_count}
    };
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
