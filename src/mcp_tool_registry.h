#ifndef MEOW_GODOT_MCP_MCP_TOOL_REGISTRY_H
#define MEOW_GODOT_MCP_MCP_TOOL_REGISTRY_H

// Pure C++17 + nlohmann/json -- NO Godot headers
// This allows unit testing without godot-cpp dependency

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <set>

struct GodotVersion {
    int major;
    int minor;
    int patch;

    bool operator>=(const GodotVersion& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch >= other.patch;
    }

    bool operator==(const GodotVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
};

// Tool categories for dock panel grouping
enum class ToolCategory {
    SCENE,      // Scene tree + node operations
    SCRIPT,     // Script read/write/edit/attach
    PROJECT,    // Project files, scene management
    RUNTIME,    // Run/stop game, output, screenshots
    INPUT,      // Input injection, text injection
    QUERY,      // Runtime queries (eval, properties)
    TILEMAP,    // TileMap operations
    COMPOSITE,  // Batch operations, node tree, duplicate
    DX          // Developer experience (validate, restart)
};

struct ToolDef {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
    GodotVersion min_version;
    ToolCategory category;
};

// Returns the static registry of all tool definitions
const std::vector<ToolDef>& get_all_tools();

// Returns a JSON array of tools where current >= tool.min_version
// and tool is not disabled. Format matches MCP tools/list result.tools array
nlohmann::json get_filtered_tools_json(const GodotVersion& current);

// Returns count of tools available for the given Godot version (excluding disabled)
int get_tool_count(const GodotVersion& current);

// Category display names (Chinese)
const char* get_category_name(ToolCategory cat);

// Tool enable/disable (persisted in ProjectSettings)
void set_tool_disabled(const std::string& name, bool disabled);
bool is_tool_disabled(const std::string& name);
const std::set<std::string>& get_disabled_tools();

#endif // MEOW_GODOT_MCP_MCP_TOOL_REGISTRY_H
