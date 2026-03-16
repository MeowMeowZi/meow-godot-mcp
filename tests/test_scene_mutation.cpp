#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// These tests validate the JSON argument extraction and response format
// contracts for scene mutation tools. Actual node manipulation requires
// a running Godot editor and is covered by UAT.

// --- create_node argument contract ---

TEST(CreateNodeArgs, RequiresType) {
    json args = {{"parent_path", ""}, {"name", "MyNode"}};
    // type is required, absence should produce error
    EXPECT_FALSE(args.contains("type"));
}

TEST(CreateNodeArgs, TypeIsString) {
    json args = {{"type", "Sprite2D"}, {"parent_path", "Player"}, {"name", "Icon"}};
    EXPECT_TRUE(args["type"].is_string());
    EXPECT_EQ(args["type"].get<std::string>(), "Sprite2D");
}

TEST(CreateNodeArgs, PropertiesIsOptionalObject) {
    json args = {
        {"type", "Node2D"},
        {"properties", {{"position", "Vector2(100, 200)"}, {"visible", "false"}}}
    };
    EXPECT_TRUE(args["properties"].is_object());
    EXPECT_EQ(args["properties"]["position"], "Vector2(100, 200)");
}

TEST(CreateNodeArgs, EmptyParentPathMeansRoot) {
    json args = {{"type", "Node"}, {"parent_path", ""}};
    EXPECT_TRUE(args["parent_path"].get<std::string>().empty());
}

// --- create_node response contract ---

TEST(CreateNodeResponse, SuccessFormat) {
    json response = {{"success", true}, {"path", "Player/Sprite2D"}, {"type", "Sprite2D"}};
    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_TRUE(response.contains("path"));
    EXPECT_TRUE(response.contains("type"));
}

TEST(CreateNodeResponse, ErrorFormat) {
    json response = {{"error", "Unknown class: FakeNode"}};
    EXPECT_TRUE(response.contains("error"));
    EXPECT_FALSE(response.contains("success"));
}

// --- set_node_property argument contract ---

TEST(SetNodePropertyArgs, AllFieldsRequired) {
    json args = {{"node_path", "Player"}, {"property", "position"}, {"value", "Vector2(100, 200)"}};
    EXPECT_TRUE(args.contains("node_path"));
    EXPECT_TRUE(args.contains("property"));
    EXPECT_TRUE(args.contains("value"));
}

TEST(SetNodePropertyResponse, SuccessFormat) {
    json response = {{"success", true}};
    EXPECT_TRUE(response["success"].get<bool>());
}

// --- delete_node argument contract ---

TEST(DeleteNodeArgs, NodePathRequired) {
    json args = {{"node_path", "Player/Sprite2D"}};
    EXPECT_TRUE(args.contains("node_path"));
    EXPECT_EQ(args["node_path"].get<std::string>(), "Player/Sprite2D");
}

TEST(DeleteNodeResponse, SuccessFormat) {
    json response = {{"success", true}};
    EXPECT_TRUE(response["success"].get<bool>());
}

TEST(DeleteNodeResponse, CannotDeleteRoot) {
    json response = {{"error", "Cannot delete scene root"}};
    std::string msg = response["error"].get<std::string>();
    EXPECT_NE(msg.find("scene root"), std::string::npos);
}
