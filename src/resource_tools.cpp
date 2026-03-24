#include "resource_tools.h"

#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>

// --- Pure C++ helpers (no Godot dependency) ---

std::string classify_file_type(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> type_map = {
        {".tscn", "scene"}, {".scn", "scene"},
        {".gd", "script"},
        {".tres", "resource"}, {".res", "resource"},
        {".png", "image"}, {".jpg", "image"}, {".jpeg", "image"},
        {".svg", "image"}, {".webp", "image"},
        {".ogg", "audio"}, {".wav", "audio"}, {".mp3", "audio"}
    };

    auto it = type_map.find(extension);
    if (it != type_map.end()) {
        return it->second;
    }
    return "other";
}

std::string truncate_script_source(const std::string& source, int line_count) {
    if (line_count <= 100) {
        return source;
    }

    // Keep first 50 lines + truncation notice
    std::istringstream stream(source);
    std::string result;
    std::string line;
    int count = 0;
    while (count < 50 && std::getline(stream, line)) {
        result += line + "\n";
        count++;
    }
    result += "[...truncated, " + std::to_string(line_count) + " total lines...]\n";
    return result;
}

// --- Godot-dependent implementations ---
#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

// Helper: resolve a node path from scene root. Returns nullptr if not found.
static Node* resolve_node_from_root(const std::string& node_path) {
    auto* ei = EditorInterface::get_singleton();
    if (!ei) return nullptr;
    Node* scene_root = ei->get_edited_scene_root();
    if (!scene_root) return nullptr;
    if (node_path.empty() || node_path == ".") return scene_root;
    return scene_root->get_node_or_null(NodePath(String(node_path.c_str())));
}

// Helper: get script source code for a node (reads from file)
static std::string get_script_source(Script* script) {
    String path = script->get_path();
    if (path.is_empty()) return "";

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (!file.is_valid()) return "";

    String content = file->get_as_text();
    return std::string(content.utf8().get_data());
}

// Helper: count lines in a string
static int count_lines(const std::string& text) {
    if (text.empty()) return 0;
    int count = 0;
    for (char c : text) {
        if (c == '\n') count++;
    }
    // Count last line even if no trailing newline
    if (!text.empty() && text.back() != '\n') count++;
    return count;
}

// Helper: build incoming connections map for all nodes in the scene tree
// Returns map of node_path -> array of {source, signal, method}
static void collect_incoming_connections(
    Node* node, Node* scene_root,
    std::unordered_map<std::string, nlohmann::json>& incoming_map
) {
    // For each signal on this node, check outgoing connections
    TypedArray<Dictionary> signals = node->get_signal_list();
    for (int i = 0; i < signals.size(); i++) {
        Dictionary sig = signals[i];
        String sig_name = sig["name"];

        TypedArray<Dictionary> conns = node->get_signal_connection_list(sig_name);
        for (int j = 0; j < conns.size(); j++) {
            Dictionary conn = conns[j];
            Callable callable = conn["callable"];
            Object* target_obj = callable.get_object();
            if (!target_obj) continue;

            Node* target_node = Object::cast_to<Node>(target_obj);
            if (!target_node || !target_node->is_inside_tree()) continue;

            String target_path = String(scene_root->get_path_to(target_node));
            std::string target_path_str(target_path.utf8().get_data());

            String source_path = String(scene_root->get_path_to(node));

            nlohmann::json entry;
            entry["source"] = std::string(source_path.utf8().get_data());
            entry["signal"] = std::string(sig_name.utf8().get_data());
            entry["method"] = std::string(String(callable.get_method()).utf8().get_data());

            incoming_map[target_path_str].push_back(entry);
        }
    }

    // Recurse into children
    for (int i = 0; i < node->get_child_count(); i++) {
        Node* child = node->get_child(i);
        if (child) {
            collect_incoming_connections(child, scene_root, incoming_map);
        }
    }
}

// Helper: serialize enriched node for scene tree
static nlohmann::json serialize_enriched_node(
    Node* node, Node* scene_root,
    int current_depth, int max_depth,
    const std::unordered_map<std::string, nlohmann::json>& incoming_map
) {
    nlohmann::json result;

    // Core fields
    result["name"] = std::string(String(node->get_name()).utf8().get_data());
    result["type"] = std::string(String(node->get_class()).utf8().get_data());

    std::string node_path_str;
    if (node == scene_root) {
        node_path_str = std::string(String(node->get_name()).utf8().get_data());
    } else {
        node_path_str = std::string(String(scene_root->get_path_to(node)).utf8().get_data());
    }
    result["path"] = node_path_str;

    // Script enrichment
    Variant script_var = node->get_script();
    if (script_var.get_type() != Variant::NIL) {
        Ref<Script> script = script_var;
        if (script.is_valid()) {
            result["script_path"] = std::string(script->get_path().utf8().get_data());
            std::string source = get_script_source(script.ptr());
            int lines = count_lines(source);
            result["script_line_count"] = lines;
            result["script_source"] = truncate_script_source(source, lines);
        }
    }

    // Signal connections: outgoing from this node
    nlohmann::json outgoing = nlohmann::json::array();
    TypedArray<Dictionary> signals = node->get_signal_list();
    for (int i = 0; i < signals.size(); i++) {
        Dictionary sig = signals[i];
        String sig_name = sig["name"];

        TypedArray<Dictionary> conns = node->get_signal_connection_list(sig_name);
        for (int j = 0; j < conns.size(); j++) {
            Dictionary conn = conns[j];
            Callable callable = conn["callable"];
            Object* target_obj = callable.get_object();
            if (!target_obj) continue;

            Node* target_node = Object::cast_to<Node>(target_obj);
            if (!target_node || !target_node->is_inside_tree()) continue;

            nlohmann::json entry;
            entry["signal"] = std::string(sig_name.utf8().get_data());
            entry["target"] = std::string(String(scene_root->get_path_to(target_node)).utf8().get_data());
            entry["method"] = std::string(String(callable.get_method()).utf8().get_data());
            outgoing.push_back(entry);
        }
    }

    // Incoming connections (from pre-computed map)
    nlohmann::json incoming = nlohmann::json::array();
    auto it = incoming_map.find(node_path_str);
    if (it != incoming_map.end()) {
        incoming = it->second;
    }

    if (!outgoing.empty() || !incoming.empty()) {
        result["signals"] = {
            {"outgoing", outgoing},
            {"incoming", incoming}
        };
    }

    // Properties: transform, visibility, @export properties
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

    CanvasItem* canvas_item = Object::cast_to<CanvasItem>(node);
    if (canvas_item) {
        result["visible"] = canvas_item->is_visible();
    } else if (node3d) {
        result["visible"] = node3d->is_visible();
    }

    // @export properties (PROPERTY_USAGE_SCRIPT_VARIABLE flag)
    nlohmann::json exports = nlohmann::json::object();
    TypedArray<Dictionary> prop_list = node->get_property_list();
    for (int i = 0; i < prop_list.size(); i++) {
        Dictionary prop = prop_list[i];
        int usage = prop["usage"];
        if (usage & PROPERTY_USAGE_SCRIPT_VARIABLE) {
            String prop_name = prop["name"];
            std::string name_str(prop_name.utf8().get_data());
            Variant value = node->get(prop_name);
            String value_str = String(value);
            exports[name_str] = std::string(value_str.utf8().get_data());
        }
    }
    if (!exports.empty()) {
        result["exports"] = exports;
    }

    // Children: recursive traversal with depth limiting
    int child_count = node->get_child_count();
    if (max_depth == -1 || current_depth < max_depth) {
        nlohmann::json children = nlohmann::json::array();
        for (int i = 0; i < child_count; i++) {
            Node* child = node->get_child(i);
            if (child) {
                children.push_back(serialize_enriched_node(
                    child, scene_root, current_depth + 1, max_depth, incoming_map));
            }
        }
        result["children"] = children;
    } else if (child_count > 0) {
        result["children_truncated"] = true;
        result["child_count"] = child_count;
    } else {
        result["children"] = nlohmann::json::array();
    }

    return result;
}

// Helper: count total nodes in a JSON tree
static int count_nodes(const nlohmann::json& tree) {
    int count = 1;
    if (tree.contains("children") && tree["children"].is_array()) {
        for (const auto& child : tree["children"]) {
            count += count_nodes(child);
        }
    }
    return count;
}

nlohmann::json get_enriched_scene_tree() {
    Node* root = EditorInterface::get_singleton()->get_edited_scene_root();

    if (root == nullptr) {
        return {
            {"tree", nullptr},
            {"message", "No scene currently open"}
        };
    }

    // Build incoming connections map first (full tree walk)
    std::unordered_map<std::string, nlohmann::json> incoming_map;
    // Initialize all entries as arrays
    collect_incoming_connections(root, root, incoming_map);

    // Serialize with depth=3
    nlohmann::json tree = serialize_enriched_node(root, root, 0, 3, incoming_map);

    // Check 10KB size limit
    std::string serialized = tree.dump();
    if (serialized.size() > 10240) {
        int node_count = count_nodes(tree);
        std::string root_name = tree.value("name", "unknown");
        std::string root_type = tree.value("type", "unknown");
        return {
            {"summary", true},
            {"node_count", node_count},
            {"root", {{"name", root_name}, {"type", root_type}}},
            {"message", "Response exceeds 10KB limit. Use godot://node/{path} resource templates to query specific nodes."},
            {"size_bytes", serialized.size()}
        };
    }

    return tree;
}

// Helper: recursive file collection with enriched metadata
static void collect_enriched_files(const String& dir_path, nlohmann::json& files) {
    Ref<DirAccess> dir = DirAccess::open(dir_path);
    if (!dir.is_valid()) return;

    dir->list_dir_begin();
    String name = dir->get_next();

    while (!name.is_empty()) {
        if (name.begins_with(".")) {
            name = dir->get_next();
            continue;
        }

        String full_path = dir_path;
        if (!full_path.ends_with("/")) {
            full_path += "/";
        }
        full_path += name;

        if (dir->current_is_dir()) {
            collect_enriched_files(full_path, files);
        } else {
            String ext = name.get_extension();
            std::string ext_str = "." + std::string(ext.utf8().get_data());
            std::string path_str(full_path.utf8().get_data());

            nlohmann::json file_entry;
            file_entry["path"] = path_str;
            file_entry["extension"] = ext_str;
            file_entry["type"] = classify_file_type(ext_str);

            // File size
            Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::READ);
            if (file.is_valid()) {
                file_entry["size"] = static_cast<int64_t>(file->get_length());
            }

            // Modification time
            file_entry["modified_time"] = static_cast<int64_t>(FileAccess::get_modified_time(full_path));

            files.push_back(file_entry);
        }

        name = dir->get_next();
    }

    dir->list_dir_end();
}

nlohmann::json get_enriched_project_files() {
    nlohmann::json files = nlohmann::json::array();
    collect_enriched_files("res://", files);
    return {
        {"success", true},
        {"files", files},
        {"count", files.size()}
    };
}

nlohmann::json enrich_node_detail(const std::string& node_path) {
    Node* node = resolve_node_from_root(node_path);
    if (!node) {
        return {{"success", false}, {"error", "Node not found: " + node_path}};
    }

    auto* ei = EditorInterface::get_singleton();
    Node* scene_root = ei->get_edited_scene_root();

    nlohmann::json result;
    result["success"] = true;

    // Core fields
    result["name"] = std::string(String(node->get_name()).utf8().get_data());
    result["type"] = std::string(String(node->get_class()).utf8().get_data());

    if (node == scene_root) {
        result["path"] = std::string(String(node->get_name()).utf8().get_data());
    } else {
        result["path"] = std::string(String(scene_root->get_path_to(node)).utf8().get_data());
    }

    // All properties (STORAGE + EDITOR, skip INTERNAL)
    nlohmann::json properties = nlohmann::json::object();
    TypedArray<Dictionary> prop_list = node->get_property_list();
    for (int i = 0; i < prop_list.size(); i++) {
        Dictionary prop = prop_list[i];
        String prop_name = prop["name"];
        int usage = prop["usage"];

        if (usage & PROPERTY_USAGE_INTERNAL) continue;
        if (!(usage & PROPERTY_USAGE_STORAGE) && !(usage & PROPERTY_USAGE_EDITOR)) continue;

        std::string name_str(prop_name.utf8().get_data());
        if (name_str.empty()) continue;

        Variant value = node->get(prop_name);
        String value_str = String(value);
        properties[name_str] = std::string(value_str.utf8().get_data());
    }
    result["properties"] = properties;

    // Full script source (no truncation)
    Variant script_var = node->get_script();
    if (script_var.get_type() != Variant::NIL) {
        Ref<Script> script = script_var;
        if (script.is_valid()) {
            result["script_path"] = std::string(script->get_path().utf8().get_data());
            std::string source = get_script_source(script.ptr());
            result["script_source"] = source;
            result["script_line_count"] = count_lines(source);
        }
    }

    // Child list (one level)
    nlohmann::json children = nlohmann::json::array();
    for (int i = 0; i < node->get_child_count(); i++) {
        Node* child = node->get_child(i);
        if (child) {
            children.push_back({
                {"name", std::string(String(child->get_name()).utf8().get_data())},
                {"type", std::string(String(child->get_class()).utf8().get_data())}
            });
        }
    }
    result["children"] = children;

    // Signal connections: outgoing
    nlohmann::json outgoing = nlohmann::json::array();
    TypedArray<Dictionary> signals = node->get_signal_list();
    for (int i = 0; i < signals.size(); i++) {
        Dictionary sig = signals[i];
        String sig_name = sig["name"];

        TypedArray<Dictionary> conns = node->get_signal_connection_list(sig_name);
        for (int j = 0; j < conns.size(); j++) {
            Dictionary conn = conns[j];
            Callable callable = conn["callable"];
            Object* target_obj = callable.get_object();
            if (!target_obj) continue;

            Node* target_node = Object::cast_to<Node>(target_obj);
            if (!target_node || !target_node->is_inside_tree()) continue;

            nlohmann::json entry;
            entry["signal"] = std::string(sig_name.utf8().get_data());
            entry["target"] = std::string(String(scene_root->get_path_to(target_node)).utf8().get_data());
            entry["method"] = std::string(String(callable.get_method()).utf8().get_data());
            outgoing.push_back(entry);
        }
    }

    // Incoming connections (walk full tree)
    nlohmann::json incoming = nlohmann::json::array();
    std::unordered_map<std::string, nlohmann::json> incoming_map;
    collect_incoming_connections(scene_root, scene_root, incoming_map);

    std::string this_path;
    if (node == scene_root) {
        this_path = std::string(String(node->get_name()).utf8().get_data());
    } else {
        this_path = std::string(String(scene_root->get_path_to(node)).utf8().get_data());
    }

    auto it = incoming_map.find(this_path);
    if (it != incoming_map.end()) {
        incoming = it->second;
    }

    result["signals"] = {
        {"outgoing", outgoing},
        {"incoming", incoming}
    };

    return result;
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
