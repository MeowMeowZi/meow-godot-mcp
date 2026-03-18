#include "mcp_tool_registry.h"

const std::vector<ToolDef>& get_all_tools() {
    static const std::vector<ToolDef> tools = {
        {
            "get_scene_tree",
            "Get the current scene tree structure including node names, types, paths, transform, visibility, and script info",
            {
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
            },
            {4, 3, 0}
        },
        {
            "create_node",
            "Create a new node in the scene tree. The node is added as a child of the specified parent with undo/redo support.",
            {
                {"type", "object"},
                {"properties", {
                    {"type", {{"type", "string"}, {"description", "Node class name (e.g., Sprite2D, CharacterBody3D, Node2D, Label)"}}},
                    {"parent_path", {{"type", "string"}, {"description", "Path to parent node relative to scene root. Empty string or omit for scene root."}}},
                    {"name", {{"type", "string"}, {"description", "Name for the new node. If omitted, uses the class name. Godot may auto-rename for uniqueness."}}},
                    {"properties", {{"type", "object"}, {"description", "Initial property values as key-value pairs. Keys are snake_case property names (e.g., position, visible, modulate). Values are strings auto-parsed to Godot types (e.g., 'Vector2(100,200)', '#ff0000', 'true')."}}}
                }},
                {"required", {"type"}}
            },
            {4, 3, 0}
        },
        {
            "set_node_property",
            "Set a property value on an existing node. Supports undo/redo. Property values are auto-parsed from strings to Godot types.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to the target node relative to scene root (e.g., 'Player', 'Player/Sprite2D')"}}},
                    {"property", {{"type", "string"}, {"description", "Property name in snake_case (e.g., position, rotation_degrees, visible, modulate, name)"}}},
                    {"value", {{"type", "string"}, {"description", "Property value as string. Auto-parsed: 'Vector2(100,200)', 'Color(1,0,0,1)', '#ff0000', '42', '3.14', 'true', 'false'"}}}
                }},
                {"required", {"node_path", "property", "value"}}
            },
            {4, 3, 0}
        },
        {
            "delete_node",
            "Delete a node from the scene tree. Cannot delete the scene root. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to the node to delete, relative to scene root"}}}
                }},
                {"required", {"node_path"}}
            },
            {4, 3, 0}
        },
        {
            "read_script",
            "Read the content of a GDScript file. Returns the full file content and line count.",
            {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Path to GDScript file (e.g., res://scripts/player.gd)"}}}
                }},
                {"required", {"path"}}
            },
            {4, 3, 0}
        },
        {
            "write_script",
            "Create a new GDScript file with the specified content. Errors if the file already exists. Use edit_script to modify existing files.",
            {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Path for the new GDScript file (e.g., res://scripts/player.gd)"}}},
                    {"content", {{"type", "string"}, {"description", "Full GDScript content to write"}}}
                }},
                {"required", {"path", "content"}}
            },
            {4, 3, 0}
        },
        {
            "edit_script",
            "Edit an existing GDScript file with line-level operations (insert, replace, delete). Line numbers are 1-based.",
            {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Path to GDScript file (e.g., res://scripts/player.gd)"}}},
                    {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}, {"description", "Line editing operation"}}},
                    {"line", {{"type", "integer"}, {"description", "1-based line number"}}},
                    {"content", {{"type", "string"}, {"description", "Content for insert/replace operations"}}},
                    {"end_line", {{"type", "integer"}, {"description", "End line for multi-line replace/delete (inclusive, 1-based). Defaults to same as line."}}}
                }},
                {"required", {"path", "operation", "line"}}
            },
            {4, 3, 0}
        },
        {
            "attach_script",
            "Attach an existing GDScript file to a scene node. Replaces any existing script. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to target node relative to scene root"}}},
                    {"script_path", {{"type", "string"}, {"description", "Path to .gd file (e.g., res://scripts/player.gd)"}}}
                }},
                {"required", {"node_path", "script_path"}}
            },
            {4, 3, 0}
        },
        {
            "detach_script",
            "Remove the script from a scene node. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to the node to detach script from"}}}
                }},
                {"required", {"node_path"}}
            },
            {4, 3, 0}
        },
        {
            "list_project_files",
            "List all files in the project directory (res://). Returns flat list with file paths and types.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0}
        },
        {
            "get_project_settings",
            "Read project.godot settings as structured data. Returns project configuration key-value pairs.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0}
        },
        {
            "get_resource_info",
            "Load and inspect a .tres or .res resource file. Returns resource type and property names/values. Read-only.",
            {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Path to resource file (e.g., res://theme.tres, res://material.res)"}}}
                }},
                {"required", {"path"}}
            },
            {4, 3, 0}
        },
        {
            "run_game",
            "Run the game in debug mode. Modes: 'main' (F5 - main scene), 'current' (F6 - current scene), 'custom' (specify scene_path). If game is already running, returns current status without restarting.",
            {
                {"type", "object"},
                {"properties", {
                    {"mode", {{"type", "string"}, {"enum", {"main", "current", "custom"}},
                              {"description", "Play mode: main scene, current scene, or custom scene"}}},
                    {"scene_path", {{"type", "string"},
                                   {"description", "Scene path for custom mode (e.g., res://levels/test.tscn)"}}}
                }},
                {"required", {"mode"}}
            },
            {4, 3, 0}
        },
        {
            "stop_game",
            "Stop the currently running game instance. Returns error if no game is running.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0}
        },
        {
            "get_game_output",
            "Get accumulated stdout/stderr log output from the running (or last-run) game. Returns lines written since last call by default. Use clear_after_read=false to keep reading from the same position.",
            {
                {"type", "object"},
                {"properties", {
                    {"clear_after_read", {{"type", "boolean"},
                                         {"description", "If true (default), advance read position so next call returns only new lines. If false, re-read from same position."}}}
                }},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0}
        },
        {
            "get_node_signals",
            "Get all signals defined on a node, including their parameters and current connections.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to the node relative to scene root"}}}
                }},
                {"required", {"node_path"}}
            },
            {4, 3, 0}
        },
        {
            "connect_signal",
            "Connect a signal from a source node to a method on a target node. Creates a signal connection in the scene.",
            {
                {"type", "object"},
                {"properties", {
                    {"source_path", {{"type", "string"}, {"description", "Path to source node (emits the signal)"}}},
                    {"signal_name", {{"type", "string"}, {"description", "Signal name to connect (e.g., 'pressed', 'body_entered')"}}},
                    {"target_path", {{"type", "string"}, {"description", "Path to target node (receives the callback)"}}},
                    {"method_name", {{"type", "string"}, {"description", "Method name on target node to call when signal fires"}}}
                }},
                {"required", {"source_path", "signal_name", "target_path", "method_name"}}
            },
            {4, 3, 0}
        },
        {
            "disconnect_signal",
            "Disconnect an existing signal connection between two nodes.",
            {
                {"type", "object"},
                {"properties", {
                    {"source_path", {{"type", "string"}, {"description", "Path to source node"}}},
                    {"signal_name", {{"type", "string"}, {"description", "Signal name to disconnect"}}},
                    {"target_path", {{"type", "string"}, {"description", "Path to target node"}}},
                    {"method_name", {{"type", "string"}, {"description", "Method name on target node"}}}
                }},
                {"required", {"source_path", "signal_name", "target_path", "method_name"}}
            },
            {4, 3, 0}
        }
    };
    return tools;
}

nlohmann::json get_filtered_tools_json(const GodotVersion& current) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& tool : get_all_tools()) {
        if (current >= tool.min_version) {
            result.push_back({
                {"name", tool.name},
                {"description", tool.description},
                {"inputSchema", tool.input_schema}
            });
        }
    }
    return result;
}

int get_tool_count(const GodotVersion& current) {
    int count = 0;
    for (const auto& tool : get_all_tools()) {
        if (current >= tool.min_version) {
            count++;
        }
    }
    return count;
}
