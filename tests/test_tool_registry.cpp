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

TEST(ToolRegistry, HasExactly12Tools) {
    const auto& tools = get_all_tools();
    ASSERT_EQ(tools.size(), 12);
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
        "detach_script", "list_project_files", "get_project_settings", "get_resource_info"
    };
    ASSERT_EQ(tools.size(), expected_names.size());
    for (size_t i = 0; i < tools.size(); i++) {
        EXPECT_EQ(tools[i].name, expected_names[i]);
    }
}

// --- Filtered tools JSON tests ---

TEST(FilteredTools, Version430Returns12Tools) {
    auto json_tools = get_filtered_tools_json({4, 3, 0});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 12);
}

TEST(FilteredTools, Version420Returns0Tools) {
    auto json_tools = get_filtered_tools_json({4, 2, 0});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 0);
}

TEST(FilteredTools, PermissiveVersionReturns12Tools) {
    auto json_tools = get_filtered_tools_json({99, 99, 99});
    ASSERT_TRUE(json_tools.is_array());
    EXPECT_EQ(json_tools.size(), 12);
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

TEST(ToolCount, Version430Returns12) {
    EXPECT_EQ(get_tool_count({4, 3, 0}), 12);
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
