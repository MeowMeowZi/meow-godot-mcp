#include "mcp_protocol.h"
#include "mcp_tool_registry.h"
#include "mcp_prompts.h"

namespace mcp {

ParseResult parse_jsonrpc(const std::string& json_str) {
    nlohmann::json doc;

    // Try to parse JSON (no exceptions -- godot-cpp disables them on non-MSVC)
    doc = nlohmann::json::parse(json_str, nullptr, false);
    if (doc.is_discarded()) {
        return {
            false,
            {},
            {
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {
                    {"code", PARSE_ERROR},
                    {"message", "Parse error: invalid JSON"}
                }}
            }
        };
    }

    // Validate: must have "method" field (string)
    if (!doc.contains("method") || !doc["method"].is_string()) {
        nlohmann::json id = doc.contains("id") ? doc["id"] : nlohmann::json(nullptr);
        return {
            false,
            {},
            {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"error", {
                    {"code", INVALID_REQUEST},
                    {"message", "Invalid request: missing or non-string 'method' field"}
                }}
            }
        };
    }

    JsonRpcMessage msg;
    msg.method = doc["method"].get<std::string>();

    // id is optional -- absence means notification
    if (doc.contains("id")) {
        msg.id = doc["id"];
        msg.is_notification = false;
    } else {
        msg.id = nullptr;
        msg.is_notification = true;
    }

    // params is optional
    if (doc.contains("params")) {
        msg.params = doc["params"];
    } else {
        msg.params = nullptr;
    }

    return {true, msg, {}};
}

nlohmann::json create_initialize_response(const nlohmann::json& id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", {
                {"tools", {{"listChanged", false}}},
                {"prompts", {{"listChanged", false}}}
            }},
            {"serverInfo", {
                {"name", "meow-godot-mcp"},
                {"version", "0.2.0"}
            }}
        }}
    };
}

nlohmann::json create_tools_list_response(const nlohmann::json& id) {
    // Backward-compatible overload: returns all tools (permissive version)
    return create_tools_list_response(id, GodotVersion{99, 99, 99});
}

nlohmann::json create_tools_list_response(const nlohmann::json& id, const GodotVersion& version) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"tools", get_filtered_tools_json(version)}
        }}
    };
}

nlohmann::json create_tool_result(const nlohmann::json& id, const nlohmann::json& content_data) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", {
                {
                    {"type", "text"},
                    {"text", content_data.dump()}
                }
            }},
            {"isError", false}
        }}
    };
}

nlohmann::json create_error_response(const nlohmann::json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}

nlohmann::json create_tool_not_found_error(const nlohmann::json& id, const std::string& tool_name) {
    return create_error_response(id, INVALID_PARAMS, "Unknown tool: " + tool_name);
}

nlohmann::json create_resources_list_response(const nlohmann::json& id, const nlohmann::json& resources) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"resources", resources}}}};
}

nlohmann::json create_resource_read_response(const nlohmann::json& id, const nlohmann::json& contents) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"contents", contents}}}};
}

nlohmann::json create_prompts_list_response(const nlohmann::json& id) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"prompts", get_all_prompts_json()}}}};
}

nlohmann::json create_prompt_get_response(const nlohmann::json& id, const std::string& description, const nlohmann::json& messages) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"description", description}, {"messages", messages}}}};
}

nlohmann::json create_prompt_not_found_error(const nlohmann::json& id, const std::string& prompt_name) {
    return create_error_response(id, INVALID_PARAMS, "Prompt not found: " + prompt_name);
}

nlohmann::json create_image_tool_result(const nlohmann::json& id,
                                         const std::string& base64_data,
                                         const std::string& mime_type,
                                         const nlohmann::json& metadata) {
    nlohmann::json content_array = nlohmann::json::array();

    // Primary: MCP ImageContent item
    content_array.push_back({
        {"type", "image"},
        {"data", base64_data},
        {"mimeType", mime_type}
    });

    // Optional: TextContent with metadata (viewport_type, dimensions, etc.)
    if (!metadata.is_null() && !metadata.empty()) {
        content_array.push_back({
            {"type", "text"},
            {"text", metadata.dump()}
        });
    }

    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", content_array},
            {"isError", false}
        }}
    };
}

} // namespace mcp
