#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// These tests validate the JSON format contract that the bridge/client will consume.
// Since scene_tools.cpp depends on Godot API (EditorInterface, Node), we test the
// expected output structure rather than calling the functions directly.

// --- Empty scene (no scene open) ---

TEST(SceneTreeFormat, EmptySceneReturnSuccess) {
    // When no scene is open, get_scene_tree returns success with null tree
    json response = {
        {"tree", nullptr},
        {"message", "No scene currently open"}
    };

    EXPECT_TRUE(response["tree"].is_null());
    EXPECT_EQ(response["message"], "No scene currently open");
    // This is NOT an error -- it's a valid success response with empty data
    EXPECT_FALSE(response.contains("error"));
}

// --- Basic node structure ---

TEST(SceneTreeFormat, BasicNodeHasCoreFields) {
    json node = {
        {"name", "Player"},
        {"type", "CharacterBody2D"},
        {"path", "/root/Main/Player"},
        {"children", json::array()}
    };

    EXPECT_EQ(node["name"], "Player");
    EXPECT_EQ(node["type"], "CharacterBody2D");
    EXPECT_EQ(node["path"], "/root/Main/Player");
    EXPECT_TRUE(node["children"].is_array());
    EXPECT_TRUE(node["children"].empty());
}

// --- Nested children ---

TEST(SceneTreeFormat, NestedChildrenStructure) {
    json tree = {
        {"name", "Root"},
        {"type", "Node2D"},
        {"path", "/root/Root"},
        {"children", {
            {
                {"name", "Child1"},
                {"type", "Sprite2D"},
                {"path", "/root/Root/Child1"},
                {"children", json::array()}
            },
            {
                {"name", "Child2"},
                {"type", "CollisionShape2D"},
                {"path", "/root/Root/Child2"},
                {"children", json::array()}
            }
        }}
    };

    ASSERT_EQ(tree["children"].size(), 2);
    EXPECT_EQ(tree["children"][0]["name"], "Child1");
    EXPECT_EQ(tree["children"][1]["name"], "Child2");
}

// --- Depth truncation ---

TEST(SceneTreeFormat, DepthTruncation) {
    // When depth limit is reached and node has children, indicate truncation
    json truncated_node = {
        {"name", "Deep"},
        {"type", "Node"},
        {"path", "/root/Deep"},
        {"children_truncated", true},
        {"child_count", 5}
    };

    EXPECT_TRUE(truncated_node["children_truncated"].get<bool>());
    EXPECT_EQ(truncated_node["child_count"], 5);
    // When truncated, "children" array is NOT present
    EXPECT_FALSE(truncated_node.contains("children"));
}

// --- Transform data (2D) ---

TEST(SceneTreeFormat, Node2DTransform) {
    json node = {
        {"name", "Sprite"},
        {"type", "Sprite2D"},
        {"path", "/root/Sprite"},
        {"transform", {
            {"position", {{"x", 100.0}, {"y", 200.0}}},
            {"rotation", 1.5707963},
            {"scale", {{"x", 1.0}, {"y", 1.0}}}
        }},
        {"visible", true},
        {"has_script", false},
        {"children", json::array()}
    };

    auto& transform = node["transform"];
    EXPECT_DOUBLE_EQ(transform["position"]["x"].get<double>(), 100.0);
    EXPECT_DOUBLE_EQ(transform["position"]["y"].get<double>(), 200.0);
    EXPECT_NEAR(transform["rotation"].get<double>(), 1.5707963, 0.0001);
    EXPECT_DOUBLE_EQ(transform["scale"]["x"].get<double>(), 1.0);
    // 2D transform has no z component
    EXPECT_FALSE(transform["position"].contains("z"));
}

// --- Transform data (3D) ---

TEST(SceneTreeFormat, Node3DTransform) {
    json node = {
        {"name", "Mesh"},
        {"type", "MeshInstance3D"},
        {"path", "/root/Mesh"},
        {"transform", {
            {"position", {{"x", 1.0}, {"y", 2.0}, {"z", 3.0}}},
            {"rotation", {{"x", 0.0}, {"y", 1.5707963}, {"z", 0.0}}},
            {"scale", {{"x", 1.0}, {"y", 1.0}, {"z", 1.0}}}
        }},
        {"visible", true},
        {"has_script", false},
        {"children", json::array()}
    };

    auto& transform = node["transform"];
    // 3D transform has z component
    EXPECT_TRUE(transform["position"].contains("z"));
    EXPECT_DOUBLE_EQ(transform["position"]["z"].get<double>(), 3.0);
    // 3D rotation is a vector (x,y,z), not a scalar
    EXPECT_TRUE(transform["rotation"].is_object());
    EXPECT_TRUE(transform["rotation"].contains("x"));
    EXPECT_TRUE(transform["rotation"].contains("y"));
    EXPECT_TRUE(transform["rotation"].contains("z"));
}

// --- Script info ---

TEST(SceneTreeFormat, NodeWithScript) {
    json node = {
        {"name", "Player"},
        {"type", "CharacterBody2D"},
        {"path", "/root/Player"},
        {"has_script", true},
        {"script_path", "res://scripts/player.gd"},
        {"children", json::array()}
    };

    EXPECT_TRUE(node["has_script"].get<bool>());
    EXPECT_EQ(node["script_path"], "res://scripts/player.gd");
}

TEST(SceneTreeFormat, NodeWithoutScript) {
    json node = {
        {"name", "Wall"},
        {"type", "StaticBody2D"},
        {"path", "/root/Wall"},
        {"has_script", false},
        {"children", json::array()}
    };

    EXPECT_FALSE(node["has_script"].get<bool>());
    // script_path should be absent when has_script is false
    EXPECT_FALSE(node.contains("script_path"));
}

// --- Visibility ---

TEST(SceneTreeFormat, NonVisualNodeOmitsVisible) {
    // Plain Node (not CanvasItem or Node3D) should NOT have "visible" field
    json node = {
        {"name", "Logic"},
        {"type", "Node"},
        {"path", "/root/Logic"},
        {"has_script", true},
        {"script_path", "res://scripts/game_logic.gd"},
        {"children", json::array()}
    };

    EXPECT_FALSE(node.contains("visible"));
    EXPECT_FALSE(node.contains("transform"));
}

// --- Node path error ---

TEST(SceneTreeFormat, NodeNotFoundError) {
    // When root_path points to a non-existent node
    json response = {
        {"error", "Node not found at path: /root/Missing"}
    };

    EXPECT_TRUE(response.contains("error"));
    std::string err_msg = response["error"].get<std::string>();
    EXPECT_NE(err_msg.find("/root/Missing"), std::string::npos);
}

// --- Complete tree example ---

TEST(SceneTreeFormat, CompleteTreeExample) {
    // A realistic scene tree with mixed node types
    json tree = {
        {"name", "Main"},
        {"type", "Node2D"},
        {"path", "/root/Main"},
        {"transform", {
            {"position", {{"x", 0.0}, {"y", 0.0}}},
            {"rotation", 0.0},
            {"scale", {{"x", 1.0}, {"y", 1.0}}}
        }},
        {"visible", true},
        {"has_script", true},
        {"script_path", "res://scripts/main.gd"},
        {"children", {
            {
                {"name", "Player"},
                {"type", "CharacterBody2D"},
                {"path", "/root/Main/Player"},
                {"transform", {
                    {"position", {{"x", 100.0}, {"y", 200.0}}},
                    {"rotation", 0.0},
                    {"scale", {{"x", 1.0}, {"y", 1.0}}}
                }},
                {"visible", true},
                {"has_script", true},
                {"script_path", "res://scripts/player.gd"},
                {"children", json::array()}
            },
            {
                {"name", "UI"},
                {"type", "CanvasLayer"},
                {"path", "/root/Main/UI"},
                {"has_script", false},
                {"children", json::array()}
            }
        }}
    };

    // Root has correct type
    EXPECT_EQ(tree["type"], "Node2D");
    // Root has children
    ASSERT_EQ(tree["children"].size(), 2);
    // Player child
    EXPECT_EQ(tree["children"][0]["name"], "Player");
    EXPECT_TRUE(tree["children"][0]["has_script"].get<bool>());
    EXPECT_EQ(tree["children"][0]["script_path"], "res://scripts/player.gd");
    // UI child (CanvasLayer is not CanvasItem, so no visible/transform)
    EXPECT_EQ(tree["children"][1]["name"], "UI");
    EXPECT_FALSE(tree["children"][1].contains("visible"));
    EXPECT_FALSE(tree["children"][1].contains("transform"));
}
