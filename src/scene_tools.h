#ifndef GODOT_MCP_MEOW_SCENE_TOOLS_H
#define GODOT_MCP_MEOW_SCENE_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class Node;
}

// Get the current scene tree as JSON
// max_depth: -1 for unlimited, otherwise max traversal depth
// include_properties: include transform, visible, has_script, script_path per node
// root_path: node path to start traversal from (empty = scene root)
nlohmann::json get_scene_tree(int max_depth = -1, bool include_properties = true, const std::string& root_path = "");

// Recursively serialize a node and its children to JSON
nlohmann::json serialize_node(godot::Node* node, int current_depth, int max_depth, bool include_properties);

#endif // GODOT_MCP_MEOW_SCENE_TOOLS_H
