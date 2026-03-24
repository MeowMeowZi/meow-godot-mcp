#include <gtest/gtest.h>
#include "mcp_prompts.h"
#include "mcp_tool_registry.h"
#include <set>
#include <string>

// --- Prompt registry count test ---

TEST(PromptRegistry, HasExactly11Prompts) {
    auto prompts = get_all_prompts_json();
    ASSERT_EQ(prompts.size(), 11);
}

// --- Prompt field validation ---

TEST(PromptRegistry, EachPromptHasRequiredFields) {
    auto prompts = get_all_prompts_json();
    for (const auto& p : prompts) {
        EXPECT_TRUE(p.contains("name")) << "Prompt missing 'name' field";
        EXPECT_TRUE(p.contains("description")) << "Prompt missing 'description' field";
        EXPECT_TRUE(p.contains("arguments")) << "Prompt missing 'arguments' field";

        EXPECT_TRUE(p["name"].is_string()) << "Prompt 'name' is not string";
        EXPECT_FALSE(p["name"].get<std::string>().empty()) << "Prompt 'name' is empty";

        EXPECT_TRUE(p["description"].is_string()) << "Prompt 'description' is not string";
        EXPECT_FALSE(p["description"].get<std::string>().empty()) << "Prompt " << p["name"] << " 'description' is empty";

        EXPECT_TRUE(p["arguments"].is_array()) << "Prompt " << p["name"] << " 'arguments' is not array";
    }
}

// --- Prompt names verification ---

TEST(PromptRegistry, PromptNamesAreCorrect) {
    auto prompts = get_all_prompts_json();
    std::vector<std::string> expected_names = {
        "create_player_controller",
        "setup_scene_structure",
        "debug_physics",
        "create_ui_interface",
        "build_ui_layout",
        "setup_animation",
        "test_game_ui",
        "tool_composition_guide",
        "debug_game_crash",
        "debug_physics_issue",
        "fix_common_errors"
    };
    ASSERT_EQ(prompts.size(), expected_names.size());
    for (size_t i = 0; i < prompts.size(); i++) {
        EXPECT_EQ(prompts[i]["name"].get<std::string>(), expected_names[i])
            << "Prompt at index " << i << " has wrong name";
    }
}

// --- Get messages returns valid JSON ---

TEST(PromptRegistry, GetMessagesReturnsValidJson) {
    auto prompts = get_all_prompts_json();
    for (const auto& p : prompts) {
        std::string name = p["name"].get<std::string>();
        auto messages = get_prompt_messages(name, nlohmann::json::object());
        ASSERT_FALSE(messages.is_null()) << "get_prompt_messages returned null for: " << name;
        ASSERT_TRUE(messages.is_array()) << "get_prompt_messages didn't return array for: " << name;
        ASSERT_GE(messages.size(), 1) << "get_prompt_messages returned empty array for: " << name;

        // Each message should have role and content
        for (const auto& msg : messages) {
            EXPECT_TRUE(msg.contains("role")) << "Message missing 'role' for prompt: " << name;
            EXPECT_TRUE(msg.contains("content")) << "Message missing 'content' for prompt: " << name;
        }
    }
}

// --- Prompt exists test ---

TEST(PromptRegistry, PromptExists) {
    EXPECT_TRUE(prompt_exists("tool_composition_guide"));
    EXPECT_TRUE(prompt_exists("debug_game_crash"));
    EXPECT_TRUE(prompt_exists("debug_physics_issue"));
    EXPECT_TRUE(prompt_exists("fix_common_errors"));
    EXPECT_FALSE(prompt_exists("nonexistent"));
    EXPECT_FALSE(prompt_exists(""));
}

// --- Tool name cross-validation (KEY TEST for PROMPT-04) ---

static std::string extract_prompt_text(const nlohmann::json& messages) {
    std::string combined;
    for (const auto& msg : messages) {
        if (msg.contains("content")) {
            auto content = msg["content"];
            if (content.is_string()) {
                combined += content.get<std::string>();
            } else if (content.is_object() && content.contains("text")) {
                combined += content["text"].get<std::string>();
            }
        }
    }
    return combined;
}

TEST(PromptToolValidation, AllReferencedToolsExist) {
    // Build set of valid tool names
    const auto& tools = get_all_tools();
    std::set<std::string> valid_tool_names;
    for (const auto& tool : tools) {
        valid_tool_names.insert(tool.name);
    }

    // Workflow prompts that should reference tools
    std::vector<std::string> workflow_prompts = {
        "tool_composition_guide",
        "debug_game_crash",
        "debug_physics_issue",
        "fix_common_errors",
        "build_ui_layout",
        "setup_animation",
        "test_game_ui"
    };

    for (const auto& prompt_name : workflow_prompts) {
        auto messages = get_prompt_messages(prompt_name, nlohmann::json::object());
        ASSERT_FALSE(messages.is_null()) << "Prompt not found: " << prompt_name;

        std::string text = extract_prompt_text(messages);

        // Count how many valid tool names appear in this prompt's text
        int matched_count = 0;
        for (const auto& tool_name : valid_tool_names) {
            if (text.find(tool_name) != std::string::npos) {
                matched_count++;
            }
        }

        // Workflow prompts should reference at least some tools
        EXPECT_GT(matched_count, 0)
            << "Prompt '" << prompt_name << "' references no valid tool names";
    }
}

// --- Per-prompt argument variation tests ---

TEST(PromptMessages, ToolCompositionGuide_Categories) {
    // scene_setup category
    {
        auto messages = get_prompt_messages("tool_composition_guide", {{"task_category", "scene_setup"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_scene"), std::string::npos) << "scene_setup should reference create_scene";
        EXPECT_NE(text.find("create_node"), std::string::npos) << "scene_setup should reference create_node";
    }

    // debugging category
    {
        auto messages = get_prompt_messages("tool_composition_guide", {{"task_category", "debugging"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("get_scene_tree"), std::string::npos) << "debugging should reference get_scene_tree";
        EXPECT_NE(text.find("find_nodes"), std::string::npos) << "debugging should reference find_nodes";
    }

    // testing category
    {
        auto messages = get_prompt_messages("tool_composition_guide", {{"task_category", "testing"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("run_test_sequence"), std::string::npos) << "testing should reference run_test_sequence";
    }

    // unknown category (summary)
    {
        auto messages = get_prompt_messages("tool_composition_guide", {{"task_category", "unknown_cat"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("scene_setup"), std::string::npos) << "summary should list scene_setup category";
        EXPECT_NE(text.find("debugging"), std::string::npos) << "summary should list debugging category";
    }
}

TEST(PromptMessages, DebugGameCrash_ErrorTypes) {
    // crash type
    {
        auto messages = get_prompt_messages("debug_game_crash", {{"error_type", "crash"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("get_game_output"), std::string::npos) << "crash should reference get_game_output";
        EXPECT_NE(text.find("read_script"), std::string::npos) << "crash should reference read_script";
    }

    // null_reference type
    {
        auto messages = get_prompt_messages("debug_game_crash", {{"error_type", "null_reference"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("find_nodes"), std::string::npos) << "null_reference should reference find_nodes";
        EXPECT_NE(text.find("eval_in_game"), std::string::npos) << "null_reference should reference eval_in_game";
    }

    // signal_error type
    {
        auto messages = get_prompt_messages("debug_game_crash", {{"error_type", "signal_error"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("get_node_signals"), std::string::npos) << "signal_error should reference get_node_signals";
        EXPECT_NE(text.find("connect_signal"), std::string::npos) << "signal_error should reference connect_signal";
    }
}

TEST(PromptMessages, FixCommonErrors_Patterns) {
    // node_not_found
    {
        auto messages = get_prompt_messages("fix_common_errors", {{"error_pattern", "node_not_found"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("find_nodes"), std::string::npos) << "node_not_found should reference find_nodes";
        EXPECT_NE(text.find("get_scene_tree"), std::string::npos) << "node_not_found should reference get_scene_tree";
    }

    // game_not_running
    {
        auto messages = get_prompt_messages("fix_common_errors", {{"error_pattern", "game_not_running"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("run_game"), std::string::npos) << "game_not_running should reference run_game";
        EXPECT_NE(text.find("get_game_bridge_status"), std::string::npos) << "game_not_running should reference get_game_bridge_status";
    }

    // no_scene_open
    {
        auto messages = get_prompt_messages("fix_common_errors", {{"error_pattern", "no_scene_open"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("list_open_scenes"), std::string::npos) << "no_scene_open should reference list_open_scenes";
        EXPECT_NE(text.find("open_scene"), std::string::npos) << "no_scene_open should reference open_scene";
    }
}
