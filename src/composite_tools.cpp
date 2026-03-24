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
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>

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

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
