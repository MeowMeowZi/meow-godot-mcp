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
            {4, 3, 0}
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
            {4, 3, 0}
        },
        {
            "list_open_scenes",
            "List all currently open scenes in the editor. Returns file paths, titles, and which scene is active.",
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            },
            {4, 3, 0}
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
            {4, 3, 0}
        },
        {
            "instantiate_scene",
            "Instantiate a PackedScene (.tscn/.scn) as a child node in the current scene. Supports undo/redo. The instance root gets its owner set to the scene root.",
            {
                {"type", "object"},
                {"properties", {
                    {"scene_path", {{"type", "string"}, {"description", "Path to the scene file to instantiate (e.g., res://scenes/enemy.tscn)"}}},
                    {"parent_path", {{"type", "string"}, {"description", "Path to parent node relative to scene root. Empty or omit for scene root."}}},
                    {"name", {{"type", "string"}, {"description", "Override name for the instantiated node. Defaults to the scene file name if omitted."}}}
                }},
                {"required", {"scene_path"}}
            },
            {4, 3, 0}
        },
        // --- Phase 7: UI System tools ---
        {
            "set_layout_preset",
            "Set a Control node's layout preset (anchors and offsets). Presets: top_left, top_right, bottom_left, bottom_right, center_left, center_top, center_right, center_bottom, center, left_wide, top_wide, right_wide, bottom_wide, vcenter_wide, hcenter_wide, full_rect.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to Control node relative to scene root"}}},
                    {"preset", {{"type", "string"}, {"description", "Layout preset name (e.g., full_rect, center, top_wide)"}}}
                }},
                {"required", {"node_path", "preset"}}
            },
            {4, 3, 0}
        },
        {
            "set_theme_override",
            "Batch set theme overrides on a Control node. Supports colors (hex or Color()), font sizes (int), and constants. Override type is auto-detected from key names or can be specified explicitly.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to Control node relative to scene root"}}},
                    {"overrides", {{"type", "object"}, {"description", "Key-value pairs of theme overrides. Keys are theme override names (e.g., font_color, font_size). Values are strings: hex colors (#ff0000), integers (16), or Color() constructors."}}}
                }},
                {"required", {"node_path", "overrides"}}
            },
            {4, 3, 0}
        },
        {
            "create_stylebox",
            "Create a StyleBoxFlat resource and apply it as a theme override on a Control node. Supports bg_color, corner_radius, border_width, border_color, content_margin, shadow, and anti-aliasing properties.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to Control node relative to scene root"}}},
                    {"override_name", {{"type", "string"}, {"description", "Theme override name to apply the StyleBox to (e.g., panel, normal, hover, pressed, disabled, focus)"}}},
                    {"bg_color", {{"type", "string"}, {"description", "Background color as hex (#rrggbb or #rrggbbaa) or Color(r,g,b,a). Default: #999999"}}},
                    {"corner_radius", {{"type", "integer"}, {"description", "Corner radius applied to all 4 corners. Default: 0"}}},
                    {"corner_radius_top_left", {{"type", "integer"}, {"description", "Top-left corner radius (overrides corner_radius)"}}},
                    {"corner_radius_top_right", {{"type", "integer"}, {"description", "Top-right corner radius (overrides corner_radius)"}}},
                    {"corner_radius_bottom_right", {{"type", "integer"}, {"description", "Bottom-right corner radius (overrides corner_radius)"}}},
                    {"corner_radius_bottom_left", {{"type", "integer"}, {"description", "Bottom-left corner radius (overrides corner_radius)"}}},
                    {"border_width", {{"type", "integer"}, {"description", "Border width applied to all 4 sides. Default: 0"}}},
                    {"border_color", {{"type", "string"}, {"description", "Border color as hex or Color(). Default: #cccccc"}}},
                    {"content_margin", {{"type", "number"}, {"description", "Content margin applied to all 4 sides. Default: -1 (auto)"}}},
                    {"shadow_color", {{"type", "string"}, {"description", "Shadow color as hex or Color()"}}},
                    {"shadow_size", {{"type", "integer"}, {"description", "Shadow size in pixels. Default: 0"}}},
                    {"anti_aliased", {{"type", "boolean"}, {"description", "Enable anti-aliasing. Default: true"}}}
                }},
                {"required", {"node_path", "override_name"}}
            },
            {4, 3, 0}
        },
        {
            "get_ui_properties",
            "Query a Control node's UI-specific properties: anchors, offsets, size flags, minimum size, pivot offset, grow direction, focus neighbors, and layout direction. For focus neighbor setting, use set_node_property with focus_neighbor_left/top/right/bottom.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to Control node relative to scene root"}}}
                }},
                {"required", {"node_path"}}
            },
            {4, 3, 0}
        },
        {
            "set_container_layout",
            "Configure a Container node's layout parameters. For BoxContainer: alignment and separation. For GridContainer: columns and h/v separation. Optionally batch-set all direct children's size_flags.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to Container node relative to scene root"}}},
                    {"separation", {{"type", "integer"}, {"description", "Space between children in pixels (BoxContainer/GridContainer)"}}},
                    {"h_separation", {{"type", "integer"}, {"description", "Horizontal separation for GridContainer"}}},
                    {"v_separation", {{"type", "integer"}, {"description", "Vertical separation for GridContainer"}}},
                    {"alignment", {{"type", "string"}, {"description", "BoxContainer alignment: begin, center, or end"}}},
                    {"columns", {{"type", "integer"}, {"description", "Number of columns for GridContainer"}}},
                    {"child_size_flags_horizontal", {{"type", "string"}, {"description", "Batch-set all direct children's horizontal size flags: shrink_begin, fill, expand, expand_fill, shrink_center, shrink_end"}}},
                    {"child_size_flags_vertical", {{"type", "string"}, {"description", "Batch-set all direct children's vertical size flags: shrink_begin, fill, expand, expand_fill, shrink_center, shrink_end"}}}
                }},
                {"required", {"node_path"}}
            },
            {4, 3, 0}
        },
        {
            "get_theme_overrides",
            "Query all theme overrides currently set on a Control node, categorized by type (colors, font_sizes, constants, styles). Returns only overrides that have been explicitly set, not inherited values.",
            {
                {"type", "object"},
                {"properties", {
                    {"node_path", {{"type", "string"}, {"description", "Path to Control node relative to scene root"}}}
                }},
                {"required", {"node_path"}}
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
