#include <gtest/gtest.h>
#include "mcp_tool_registry.h"

// --- GodotVersion comparison tests ---

TEST(GodotVersion, EqualVersions) {
    GodotVersion a{4, 3, 0};
    GodotVersion b{4, 3, 0};
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(b >= a);
    EXPECT_TRUE(a == b);
}

TEST(GodotVersion, GreaterMajor) {
    GodotVersion a{5, 0, 0};
    GodotVersion b{4, 3, 0};
    EXPECT_TRUE(a >= b);
    EXPECT_FALSE(b >= a);
}

TEST(GodotVersion, LesserMajor) {
    GodotVersion a{3, 9, 9};
    GodotVersion b{4, 0, 0};
    EXPECT_FALSE(a >= b);
    EXPECT_TRUE(b >= a);
}

TEST(GodotVersion, GreaterMinor) {
    GodotVersion a{4, 4, 0};
    GodotVersion b{4, 3, 0};
    EXPECT_TRUE(a >= b);
    EXPECT_FALSE(b >= a);
}

TEST(GodotVersion, LesserMinor) {
    GodotVersion a{4, 2, 0};
    GodotVersion b{4, 3, 0};
    EXPECT_FALSE(a >= b);
}

TEST(GodotVersion, GreaterPatch) {
    GodotVersion a{4, 3, 1};
    GodotVersion b{4, 3, 0};
    EXPECT_TRUE(a >= b);
    EXPECT_FALSE(b >= a);
}

TEST(GodotVersion, LesserPatch) {
    GodotVersion a{4, 3, 0};
    GodotVersion b{4, 3, 1};
    EXPECT_FALSE(a >= b);
}

// --- Tool registry tests ---

TEST(ToolRegistry, HasExactly38Tools) {
    const auto& tools = get_all_tools();
    ASSERT_EQ(tools.size(), 44);
}

TEST(ToolRegistry, EachToolHasNonEmptyFields) {
    const auto& tools = get_all_tools();
    for (const auto& tool : tools) {
        EXPECT_FALSE(tool.name.empty()) << "Tool has empty name";
        EXPECT_FALSE(tool.description.empty()) << "Tool " << tool.name << " has empty description";
        EXPECT_TRUE(tool.input_schema.is_object()) << "Tool " << tool.name << " has non-object input_schema";
        EXPECT_EQ(tool.input_schema["type"], "object") << "Tool " << tool.name << " input_schema type is not 'object'";
    }
}

TEST(ToolRegistry, AllToolsRequireVersion430) {
    const auto& tools = get_all_tools();
    GodotVersion expected{4, 3, 0};
    for (const auto& tool : tools) {
        EXPECT_EQ(tool.min_version, expected)
            << "Tool " << tool.name << " has unexpected min_version";
    }
}

TEST(ToolRegistry, ToolNamesAreCorrect) {
    const auto& tools = get_all_tools();
    std::vector<std::string> expected_names = {
        "get_scene_tree", "create_node", "set_node_property", "delete_node",
        "read_script", "write_script", "edit_script", "attach_script",
        "detach_script", "list_project_files", "get_project_settings", "get_resource_info",
        "run_game", "stop_game", "get_game_output",
        "get_node_signals", "connect_signal", "disconnect_signal",
        "save_scene", "open_scene", "list_open_scenes", "create_scene", "instantiate_scene",
        "set_layout_preset", "set_theme_override", "create_stylebox", "get_ui_properties", "set_container_layout", "get_theme_overrides",
        "create_animation", "add_animation_track", "set_keyframe", "get_animation_info", "set_animation_properties",
        "capture_viewport",
        "inject_input", "capture_game_viewport", "get_game_bridge_status",
        "click_node", "get_node_rect",
        "get_game_node_property", "eval_in_game", "get_game_scene_tree",
        "run_test_sequence"
    };
    ASSERT_EQ(tools.size(), expected_names.size());
    for (size_t i = 0; i < tools.size(); i++) {
        EXPECT_EQ(tools[i].name, expected_names[i]);
    }
}

// --- Filtered tools JSON tests ---

TEST(FilteredTools, Version430Returns38Tools) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 44);
}

TEST(FilteredTools, Version420Returns0Tools) {
    auto json_tools = get_filtered_tools_json({4, 2, 0});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 0);
}

TEST(FilteredTools, PermissiveVersionReturns38Tools) {
    auto json_tools = get_filtered_tools_json({99, 99, 99});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 44);
}

TEST(FilteredTools, EachToolHasNameDescriptionSchema) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        EXPECT_TRUE(tool.contains("name"));
        EXPECT_TRUE(tool.contains("description"));
        EXPECT_TRUE(tool.contains("inputSchema"));
        EXPECT_TRUE(tool["name"].is_string());
        EXPECT_TRUE(tool["description"].is_string());
        EXPECT_TRUE(tool["inputSchema"].is_object());
    }
}

TEST(FilteredTools, FirstToolIsGetSceneTree) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    ASSERT_GT(json_tools.size(), 0);
    EXPECT_EQ(json_tools[0]["name"], "get_scene_tree");
}

// --- Tool count tests ---

TEST(ToolCount, Version430Returns38) {
    EXPECT_EQ(get_tool_count({4, 3, 0}), 44);
}

TEST(ToolCount, Version420Returns0) {
    EXPECT_EQ(get_tool_count({4, 2, 0}), 0);
}

TEST(ToolCount, MatchesFilteredArraySize) {
    GodotVersion v{4, 3, 0};
    auto json_tools = get_filtered_tools_json(v);
    EXPECT_EQ(get_tool_count(v), static_cast<int>(json_tools.size()));
}

// --- Schema validation tests ---

TEST(FilteredTools, GetSceneTreeSchemaMatchesExpected) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    // Find get_scene_tree
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_scene_tree") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("max_depth"));
            EXPECT_TRUE(schema["properties"].contains("include_properties"));
            EXPECT_TRUE(schema["properties"].contains("root_path"));
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "get_scene_tree not found in filtered tools";
}

TEST(FilteredTools, CreateNodeSchemaMatchesExpected) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "create_node") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("type"));
            EXPECT_TRUE(schema["properties"].contains("parent_path"));
            EXPECT_TRUE(schema["properties"].contains("name"));
            EXPECT_TRUE(schema["properties"].contains("properties"));
            EXPECT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "type");
            return;
        }
    }
    FAIL() << "create_node not found in filtered tools";
}

// --- Phase 5 new tool schema validation tests ---

TEST(FilteredTools, RunGameSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "run_game") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("mode"));
            EXPECT_TRUE(schema["properties"].contains("scene_path"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "mode");
            // Verify mode enum
            auto mode_prop = schema["properties"]["mode"];
            ASSERT_TRUE(mode_prop.contains("enum"));
            ASSERT_EQ(mode_prop["enum"].size(), 3);
            EXPECT_EQ(mode_prop["enum"][0], "main");
            EXPECT_EQ(mode_prop["enum"][1], "current");
            EXPECT_EQ(mode_prop["enum"][2], "custom");
            return;
        }
    }
    FAIL() << "run_game not found in filtered tools";
}

TEST(FilteredTools, StopGameSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "stop_game") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "stop_game not found in filtered tools";
}

TEST(FilteredTools, GetGameOutputSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_game_output") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("clear_after_read"));
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "get_game_output not found in filtered tools";
}

TEST(FilteredTools, GetNodeSignalsSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_node_signals") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "node_path");
            return;
        }
    }
    FAIL() << "get_node_signals not found in filtered tools";
}

TEST(FilteredTools, ConnectSignalSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "connect_signal") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("source_path"));
            EXPECT_TRUE(schema["properties"].contains("signal_name"));
            EXPECT_TRUE(schema["properties"].contains("target_path"));
            EXPECT_TRUE(schema["properties"].contains("method_name"));
            ASSERT_EQ(schema["required"].size(), 4);
            EXPECT_EQ(schema["required"][0], "source_path");
            EXPECT_EQ(schema["required"][1], "signal_name");
            EXPECT_EQ(schema["required"][2], "target_path");
            EXPECT_EQ(schema["required"][3], "method_name");
            return;
        }
    }
    FAIL() << "connect_signal not found in filtered tools";
}

TEST(FilteredTools, DisconnectSignalSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "disconnect_signal") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("source_path"));
            EXPECT_TRUE(schema["properties"].contains("signal_name"));
            EXPECT_TRUE(schema["properties"].contains("target_path"));
            EXPECT_TRUE(schema["properties"].contains("method_name"));
            ASSERT_EQ(schema["required"].size(), 4);
            EXPECT_EQ(schema["required"][0], "source_path");
            EXPECT_EQ(schema["required"][1], "signal_name");
            EXPECT_EQ(schema["required"][2], "target_path");
            EXPECT_EQ(schema["required"][3], "method_name");
            return;
        }
    }
    FAIL() << "disconnect_signal not found in filtered tools";
}

// --- Phase 6 scene file tool schema validation tests ---

TEST(FilteredTools, SaveSceneSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "save_scene") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("path"));
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "save_scene not found in filtered tools";
}

TEST(FilteredTools, OpenSceneSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "open_scene") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("path"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "path");
            return;
        }
    }
    FAIL() << "open_scene not found in filtered tools";
}

TEST(FilteredTools, ListOpenScenesSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "list_open_scenes") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "list_open_scenes not found in filtered tools";
}

TEST(FilteredTools, CreateSceneSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "create_scene") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("root_type"));
            EXPECT_TRUE(schema["properties"].contains("path"));
            EXPECT_TRUE(schema["properties"].contains("root_name"));
            ASSERT_EQ(schema["required"].size(), 2);
            EXPECT_EQ(schema["required"][0], "root_type");
            EXPECT_EQ(schema["required"][1], "path");
            return;
        }
    }
    FAIL() << "create_scene not found in filtered tools";
}

TEST(FilteredTools, InstantiateSceneSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "instantiate_scene") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("scene_path"));
            EXPECT_TRUE(schema["properties"].contains("parent_path"));
            EXPECT_TRUE(schema["properties"].contains("name"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "scene_path");
            return;
        }
    }
    FAIL() << "instantiate_scene not found in filtered tools";
}

// --- Phase 7 UI system tool schema validation tests ---

TEST(FilteredTools, SetLayoutPresetSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "set_layout_preset") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            EXPECT_TRUE(schema["properties"].contains("preset"));
            ASSERT_EQ(schema["required"].size(), 2);
            EXPECT_EQ(schema["required"][0], "node_path");
            EXPECT_EQ(schema["required"][1], "preset");
            return;
        }
    }
    FAIL() << "set_layout_preset not found in filtered tools";
}

TEST(FilteredTools, SetThemeOverrideSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "set_theme_override") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            EXPECT_TRUE(schema["properties"].contains("overrides"));
            ASSERT_EQ(schema["required"].size(), 2);
            EXPECT_EQ(schema["required"][0], "node_path");
            EXPECT_EQ(schema["required"][1], "overrides");
            return;
        }
    }
    FAIL() << "set_theme_override not found in filtered tools";
}

TEST(FilteredTools, CreateStyleboxSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "create_stylebox") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            EXPECT_TRUE(schema["properties"].contains("override_name"));
            EXPECT_TRUE(schema["properties"].contains("bg_color"));
            EXPECT_TRUE(schema["properties"].contains("corner_radius"));
            EXPECT_TRUE(schema["properties"].contains("border_width"));
            ASSERT_EQ(schema["required"].size(), 2);
            EXPECT_EQ(schema["required"][0], "node_path");
            EXPECT_EQ(schema["required"][1], "override_name");
            return;
        }
    }
    FAIL() << "create_stylebox not found in filtered tools";
}

TEST(FilteredTools, GetUIPropertiesSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_ui_properties") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "node_path");
            return;
        }
    }
    FAIL() << "get_ui_properties not found in filtered tools";
}

TEST(FilteredTools, SetContainerLayoutSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "set_container_layout") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            EXPECT_TRUE(schema["properties"].contains("separation"));
            EXPECT_TRUE(schema["properties"].contains("alignment"));
            EXPECT_TRUE(schema["properties"].contains("columns"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "node_path");
            return;
        }
    }
    FAIL() << "set_container_layout not found in filtered tools";
}

TEST(FilteredTools, GetThemeOverridesSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_theme_overrides") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "node_path");
            return;
        }
    }
    FAIL() << "get_theme_overrides not found in filtered tools";
}

// --- Phase 8 Animation system tool schema validation tests ---

TEST(FilteredTools, CreateAnimationSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "create_animation") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("animation_name"));
            EXPECT_TRUE(schema["properties"].contains("player_path"));
            EXPECT_TRUE(schema["properties"].contains("parent_path"));
            EXPECT_TRUE(schema["properties"].contains("node_name"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "animation_name");
            return;
        }
    }
    FAIL() << "create_animation not found in filtered tools";
}

TEST(FilteredTools, AddAnimationTrackSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "add_animation_track") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("player_path"));
            EXPECT_TRUE(schema["properties"].contains("animation_name"));
            EXPECT_TRUE(schema["properties"].contains("track_type"));
            EXPECT_TRUE(schema["properties"].contains("track_path"));
            ASSERT_EQ(schema["required"].size(), 4);
            EXPECT_EQ(schema["required"][0], "player_path");
            EXPECT_EQ(schema["required"][1], "animation_name");
            EXPECT_EQ(schema["required"][2], "track_type");
            EXPECT_EQ(schema["required"][3], "track_path");
            return;
        }
    }
    FAIL() << "add_animation_track not found in filtered tools";
}

TEST(FilteredTools, SetKeyframeSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "set_keyframe") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("player_path"));
            EXPECT_TRUE(schema["properties"].contains("animation_name"));
            EXPECT_TRUE(schema["properties"].contains("track_index"));
            EXPECT_TRUE(schema["properties"].contains("time"));
            EXPECT_TRUE(schema["properties"].contains("action"));
            EXPECT_TRUE(schema["properties"].contains("value"));
            EXPECT_TRUE(schema["properties"].contains("transition"));
            ASSERT_EQ(schema["required"].size(), 5);
            EXPECT_EQ(schema["required"][0], "player_path");
            EXPECT_EQ(schema["required"][1], "animation_name");
            EXPECT_EQ(schema["required"][2], "track_index");
            EXPECT_EQ(schema["required"][3], "time");
            EXPECT_EQ(schema["required"][4], "action");
            // Verify track_index is integer, time is number
            EXPECT_EQ(schema["properties"]["track_index"]["type"], "integer");
            EXPECT_EQ(schema["properties"]["time"]["type"], "number");
            return;
        }
    }
    FAIL() << "set_keyframe not found in filtered tools";
}

TEST(FilteredTools, GetAnimationInfoSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_animation_info") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("player_path"));
            EXPECT_TRUE(schema["properties"].contains("animation_name"));
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "player_path");
            return;
        }
    }
    FAIL() << "get_animation_info not found in filtered tools";
}

TEST(FilteredTools, SetAnimationPropertiesSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "set_animation_properties") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("player_path"));
            EXPECT_TRUE(schema["properties"].contains("animation_name"));
            EXPECT_TRUE(schema["properties"].contains("length"));
            EXPECT_TRUE(schema["properties"].contains("loop_mode"));
            EXPECT_TRUE(schema["properties"].contains("step"));
            ASSERT_EQ(schema["required"].size(), 2);
            EXPECT_EQ(schema["required"][0], "player_path");
            EXPECT_EQ(schema["required"][1], "animation_name");
            // Verify length and step are number types
            EXPECT_EQ(schema["properties"]["length"]["type"], "number");
            EXPECT_EQ(schema["properties"]["step"]["type"], "number");
            return;
        }
    }
    FAIL() << "set_animation_properties not found in filtered tools";
}

// --- Phase 9 Viewport Screenshot tool schema validation tests ---

TEST(FilteredTools, CaptureViewportSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "capture_viewport") {
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("viewport_type"));
            EXPECT_TRUE(schema["properties"].contains("width"));
            EXPECT_TRUE(schema["properties"].contains("height"));
            // viewport_type has enum constraint
            auto vt_prop = schema["properties"]["viewport_type"];
            ASSERT_TRUE(vt_prop.contains("enum"));
            ASSERT_EQ(vt_prop["enum"].size(), 2);
            EXPECT_EQ(vt_prop["enum"][0], "2d");
            EXPECT_EQ(vt_prop["enum"][1], "3d");
            // width and height are integers
            EXPECT_EQ(schema["properties"]["width"]["type"], "integer");
            EXPECT_EQ(schema["properties"]["height"]["type"], "integer");
            // All params optional
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "capture_viewport not found in filtered tools";
}

// --- Phase 10 Game Bridge tool schema validation tests ---

TEST(FilteredTools, InjectInputSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "inject_input") {
            auto schema = tool["inputSchema"];
            // Check all 8 properties exist
            EXPECT_TRUE(schema["properties"].contains("type"));
            EXPECT_TRUE(schema["properties"].contains("keycode"));
            EXPECT_TRUE(schema["properties"].contains("pressed"));
            EXPECT_TRUE(schema["properties"].contains("action_name"));
            EXPECT_TRUE(schema["properties"].contains("mouse_action"));
            EXPECT_TRUE(schema["properties"].contains("position"));
            EXPECT_TRUE(schema["properties"].contains("button"));
            EXPECT_TRUE(schema["properties"].contains("direction"));
            // required has exactly 1 entry: "type"
            ASSERT_EQ(schema["required"].size(), 1);
            EXPECT_EQ(schema["required"][0], "type");
            // type property has enum with 3 values
            auto type_prop = schema["properties"]["type"];
            ASSERT_TRUE(type_prop.contains("enum"));
            ASSERT_EQ(type_prop["enum"].size(), 3);
            EXPECT_EQ(type_prop["enum"][0], "key");
            EXPECT_EQ(type_prop["enum"][1], "mouse");
            EXPECT_EQ(type_prop["enum"][2], "action");
            // mouse_action has enum: move, click, scroll
            auto ma_prop = schema["properties"]["mouse_action"];
            ASSERT_TRUE(ma_prop.contains("enum"));
            ASSERT_EQ(ma_prop["enum"].size(), 3);
            EXPECT_EQ(ma_prop["enum"][0], "move");
            EXPECT_EQ(ma_prop["enum"][1], "click");
            EXPECT_EQ(ma_prop["enum"][2], "scroll");
            // button has enum: left, right, middle
            auto btn_prop = schema["properties"]["button"];
            ASSERT_TRUE(btn_prop.contains("enum"));
            ASSERT_EQ(btn_prop["enum"].size(), 3);
            EXPECT_EQ(btn_prop["enum"][0], "left");
            EXPECT_EQ(btn_prop["enum"][1], "right");
            EXPECT_EQ(btn_prop["enum"][2], "middle");
            // direction has enum: up, down
            auto dir_prop = schema["properties"]["direction"];
            ASSERT_TRUE(dir_prop.contains("enum"));
            ASSERT_EQ(dir_prop["enum"].size(), 2);
            EXPECT_EQ(dir_prop["enum"][0], "up");
            EXPECT_EQ(dir_prop["enum"][1], "down");
            return;
        }
    }
    FAIL() << "inject_input not found in filtered tools";
}

TEST(FilteredTools, CaptureGameViewportSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "capture_game_viewport") {
            auto schema = tool["inputSchema"];
            // properties contains width and height
            EXPECT_TRUE(schema["properties"].contains("width"));
            EXPECT_TRUE(schema["properties"].contains("height"));
            // width and height are integers
            EXPECT_EQ(schema["properties"]["width"]["type"], "integer");
            EXPECT_EQ(schema["properties"]["height"]["type"], "integer");
            // required is empty
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "capture_game_viewport not found in filtered tools";
}

TEST(FilteredTools, GetGameBridgeStatusSchemaValidation) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    for (const auto& tool : json_tools) {
        if (tool["name"] == "get_game_bridge_status") {
            auto schema = tool["inputSchema"];
            // required is empty
            EXPECT_TRUE(schema["required"].empty());
            return;
        }
    }
    FAIL() << "get_game_bridge_status not found in filtered tools";
}
