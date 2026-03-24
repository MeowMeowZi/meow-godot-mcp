#include "resource_tools.h"

#include <sstream>
#include <unordered_map>
#include <vector>

// --- Pure C++ helpers (no Godot dependency) ---

std::string classify_file_type(const std::string& extension) {
    // Stub: return empty to fail tests
    return "";
}

std::string truncate_script_source(const std::string& source, int line_count) {
    // Stub: return empty to fail tests
    return "";
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

using namespace godot;

nlohmann::json get_enriched_scene_tree() {
    return {{"error", "not implemented"}};
}

nlohmann::json get_enriched_project_files() {
    return {{"error", "not implemented"}};
}

nlohmann::json enrich_node_detail(const std::string& node_path) {
    return {{"error", "not implemented"}};
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
