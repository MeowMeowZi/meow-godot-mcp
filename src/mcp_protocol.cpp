#include "mcp_protocol.h"

namespace mcp {

// Stub implementations -- will be filled in GREEN phase

ParseResult parse_jsonrpc(const std::string& json_str) {
    return {false, {}, {}};
}

nlohmann::json create_initialize_response(const nlohmann::json& id) {
    return {};
}

nlohmann::json create_tools_list_response(const nlohmann::json& id) {
    return {};
}

nlohmann::json create_tool_result(const nlohmann::json& id, const nlohmann::json& content_data) {
    return {};
}

nlohmann::json create_error_response(const nlohmann::json& id, int code, const std::string& message) {
    return {};
}

nlohmann::json create_tool_not_found_error(const nlohmann::json& id, const std::string& tool_name) {
    return {};
}

} // namespace mcp
