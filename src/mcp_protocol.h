#ifndef MEOW_GODOT_MCP_MCP_PROTOCOL_H
#define MEOW_GODOT_MCP_MCP_PROTOCOL_H

// Pure C++17 + nlohmann/json -- NO Godot headers
// This allows unit testing without godot-cpp dependency

#include <nlohmann/json.hpp>
#include <string>

struct GodotVersion;

namespace mcp {

// JSON-RPC 2.0 standard error codes
enum JsonRpcError {
    PARSE_ERROR      = -32700,  // Invalid JSON
    INVALID_REQUEST  = -32600,  // Not a valid JSON-RPC request
    METHOD_NOT_FOUND = -32601,  // Method does not exist
    INVALID_PARAMS   = -32602,  // Invalid method parameters
    INTERNAL_ERROR   = -32603,  // Internal server error
};

// Parsed JSON-RPC message
struct JsonRpcMessage {
    std::string method;
    nlohmann::json id;          // int, string, or null for notifications
    nlohmann::json params;
    bool is_notification;       // true when id is absent
};

// Non-throwing parse result
struct ParseResult {
    bool success;
    JsonRpcMessage message;
    nlohmann::json error_response;  // Pre-built error JSON if success==false
};

// Parse a JSON-RPC 2.0 request string
ParseResult parse_jsonrpc(const std::string& json_str);

// MCP message builders (spec 2025-03-26)
nlohmann::json create_initialize_response(const nlohmann::json& id);
nlohmann::json create_tools_list_response(const nlohmann::json& id);
nlohmann::json create_tools_list_response(const nlohmann::json& id, const GodotVersion& version);
nlohmann::json create_tool_result(const nlohmann::json& id, const nlohmann::json& content_data);
nlohmann::json create_error_response(const nlohmann::json& id, int code, const std::string& message);
nlohmann::json create_tool_not_found_error(const nlohmann::json& id, const std::string& tool_name);

// MCP Resources protocol builders
nlohmann::json create_resources_list_response(const nlohmann::json& id, const nlohmann::json& resources);
nlohmann::json create_resource_read_response(const nlohmann::json& id, const nlohmann::json& contents);

// MCP Prompts protocol builders
nlohmann::json create_prompts_list_response(const nlohmann::json& id);
nlohmann::json create_prompt_get_response(const nlohmann::json& id, const std::string& description, const nlohmann::json& messages);
nlohmann::json create_prompt_not_found_error(const nlohmann::json& id, const std::string& prompt_name);

// MCP ImageContent tool result builder (for viewport screenshots)
nlohmann::json create_image_tool_result(const nlohmann::json& id,
                                         const std::string& base64_data,
                                         const std::string& mime_type,
                                         const nlohmann::json& metadata = nlohmann::json());

} // namespace mcp

#endif // MEOW_GODOT_MCP_MCP_PROTOCOL_H
