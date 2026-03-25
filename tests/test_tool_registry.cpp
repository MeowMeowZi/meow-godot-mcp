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
    ASSERT_EQ(tools.size(), 30);
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
        "list_project_files",
        "run_game", "stop_game", "get_game_output",
        "connect_signal",
        "save_scene", "open_scene", "create_scene",
        "capture_viewport",
        "inject_input", "validate_scripts", "create_node_tree",
        "inject_input_sequence", "inject_text",
        "capture_game_viewport",
        "get_game_node_property", "eval_in_game",
        "set_tilemap_cells", "erase_tilemap_cells",
        "batch_set_property",
        "duplicate_node",
        "restart_editor"
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
    EXPECT_EQ(json_tools.size(), 30);
}

TEST(FilteredTools, Version420Returns0Tools) {
    auto json_tools = get_filtered_tools_json({4, 2, 0});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 0);
}

TEST(FilteredTools, PermissiveVersionReturns38Tools) {
    auto json_tools = get_filtered_tools_json({99, 99, 99});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 30);
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
    EXPECT_EQ(get_tool_count({4, 3, 0}), 30);
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

// get_game_bridge_status test removed (tool consolidated in DX optimization)
