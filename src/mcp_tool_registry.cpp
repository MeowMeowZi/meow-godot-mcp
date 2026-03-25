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
            {4, 3, 0},
            ToolCategory::SCENE
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
            {4, 3, 0},
            ToolCategory::SCENE
        },
        {
            "set_node_property",
            "Set a property value on an existing node. Supports undo/redo. Property values are auto-parsed from strings to Godot types.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to the target node relative to scene root (e.g., 'Player', 'Player/Sprite2D'). Use '' or '.' for the scene root itself."}}},
                    {"property", {{"type", "string"}, {"description", "Property name in snake_case (e.g., position, rotation_degrees, visible, modulate, name)"}}},
                    {"value", {{"type", "string"}, {"description", "Property value as string. Auto-parsed: 'Vector2(100,200)', 'Color(1,0,0,1)', '#ff0000', '42', '3.14', 'true', 'false'. "
                                                                  "For resource properties: 'res://path/to/resource.png' loads from disk. "
                                                                  "'new:ClassName(prop=val)' creates inline (e.g., 'new:RectangleShape2D(size=Vector2(100,50))', 'new:CircleShape2D(radius=25)')."}}}
                }},
                {"required", {"node_path", "property", "value"}}
            },
            {4, 3, 0},
            ToolCategory::SCENE
        },
        {
            "delete_node",
            "Delete a node from the scene tree. Cannot delete the scene root. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to the node to delete, relative to scene root. Use '' or '.' for the scene root itself."}}}
                }},
                {"required", {"node_path"}}
            },
            {4, 3, 0},
            ToolCategory::SCENE
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
            {4, 3, 0},
            ToolCategory::SCRIPT
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
            {4, 3, 0},
            ToolCategory::SCRIPT
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
            {4, 3, 0},
            ToolCategory::SCRIPT
        },
        {
            "attach_script",
            "Attach an existing GDScript file to a scene node. Replaces any existing script. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to target node relative to scene root. Use '' or '.' for the scene root itself."}}},
                    {"script_path", {{"type", "string"}, {"description", "Path to .gd file (e.g., res://scripts/player.gd)"}}}
                }},
                {"required", {"node_path", "script_path"}}
            },
            {4, 3, 0},
            ToolCategory::SCRIPT
        },
        {
            "list_project_files",
            "List all files in the project directory (res://). Returns flat list with file paths and types.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::PROJECT
        },
        {
            "run_game",
            "Run the game in debug mode. Modes: 'main' (F5 - main scene), 'current' (F6 - current scene), 'custom' (specify scene_path). If game is already running, returns current status without restarting. With wait_for_bridge=true, the response is deferred until the game's MCP bridge connects (or timeout). This eliminates the need to manually poll get_game_bridge_status.",
            {
                {"type", "object"},
                {"properties", {
                    {"mode", {{"type", "string"}, {"enum", {"main", "current", "custom"}},
                              {"description", "Play mode: main scene, current scene, or custom scene"}}},
                    {"scene_path", {{"type", "string"},
                                   {"description", "Scene path for custom mode (e.g., res://levels/test.tscn)"}}},
                    {"wait_for_bridge", {{"type", "boolean"},
                                        {"description", "If true, wait for the game bridge to connect before returning. Default: false"}}},
                    {"timeout", {{"type", "integer"},
                                {"description", "Timeout in milliseconds for wait_for_bridge. Default: 10000 (10 seconds)"}}}
                }},
                {"required", {"mode"}}
            },
            {4, 3, 0},
            ToolCategory::RUNTIME
        },
        {
            "stop_game",
            "Stop the currently running game instance. Returns error if no game is running.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::RUNTIME
        },
        {
            "get_game_output",
            "Get accumulated stdout/stderr output from the running game. Captures print(), push_error(), "
            "and push_warning() output automatically via the debugger channel -- no project settings needed. "
            "Returns lines since last call by default. Supports filtering by level, time, and keyword.",
            {
                {"type", "object"},
                {"properties", {
                    {"clear_after_read", {{"type", "boolean"},
                                         {"description", "If true (default), advance read position so next call returns only new lines. If false, re-read from same position."}}},
                    {"level", {{"type", "string"},
                              {"enum", {"info", "warning", "error"}},
                              {"description", "Filter by log level. Omit to return all levels."}}},
                    {"since", {{"type", "integer"},
                              {"description", "Return only entries with timestamp >= this value (milliseconds). Useful for time-windowed queries."}}},
                    {"keyword", {{"type", "string"},
                                {"description", "Filter lines containing this substring (case-sensitive). Omit to return all lines."}}}
                }},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::RUNTIME
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
            {4, 3, 0},
            ToolCategory::QUERY
        },
        // --- Phase 6: Scene File Management tools ---
        {
            "save_scene",
            "Save the current scene to disk. Without path: overwrites current file (Ctrl+S). With path: saves to new location (Ctrl+Shift+S). Returns error if scene has no file path and no path is provided.",
            {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Optional file path (e.g., res://scenes/level.tscn). Omit to overwrite current file. Extension determines format: .tscn (text) or .scn (binary)."}}}
                }},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::PROJECT
        },
        {
            "open_scene",
            "Open an existing scene file in the editor. Adds a new tab without closing the current scene. The opened scene becomes the active edited scene.",
            {
                {"type", "object"},
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "Path to scene file (e.g., res://scenes/level.tscn)"}}}
                }},
                {"required", {"path"}}
            },
            {4, 3, 0},
            ToolCategory::PROJECT
        },
        {
            "create_scene",
            "Create a new scene with the specified root node type, save it to disk, and open it in the editor. The root node class must be a Node subclass.",
            {
                {"type", "object"},
                {"properties", {
                    {"root_type", {{"type", "string"}, {"description", "Root node class name (e.g., Node2D, Node3D, Control, CharacterBody2D)"}}},
                    {"path", {{"type", "string"}, {"description", "File path to save the scene (e.g., res://scenes/player.tscn)"}}},
                    {"root_name", {{"type", "string"}, {"description", "Name for the root node. Defaults to the class name if omitted."}}}
                }},
                {"required", {"root_type", "path"}}
            },
            {4, 3, 0},
            ToolCategory::PROJECT
        },
        // --- Phase 9: Viewport Screenshot tools ---
        {
            "capture_viewport",
            "Capture a screenshot of the editor 2D or 3D viewport. Returns the image as base64-encoded PNG via MCP ImageContent. The AI client renders the image natively.",
            {
                {"type", "object"},
                {"properties", {
                    {"viewport_type", {
                        {"type", "string"},
                        {"enum", {"2d", "3d"}},
                        {"description", "Which viewport to capture: 2d or 3d (default: 2d)"}
                    }},
                    {"width", {
                        {"type", "integer"},
                        {"description", "Optional output width in pixels. Scales the image. Omit for original viewport resolution."}
                    }},
                    {"height", {
                        {"type", "integer"},
                        {"description", "Optional output height in pixels. Scales the image. Omit for original viewport resolution."}
                    }}
                }},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::RUNTIME
        },
        // --- Phase 10: Running Game Bridge tools ---
        {
            "inject_input",
            "Inject input events into a running game. Supports keyboard keys, mouse events, and input actions. Requires a game to be running with the MCP bridge connected.",
            {
                {"type", "object"},
                {"properties", {
                    {"type", {
                        {"type", "string"},
                        {"enum", {"key", "mouse", "action"}},
                        {"description", "Input type to inject"}
                    }},
                    {"keycode", {
                        {"type", "string"},
                        {"description", "Key name for type=key (e.g. 'W', 'space', 'escape', 'enter'). Uses Godot key names."}
                    }},
                    {"pressed", {
                        {"type", "boolean"},
                        {"description", "Whether the key/action is pressed (true) or released (false). Default: true"}
                    }},
                    {"action_name", {
                        {"type", "string"},
                        {"description", "Action map name for type=action (e.g. 'ui_accept', 'move_left')"}
                    }},
                    {"mouse_action", {
                        {"type", "string"},
                        {"enum", {"move", "click", "scroll"}},
                        {"description", "Mouse action for type=mouse"}
                    }},
                    {"position", {
                        {"type", "object"},
                        {"properties", {
                            {"x", {{"type", "number"}}},
                            {"y", {{"type", "number"}}}
                        }},
                        {"description", "Mouse position in viewport coordinates for type=mouse"}
                    }},
                    {"button", {
                        {"type", "string"},
                        {"enum", {"left", "right", "middle"}},
                        {"description", "Mouse button for type=mouse click. Default: left"}
                    }},
                    {"direction", {
                        {"type", "string"},
                        {"enum", {"up", "down"}},
                        {"description", "Scroll direction for type=mouse scroll"}
                    }}
                }},
                {"required", {"type"}}
            },
            {4, 3, 0},
            ToolCategory::INPUT
        },
        {
            "validate_scripts",
            "Validate all GDScript files in the project for syntax errors. Scans res:// recursively (excluding addons/). Returns per-file status and error count. Use before run_game to catch script errors early.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::DX
        },
        {
            "create_node_tree",
            "Create an arbitrary node tree from a declarative JSON specification in one atomic operation. Supports any node type with properties and nested children. Ctrl+Z undoes the entire creation.",
            {
                {"type", "object"},
                {"properties", {
                    {"spec", {
                        {"type", "object"},
                        {"description", "Node tree spec: {\"type\": \"Node2D\", \"name\": \"MyNode\", \"properties\": {\"position\": \"Vector2(100,200)\"}, \"children\": [{\"type\": \"Sprite2D\", ...}]}. Supports unlimited nesting."}
                    }},
                    {"parent_path", {
                        {"type", "string"},
                        {"description", "Path to parent node. Empty or omit for scene root."}
                    }}
                }},
                {"required", {"spec"}}
            },
            {4, 3, 0},
            ToolCategory::COMPOSITE
        },
        {
            "inject_input_sequence",
            "Execute a sequence of input steps with timing in the running game. Each step can be an action press+hold+release, key press, mouse event, or wait. Steps execute sequentially with specified durations. Requires a game to be running with the MCP bridge connected.",
            {
                {"type", "object"},
                {"properties", {
                    {"steps", {
                        {"type", "array"},
                        {"description", "Array of input steps. Each step: {\"action\": \"ui_right\", \"duration\": 1000} (hold action for ms), {\"key\": \"W\", \"duration\": 500} (hold key), {\"mouse\": \"click\", \"position\": {\"x\": 100, \"y\": 200}} (click), {\"wait\": 300} (delay ms). Duration defaults to 50ms if omitted."},
                        {"items", {{"type", "object"}}}
                    }}
                }},
                {"required", {"steps"}}
            },
            {4, 3, 0},
            ToolCategory::INPUT
        },
        {
            "inject_text",
            "Set text content on a LineEdit or TextEdit node in the running game. Directly sets the text property and emits the appropriate change signal. Requires a game to be running with the MCP bridge connected.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {
                        {"type", "string"},
                        {"description", "Path to the LineEdit or TextEdit node relative to scene root (e.g., 'UI/NameInput')"}
                    }},
                    {"text", {
                        {"type", "string"},
                        {"description", "Text content to set on the node"}
                    }}
                }},
                {"required", {"node_path", "text"}}
            },
            {4, 3, 0},
            ToolCategory::INPUT
        },
        {
            "capture_game_viewport",
            "Capture a screenshot of the running game's viewport. Returns the image as base64-encoded PNG via MCP ImageContent. Requires a game to be running with the MCP bridge connected.",
            {
                {"type", "object"},
                {"properties", {
                    {"width", {
                        {"type", "integer"},
                        {"description", "Optional output width in pixels. Omit for original game resolution."}
                    }},
                    {"height", {
                        {"type", "integer"},
                        {"description", "Optional output height in pixels. Omit for original game resolution."}
                    }}
                }},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::RUNTIME
        },
        // --- Phase 13: Runtime State Query tools ---
        {
            "get_game_node_property",
            "Read a property value from a node in the running game. Returns the value "
            "as a string (using Godot var_to_str format) and the property type name. "
            "Requires a game to be running with the MCP bridge connected.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {
                        {"type", "string"},
                        {"description", "Path to the node relative to the scene root "
                                       "(e.g., 'Player', 'UI/ScoreLabel'). Empty string for scene root."}
                    }},
                    {"property", {
                        {"type", "string"},
                        {"description", "Property name to read (e.g., 'position', 'text', 'visible', 'health')"}
                    }}
                }},
                {"required", {"node_path", "property"}}
            },
            {4, 3, 0},
            ToolCategory::QUERY
        },
        {
            "eval_in_game",
            "Execute a GDScript expression in the running game and return the result. "
            "The expression runs with the current scene root as the base instance, so "
            "methods like get_children(), get_node() etc. are available. "
            "Returns the result as a string (using Godot var_to_str format). "
            "Requires a game to be running with the MCP bridge connected.",
            {
                {"type", "object"},
                {"properties", {
                    {"expression", {
                        {"type", "string"},
                        {"description", "GDScript expression to evaluate (e.g., 'get_children().size()', "
                                       "'get_node(\"Player\").position', '2 + 2')"}
                    }}
                }},
                {"required", {"expression"}}
            },
            {4, 3, 0},
            ToolCategory::QUERY
        },
        // --- Phase 20: TileMap Operations ---
        {
            "set_tilemap_cells",
            "Batch-place tiles on a TileMapLayer at specified grid coordinates. "
            "The TileMapLayer must have a TileSet assigned. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to TileMapLayer node relative to scene root"}}},
                    {"cells", {
                        {"type", "array"},
                        {"description", "Array of tiles to place"},
                        {"items", {
                            {"type", "object"},
                            {"properties", {
                                {"x", {{"type", "integer"}, {"description", "Grid x coordinate"}}},
                                {"y", {{"type", "integer"}, {"description", "Grid y coordinate"}}},
                                {"source_id", {{"type", "integer"}, {"description", "TileSet source ID (usually 0 for the first atlas)"}}},
                                {"atlas_x", {{"type", "integer"}, {"description", "Atlas tile x coordinate within the source"}}},
                                {"atlas_y", {{"type", "integer"}, {"description", "Atlas tile y coordinate within the source"}}},
                                {"alternative_tile", {{"type", "integer"}, {"description", "Alternative tile ID (default: 0)"}}}
                            }},
                            {"required", {"x", "y", "source_id", "atlas_x", "atlas_y"}}
                        }}
                    }}
                }},
                {"required", {"node_path", "cells"}}
            },
            {4, 3, 0},
            ToolCategory::TILEMAP
        },
        {
            "erase_tilemap_cells",
            "Batch-erase tiles from a TileMapLayer at specified grid coordinates. "
            "Already-empty cells are skipped. Supports undo/redo.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to TileMapLayer node relative to scene root"}}},
                    {"coords", {
                        {"type", "array"},
                        {"description", "Array of grid coordinates to erase"},
                        {"items", {
                            {"type", "object"},
                            {"properties", {
                                {"x", {{"type", "integer"}, {"description", "Grid x coordinate"}}},
                                {"y", {{"type", "integer"}, {"description", "Grid y coordinate"}}}
                            }},
                            {"required", {"x", "y"}}
                        }}
                    }}
                }},
                {"required", {"node_path", "coords"}}
            },
            {4, 3, 0},
            ToolCategory::TILEMAP
        },
        {
            "batch_set_property",
            "Set a property on multiple nodes in one atomic operation. Specify nodes by explicit paths or type filter. "
            "Ctrl+Z undoes all changes at once.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_paths", {{"type", "array"}, {"items", {{"type", "string"}}},
                        {"description", "Explicit list of node paths to modify"}}},
                    {"type_filter", {{"type", "string"}, {"description", "Apply to all nodes of this type (e.g., 'Label', 'Sprite2D')"}}},
                    {"property", {{"type", "string"}, {"description", "Property name to set (e.g., 'visible', 'modulate')"}}},
                    {"value", {{"type", "string"}, {"description", "Property value as string (auto-parsed: 'Vector2(100,200)', '#ff0000', 'true')"}}}
                }},
                {"required", {"property", "value"}}
            },
            {4, 3, 0},
            ToolCategory::COMPOSITE
        },
        {
            "duplicate_node",
            "Deep-copy a node and its entire subtree to a new parent. Preserves all children, properties, and script references. "
            "Single undo step.",
            {
                {"type", "object"},
                {"properties", {
                    {"source_path", {{"type", "string"}, {"description", "Path to the node to duplicate"}}},
                    {"target_parent_path", {{"type", "string"}, {"description", "Path to new parent (default: same parent as source)"}}},
                    {"new_name", {{"type", "string"}, {"description", "Name for the duplicated node (default: Godot auto-names with suffix)"}}}
                }},
                {"required", {"source_path"}}
            },
            {4, 3, 0},
            ToolCategory::COMPOSITE
        },
        // --- Restart Editor ---
        {
            "restart_editor",
            "Restart the Godot editor. Useful after recompiling GDExtension plugins. "
            "Saves the project before restarting by default.",
            {
                {"type", "object"},
                {"properties", {
                    {"save", {{"type", "boolean"}, {"description", "Save the project before restarting. Default: true"}}}
                }},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0},
            ToolCategory::DX
        }
    };
    return tools;
}

// --- Disabled tools set ---
static std::set<std::string> s_disabled_tools;

void set_tool_disabled(const std::string& name, bool disabled) {
    if (disabled) {
        s_disabled_tools.insert(name);
    } else {
        s_disabled_tools.erase(name);
    }
}

bool is_tool_disabled(const std::string& name) {
    return s_disabled_tools.count(name) > 0;
}

const std::set<std::string>& get_disabled_tools() {
    return s_disabled_tools;
}

// --- Category display names ---
const char* get_category_name(ToolCategory cat) {
    switch (cat) {
        case ToolCategory::SCENE:     return "Scene";
        case ToolCategory::SCRIPT:    return "Script";
        case ToolCategory::PROJECT:   return "Project";
        case ToolCategory::RUNTIME:   return "Runtime";
        case ToolCategory::INPUT:     return "Input";
        case ToolCategory::QUERY:     return "Query";
        case ToolCategory::TILEMAP:   return "TileMap";
        case ToolCategory::COMPOSITE: return "Composite";
        case ToolCategory::DX:        return "DX";
        default:                      return "Other";
    }
}

nlohmann::json get_filtered_tools_json(const GodotVersion& current) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& tool : get_all_tools()) {
        if (current >= tool.min_version && !is_tool_disabled(tool.name)) {
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
        if (current >= tool.min_version && !is_tool_disabled(tool.name)) {
            count++;
        }
    }
    return count;
}
