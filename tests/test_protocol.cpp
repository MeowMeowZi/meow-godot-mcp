#include <gtest/gtest.h>
#include "mcp_protocol.h"
#include "mcp_prompts.h"

using namespace mcp;
using json = nlohmann::json;

// --- parse_jsonrpc tests ---

TEST(ParseJsonRpc, ValidRequest) {
    std::string input = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    auto result = parse_jsonrpc(input);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.message.method, "initialize");
    EXPECT_EQ(result.message.id, 1);
    EXPECT_TRUE(result.message.params.is_object());
    EXPECT_FALSE(result.message.is_notification);
}

TEST(ParseJsonRpc, ValidRequestWithStringId) {
    std::string input = R"({"jsonrpc":"2.0","id":"abc","method":"tools/list"})";
    auto result = parse_jsonrpc(input);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.message.method, "tools/list");
    EXPECT_EQ(result.message.id, "abc");
    EXPECT_FALSE(result.message.is_notification);
}

TEST(ParseJsonRpc, MalformedJson) {
    std::string input = "not valid json {{{";
    auto result = parse_jsonrpc(input);

    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_response.contains("error"));
    EXPECT_EQ(result.error_response["error"]["code"], PARSE_ERROR);
}

TEST(ParseJsonRpc, MissingMethod) {
    std::string input = R"({"jsonrpc":"2.0","id":1})";
    auto result = parse_jsonrpc(input);

    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_response.contains("error"));
    EXPECT_EQ(result.error_response["error"]["code"], INVALID_REQUEST);
}

TEST(ParseJsonRpc, Notification) {
    std::string input = R"({"jsonrpc":"2.0","method":"notifications/initialized"})";
    auto result = parse_jsonrpc(input);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.message.method, "notifications/initialized");
    EXPECT_TRUE(result.message.is_notification);
    EXPECT_TRUE(result.message.id.is_null());
}

TEST(ParseJsonRpc, ParamsOptional) {
    std::string input = R"({"jsonrpc":"2.0","id":5,"method":"tools/list"})";
    auto result = parse_jsonrpc(input);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.message.method, "tools/list");
    // params should be null or empty when not provided
}

// --- create_initialize_response tests ---

TEST(InitializeResponse, HasCorrectProtocolVersion) {
    auto response = create_initialize_response(1);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["result"]["protocolVersion"], "2025-03-26");
}

TEST(InitializeResponse, HasServerInfo) {
    auto response = create_initialize_response(1);

    EXPECT_EQ(response["result"]["serverInfo"]["name"], "meow-godot-mcp");
    EXPECT_EQ(response["result"]["serverInfo"]["version"], "0.2.0");
}

TEST(InitializeResponse, HasToolsCapability) {
    auto response = create_initialize_response(1);

    ASSERT_TRUE(response["result"]["capabilities"].contains("tools"));
    // listChanged is false since we don't dynamically add/remove tools
    EXPECT_EQ(response["result"]["capabilities"]["tools"]["listChanged"], false);
}

TEST(InitializeResponse, PreservesStringId) {
    auto response = create_initialize_response("req-42");

    EXPECT_EQ(response["id"], "req-42");
}

// --- create_tools_list_response tests ---

TEST(ToolsListResponse, HasGetSceneTreeTool) {
    auto response = create_tools_list_response(2);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 2);

    auto tools = response["result"]["tools"];
    ASSERT_EQ(tools.size(), 52);
    EXPECT_EQ(tools[0]["name"], "get_scene_tree");
}

TEST(ToolsListResponse, HasCorrectInputSchema) {
    auto response = create_tools_list_response(2);
    auto schema = response["result"]["tools"][0]["inputSchema"];

    EXPECT_EQ(schema["type"], "object");

    auto props = schema["properties"];
    ASSERT_TRUE(props.contains("max_depth"));
    EXPECT_EQ(props["max_depth"]["type"], "integer");

    ASSERT_TRUE(props.contains("include_properties"));
    EXPECT_EQ(props["include_properties"]["type"], "boolean");

    ASSERT_TRUE(props.contains("root_path"));
    EXPECT_EQ(props["root_path"]["type"], "string");

    // All properties are optional
    auto required = schema["required"];
    EXPECT_TRUE(required.empty());
}

TEST(ToolsListResponse, HasDescription) {
    auto response = create_tools_list_response(2);
    auto desc = response["result"]["tools"][0]["description"].get<std::string>();

    // Description should mention scene tree
    EXPECT_NE(desc.find("scene tree"), std::string::npos);
}

// --- create_tool_result tests ---

TEST(ToolResult, WrapsContentCorrectly) {
    json data = {{"name", "Root"}, {"type", "Node2D"}};
    auto response = create_tool_result(3, data);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 3);
    EXPECT_FALSE(response["result"]["isError"]);

    auto content = response["result"]["content"];
    ASSERT_EQ(content.size(), 1);
    EXPECT_EQ(content[0]["type"], "text");
    // text field contains the serialized JSON data
    EXPECT_FALSE(content[0]["text"].get<std::string>().empty());
}

TEST(ToolResult, TextContainsSerializedData) {
    json data = {{"key", "value"}};
    auto response = create_tool_result(3, data);

    std::string text = response["result"]["content"][0]["text"].get<std::string>();
    // Parse the text back and verify it matches
    auto parsed = json::parse(text);
    EXPECT_EQ(parsed["key"], "value");
}

// --- create_error_response tests ---

TEST(ErrorResponse, HasCorrectStructure) {
    auto response = create_error_response(4, METHOD_NOT_FOUND, "Method not found");

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 4);
    EXPECT_EQ(response["error"]["code"], -32601);
    EXPECT_EQ(response["error"]["message"], "Method not found");
}

TEST(ErrorResponse, WithNullId) {
    auto response = create_error_response(nullptr, PARSE_ERROR, "Parse error");

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_TRUE(response["id"].is_null());
    EXPECT_EQ(response["error"]["code"], -32700);
}

TEST(ErrorResponse, WithInternalError) {
    auto response = create_error_response(10, INTERNAL_ERROR, "Something went wrong");

    EXPECT_EQ(response["error"]["code"], -32603);
    EXPECT_EQ(response["error"]["message"], "Something went wrong");
}

// --- create_tool_not_found_error tests ---

TEST(ToolNotFoundError, ContainsToolName) {
    auto response = create_tool_not_found_error(5, "foo_tool");

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 5);
    EXPECT_EQ(response["error"]["code"], INVALID_PARAMS);
    // Error message should contain the tool name
    std::string msg = response["error"]["message"].get<std::string>();
    EXPECT_NE(msg.find("foo_tool"), std::string::npos);
}

TEST(ToolNotFoundError, UsesInvalidParamsCode) {
    auto response = create_tool_not_found_error(6, "bar");

    EXPECT_EQ(response["error"]["code"], -32602);
}

// --- Scene mutation tool registration tests ---

TEST(ToolsListResponse, HasCreateNodeTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "create_node") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("type"));
            EXPECT_TRUE(schema["properties"].contains("parent_path"));
            EXPECT_TRUE(schema["properties"].contains("name"));
            EXPECT_TRUE(schema["properties"].contains("properties"));
            // "type" is required
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 1);
            EXPECT_EQ(req[0], "type");
            break;
        }
    }
    EXPECT_TRUE(found) << "create_node tool not found in tools/list";
}

TEST(ToolsListResponse, HasSetNodePropertyTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "set_node_property") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            EXPECT_TRUE(schema["properties"].contains("property"));
            EXPECT_TRUE(schema["properties"].contains("value"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 3);
            break;
        }
    }
    EXPECT_TRUE(found) << "set_node_property tool not found in tools/list";
}

TEST(ToolsListResponse, HasDeleteNodeTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "delete_node") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 1);
            EXPECT_EQ(req[0], "node_path");
            break;
        }
    }
    EXPECT_TRUE(found) << "delete_node tool not found in tools/list";
}

// --- Script tool registration tests ---

TEST(ToolsListResponse, HasReadScriptTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "read_script") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("path"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 1);
            EXPECT_EQ(req[0], "path");
            break;
        }
    }
    EXPECT_TRUE(found) << "read_script tool not found in tools/list";
}

TEST(ToolsListResponse, HasWriteScriptTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "write_script") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("path"));
            EXPECT_TRUE(schema["properties"].contains("content"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 2);
            break;
        }
    }
    EXPECT_TRUE(found) << "write_script tool not found in tools/list";
}

TEST(ToolsListResponse, HasEditScriptTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "edit_script") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("path"));
            EXPECT_TRUE(schema["properties"].contains("operation"));
            EXPECT_TRUE(schema["properties"].contains("line"));
            EXPECT_TRUE(schema["properties"].contains("content"));
            EXPECT_TRUE(schema["properties"].contains("end_line"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 3);
            break;
        }
    }
    EXPECT_TRUE(found) << "edit_script tool not found in tools/list";
}

TEST(ToolsListResponse, HasAttachScriptTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "attach_script") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            EXPECT_TRUE(schema["properties"].contains("script_path"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 2);
            break;
        }
    }
    EXPECT_TRUE(found) << "attach_script tool not found in tools/list";
}

TEST(ToolsListResponse, HasDetachScriptTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "detach_script") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("node_path"));
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 1);
            EXPECT_EQ(req[0], "node_path");
            break;
        }
    }
    EXPECT_TRUE(found) << "detach_script tool not found in tools/list";
}

// --- Project tool registration tests ---

TEST(ToolsListResponse, HasListProjectFilesTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "list_project_files") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].empty());
            EXPECT_TRUE(schema["required"].empty());
            break;
        }
    }
    EXPECT_TRUE(found) << "list_project_files tool not found in tools/list";
}

TEST(ToolsListResponse, HasGetProjectSettingsTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "get_project_settings") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("category"));
            EXPECT_TRUE(schema["required"].empty());
            break;
        }
    }
    EXPECT_TRUE(found) << "get_project_settings tool not found in tools/list";
}

TEST(ToolsListResponse, HasGetResourceInfoTool) {
    auto response = create_tools_list_response(2);
    auto tools = response["result"]["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "get_resource_info") {
            found = true;
            auto schema = tool["inputSchema"];
            EXPECT_TRUE(schema["properties"].contains("path"));
            EXPECT_EQ(schema["properties"]["path"]["type"], "string");
            auto req = schema["required"];
            EXPECT_EQ(req.size(), 1);
            EXPECT_EQ(req[0], "path");
            break;
        }
    }
    EXPECT_TRUE(found) << "get_resource_info tool not found in tools/list";
}

// --- Initialize response resources capability test ---
// resources capability removed from initialize response (not supported, was causing null)

TEST(InitializeResponse, DoesNotAdvertiseResources) {
    auto response = create_initialize_response(1);
    EXPECT_FALSE(response["result"]["capabilities"].contains("resources"));
}

// --- Resources protocol builder tests ---

TEST(ResourcesListResponse, HasCorrectStructure) {
    nlohmann::json resources = {{{"uri", "test://foo"}, {"name", "Test"}}};
    auto response = create_resources_list_response(1, resources);
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_EQ(response["result"]["resources"].size(), 1);
    EXPECT_EQ(response["result"]["resources"][0]["uri"], "test://foo");
}

TEST(ResourceReadResponse, HasCorrectStructure) {
    nlohmann::json contents = {{{"uri", "test://foo"}, {"mimeType", "application/json"}, {"text", "{}"}}};
    auto response = create_resource_read_response(2, contents);
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 2);
    EXPECT_EQ(response["result"]["contents"].size(), 1);
    EXPECT_EQ(response["result"]["contents"][0]["mimeType"], "application/json");
}

// --- Initialize response prompts capability test ---

TEST(InitializeResponse, HasPromptsCapability) {
    auto response = create_initialize_response(1);
    ASSERT_TRUE(response["result"]["capabilities"].contains("prompts"));
    EXPECT_EQ(response["result"]["capabilities"]["prompts"]["listChanged"], false);
}

// --- Prompts module tests ---

TEST(PromptsListResponse, HasCorrectStructure) {
    auto response = create_prompts_list_response(10);
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 10);
    auto prompts = response["result"]["prompts"];
    ASSERT_EQ(prompts.size(), 7);
    for (const auto& p : prompts) {
        EXPECT_TRUE(p.contains("name"));
        EXPECT_TRUE(p.contains("description"));
        EXPECT_TRUE(p.contains("arguments"));
        EXPECT_TRUE(p["name"].is_string());
        EXPECT_TRUE(p["description"].is_string());
        EXPECT_TRUE(p["arguments"].is_array());
    }
}

TEST(PromptsListResponse, HasCreatePlayerControllerPrompt) {
    auto response = create_prompts_list_response(10);
    auto prompts = response["result"]["prompts"];
    bool found = false;
    for (const auto& p : prompts) {
        if (p["name"] == "create_player_controller") {
            found = true;
            EXPECT_FALSE(p["description"].get<std::string>().empty());
            ASSERT_GE(p["arguments"].size(), 1);
            EXPECT_EQ(p["arguments"][0]["name"], "movement_type");
            EXPECT_TRUE(p["arguments"][0]["required"].get<bool>());
            break;
        }
    }
    EXPECT_TRUE(found) << "create_player_controller prompt not found";
}

TEST(PromptGetResponse, HasCorrectStructure) {
    auto response = create_prompt_get_response(11, "Test description", json::array({
        {{"role", "user"}, {"content", {{"type", "text"}, {"text", "Hello"}}}}
    }));
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 11);
    EXPECT_EQ(response["result"]["description"], "Test description");
    ASSERT_EQ(response["result"]["messages"].size(), 1);
    EXPECT_EQ(response["result"]["messages"][0]["role"], "user");
}

TEST(PromptNotFoundError, ContainsPromptName) {
    auto response = create_prompt_not_found_error(12, "nonexistent");
    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 12);
    EXPECT_EQ(response["error"]["code"], INVALID_PARAMS);
    std::string msg = response["error"]["message"].get<std::string>();
    EXPECT_NE(msg.find("nonexistent"), std::string::npos);
}

// --- Prompts data layer tests ---

TEST(PromptsData, GetAllPromptsReturns7) {
    auto prompts = get_all_prompts_json();
    ASSERT_EQ(prompts.size(), 7);
}

TEST(PromptsData, PromptExistsWorks) {
    EXPECT_TRUE(prompt_exists("create_player_controller"));
    EXPECT_TRUE(prompt_exists("setup_scene_structure"));
    EXPECT_TRUE(prompt_exists("debug_physics"));
    EXPECT_TRUE(prompt_exists("create_ui_interface"));
    EXPECT_TRUE(prompt_exists("build_ui_layout"));
    EXPECT_TRUE(prompt_exists("setup_animation"));
    EXPECT_FALSE(prompt_exists("nonexistent"));
    EXPECT_FALSE(prompt_exists(""));
}

TEST(PromptsData, GetPromptMessagesValidPrompt) {
    auto messages = get_prompt_messages("create_player_controller", {{"movement_type", "2d_platformer"}});
    ASSERT_FALSE(messages.is_null());
    ASSERT_TRUE(messages.is_array());
    ASSERT_GE(messages.size(), 1);
    EXPECT_EQ(messages[0]["role"], "user");
    EXPECT_TRUE(messages[0]["content"].contains("type"));
    EXPECT_EQ(messages[0]["content"]["type"], "text");
    // Verify argument substitution
    std::string text = messages[0]["content"]["text"].get<std::string>();
    EXPECT_NE(text.find("2d_platformer"), std::string::npos);
}

TEST(PromptsData, GetPromptMessagesNonexistent) {
    auto messages = get_prompt_messages("nonexistent", {});
    EXPECT_TRUE(messages.is_null());
}

TEST(PromptsData, BuildUiLayoutPromptContent) {
    auto messages = get_prompt_messages("build_ui_layout", {{"layout_type", "main_menu"}});
    ASSERT_FALSE(messages.is_null());
    ASSERT_TRUE(messages.is_array());
    ASSERT_GE(messages.size(), 1);
    EXPECT_EQ(messages[0]["role"], "user");
    std::string text = messages[0]["content"]["text"].get<std::string>();
    // Must reference v1.1 UI tool names
    EXPECT_NE(text.find("create_node"), std::string::npos);
    EXPECT_NE(text.find("set_layout_preset"), std::string::npos);
    EXPECT_NE(text.find("set_theme_override"), std::string::npos);
    EXPECT_NE(text.find("create_stylebox"), std::string::npos);
    EXPECT_NE(text.find("set_container_layout"), std::string::npos);
    EXPECT_NE(text.find("get_ui_properties"), std::string::npos);
    EXPECT_NE(text.find("save_scene"), std::string::npos);
    // Must mention the layout type
    EXPECT_NE(text.find("Main Menu"), std::string::npos);
}

TEST(PromptsData, SetupAnimationPromptContent) {
    auto messages = get_prompt_messages("setup_animation", {{"animation_type", "ui_transition"}});
    ASSERT_FALSE(messages.is_null());
    ASSERT_TRUE(messages.is_array());
    ASSERT_GE(messages.size(), 1);
    EXPECT_EQ(messages[0]["role"], "user");
    std::string text = messages[0]["content"]["text"].get<std::string>();
    // Must reference v1.1 animation tool names
    EXPECT_NE(text.find("create_animation"), std::string::npos);
    EXPECT_NE(text.find("add_animation_track"), std::string::npos);
    EXPECT_NE(text.find("set_keyframe"), std::string::npos);
    EXPECT_NE(text.find("set_animation_properties"), std::string::npos);
    EXPECT_NE(text.find("get_animation_info"), std::string::npos);
}

TEST(PromptsData, BuildUiLayoutDefaultParam) {
    // No arguments -- should default to main_menu
    auto messages = get_prompt_messages("build_ui_layout", {});
    ASSERT_FALSE(messages.is_null());
    std::string text = messages[0]["content"]["text"].get<std::string>();
    EXPECT_NE(text.find("Main Menu"), std::string::npos);
}

TEST(PromptsData, SetupAnimationDefaultParam) {
    // No arguments -- should default to ui_transition
    auto messages = get_prompt_messages("setup_animation", {});
    ASSERT_FALSE(messages.is_null());
    std::string text = messages[0]["content"]["text"].get<std::string>();
    EXPECT_NE(text.find("UI Transition"), std::string::npos);
}

// --- create_image_tool_result tests ---

TEST(ImageToolResult, HasCorrectStructure) {
    auto response = mcp::create_image_tool_result(42, "iVBORw0KGgo=", "image/png");

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 42);
    EXPECT_FALSE(response["result"]["isError"]);

    auto content = response["result"]["content"];
    ASSERT_EQ(content.size(), 1);
    EXPECT_EQ(content[0]["type"], "image");
    EXPECT_EQ(content[0]["data"], "iVBORw0KGgo=");
    EXPECT_EQ(content[0]["mimeType"], "image/png");
}

TEST(ImageToolResult, IncludesMetadataAsTextContent) {
    json metadata = {{"viewport_type", "2d"}, {"width", 1920}, {"height", 1080}};
    auto response = mcp::create_image_tool_result(43, "base64data", "image/png", metadata);

    auto content = response["result"]["content"];
    ASSERT_EQ(content.size(), 2);
    EXPECT_EQ(content[0]["type"], "image");
    EXPECT_EQ(content[1]["type"], "text");
    // Metadata text should be parseable JSON
    auto parsed = json::parse(content[1]["text"].get<std::string>());
    EXPECT_EQ(parsed["viewport_type"], "2d");
    EXPECT_EQ(parsed["width"], 1920);
}

TEST(ImageToolResult, OmitsMetadataWhenNull) {
    auto response = mcp::create_image_tool_result(44, "data", "image/png");

    auto content = response["result"]["content"];
    ASSERT_EQ(content.size(), 1);
    EXPECT_EQ(content[0]["type"], "image");
}

TEST(ImageToolResult, PreservesStringId) {
    auto response = mcp::create_image_tool_result("req-99", "data", "image/png");
    EXPECT_EQ(response["id"], "req-99");
}
