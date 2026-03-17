#include <gtest/gtest.h>
#include "mcp_protocol.h"

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

    EXPECT_EQ(response["result"]["serverInfo"]["name"], "godot-mcp-meow");
    EXPECT_EQ(response["result"]["serverInfo"]["version"], "0.1.0");
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
    ASSERT_EQ(tools.size(), 9);
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
