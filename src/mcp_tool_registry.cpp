#include "mcp_tool_registry.h"

const std::vector<ToolDef>& get_all_tools() {
    static std::vector<ToolDef> tools;
    // STUB: empty - tests should fail (RED phase)
    return tools;
}

nlohmann::json get_filtered_tools_json(const GodotVersion& current) {
    // STUB: empty array - tests should fail (RED phase)
    return nlohmann::json::array();
}

int get_tool_count(const GodotVersion& current) {
    // STUB: returns 0 - tests should fail (RED phase)
    return 0;
}
