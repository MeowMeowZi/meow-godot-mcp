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
                },
                {
                    {"name", "create_node"},
                    {"description", "Create a new node in the scene tree. The node is added as a child of the specified parent with undo/redo support."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"type", {{"type", "string"}, {"description", "Node class name (e.g., Sprite2D, CharacterBody3D, Node2D, Label)"}}},
                            {"parent_path", {{"type", "string"}, {"description", "Path to parent node relative to scene root. Empty string or omit for scene root."}}},
                            {"name", {{"type", "string"}, {"description", "Name for the new node. If omitted, uses the class name. Godot may auto-rename for uniqueness."}}},
                            {"properties", {{"type", "object"}, {"description", "Initial property values as key-value pairs. Keys are snake_case property names (e.g., position, visible, modulate). Values are strings auto-parsed to Godot types (e.g., 'Vector2(100,200)', '#ff0000', 'true')."}}}
                        }},
                        {"required", {"type"}}
                    }}
                },
                {
                    {"name", "set_node_property"},
                    {"description", "Set a property value on an existing node. Supports undo/redo. Property values are auto-parsed from strings to Godot types."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"node_path", {{"type", "string"}, {"description", "Path to the target node relative to scene root (e.g., 'Player', 'Player/Sprite2D')"}}},
                            {"property", {{"type", "string"}, {"description", "Property name in snake_case (e.g., position, rotation_degrees, visible, modulate, name)"}}},
                            {"value", {{"type", "string"}, {"description", "Property value as string. Auto-parsed: 'Vector2(100,200)', 'Color(1,0,0,1)', '#ff0000', '42', '3.14', 'true', 'false'"}}}
                        }},
                        {"required", {"node_path", "property", "value"}}
                    }}
                },
                {
                    {"name", "delete_node"},
                    {"description", "Delete a node from the scene tree. Cannot delete the scene root. Supports undo/redo."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"node_path", {{"type", "string"}, {"description", "Path to the node to delete, relative to scene root"}}}
                        }},
                        {"required", {"node_path"}}
                    }}
                },
                {
                    {"name", "read_script"},
                    {"description", "Read the content of a GDScript file. Returns the full file content and line count."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"path", {{"type", "string"}, {"description", "Path to GDScript file (e.g., res://scripts/player.gd)"}}}
                        }},
                        {"required", {"path"}}
                    }}
                },
                {
                    {"name", "write_script"},
                    {"description", "Create a new GDScript file with the specified content. Errors if the file already exists. Use edit_script to modify existing files."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"path", {{"type", "string"}, {"description", "Path for the new GDScript file (e.g., res://scripts/player.gd)"}}},
                            {"content", {{"type", "string"}, {"description", "Full GDScript content to write"}}}
                        }},
                        {"required", {"path", "content"}}
                    }}
                },
                {
                    {"name", "edit_script"},
                    {"description", "Edit an existing GDScript file with line-level operations (insert, replace, delete). Line numbers are 1-based."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"path", {{"type", "string"}, {"description", "Path to GDScript file (e.g., res://scripts/player.gd)"}}},
                            {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}, {"description", "Line editing operation"}}},
                            {"line", {{"type", "integer"}, {"description", "1-based line number"}}},
                            {"content", {{"type", "string"}, {"description", "Content for insert/replace operations"}}},
                            {"end_line", {{"type", "integer"}, {"description", "End line for multi-line replace/delete (inclusive, 1-based). Defaults to same as line."}}}
                        }},
                        {"required", {"path", "operation", "line"}}
                    }}
                },
                {
                    {"name", "attach_script"},
                    {"description", "Attach an existing GDScript file to a scene node. Replaces any existing script. Supports undo/redo."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"node_path", {{"type", "string"}, {"description", "Path to target node relative to scene root"}}},
                            {"script_path", {{"type", "string"}, {"description", "Path to .gd file (e.g., res://scripts/player.gd)"}}}
                        }},
                        {"required", {"node_path", "script_path"}}
                    }}
                },
                {
                    {"name", "detach_script"},
                    {"description", "Remove the script from a scene node. Supports undo/redo."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"node_path", {{"type", "string"}, {"description", "Path to the node to detach script from"}}}
                        }},
                        {"required", {"node_path"}}
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
