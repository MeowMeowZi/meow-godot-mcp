#ifndef MEOW_GODOT_MCP_RESOURCE_TOOLS_H
#define MEOW_GODOT_MCP_RESOURCE_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

// Pure C++ helpers (testable without Godot)

// Classify a file extension into a category string
// Returns: "scene", "script", "resource", "image", "audio", or "other"
std::string classify_file_type(const std::string& extension);

// Truncate script source code if it exceeds 100 lines
// Returns first 50 lines + truncation notice for content > 100 lines
// Returns content unchanged for <= 100 lines
std::string truncate_script_source(const std::string& source, int line_count);

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// Get enriched scene tree: nodes with inline scripts, signals, @export properties
// Depth limited to 3, 10KB response size limit
nlohmann::json get_enriched_scene_tree();

// Get enriched project files: file size, type classification, modification timestamps
nlohmann::json get_enriched_project_files();

// Get full detail for a single node: all properties, full script source,
// child list (one level), all signal connections
nlohmann::json enrich_node_detail(const std::string& node_path);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED

#endif // MEOW_GODOT_MCP_RESOURCE_TOOLS_H
