#include "mcp_protocol.h"

namespace mcp {

ParseResult parse_jsonrpc(const std::string& json_str) {
    nlohmann::json doc;

    // Try to parse JSON
    try {
        doc = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error&) {
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
                {"tools", {{"listChanged", false}}}
            }},
            {"serverInfo", {
                {"name", "godot-mcp-meow"},
                {"version", "0.1.0"}
            }}
        }}
    };
}

nlohmann::json create_tools_list_response(const nlohmann::json& id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"tools", {
                {
                    {"name", "get_scene_tree"},
                    {"description", "Get the current scene tree structure including node names, types, paths, transform, visibility, and script info"},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"max_depth", {
                                {"type", "integer"},
                                {"description", "Maximum depth to traverse (default: unlimited)"}
                            }},
                            {"include_properties", {
                                {"type", "boolean"},
                                {"description", "Include transform, visible, has_script, script_path per node (default: true)"}
                            }},
                            {"root_path", {
                                {"type", "string"},
                                {"description", "Node path to start traversal from (default: scene root)"}
                            }}
                        }},
                        {"required", nlohmann::json::array()}
                    }}
                }
            }}
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

} // namespace mcp
