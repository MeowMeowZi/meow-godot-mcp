#ifndef MEOW_GODOT_MCP_MCP_TOOL_REGISTRY_H
#define MEOW_GODOT_MCP_MCP_TOOL_REGISTRY_H

// Pure C++17 + nlohmann/json -- NO Godot headers
// This allows unit testing without godot-cpp dependency

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

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

struct ToolDef {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
    GodotVersion min_version;
};

// Returns the static registry of all tool definitions
const std::vector<ToolDef>& get_all_tools();

// Returns a JSON array of tools where current >= tool.min_version
// Format matches MCP tools/list result.tools array
nlohmann::json get_filtered_tools_json(const GodotVersion& current);

// Returns count of tools available for the given Godot version
int get_tool_count(const GodotVersion& current);

#endif // MEOW_GODOT_MCP_MCP_TOOL_REGISTRY_H
