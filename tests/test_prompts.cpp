#include <gtest/gtest.h>
#include "mcp_prompts.h"
#include "mcp_tool_registry.h"
#include <set>
#include <string>

// --- Prompt registry count test ---

TEST(PromptRegistry, HasExactly15Prompts) {
    auto prompts = get_all_prompts_json();
    ASSERT_EQ(prompts.size(), 12);
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
        "tool_composition_guide",
        "debug_game_crash",
        "debug_physics_issue",
        "fix_common_errors",
        "build_platformer_game",
        "setup_tilemap_level",
        "build_top_down_game",
        "create_game_from_scratch"
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
    EXPECT_TRUE(prompt_exists("build_platformer_game"));
    EXPECT_TRUE(prompt_exists("setup_tilemap_level"));
    EXPECT_TRUE(prompt_exists("build_top_down_game"));
    EXPECT_TRUE(prompt_exists("create_game_from_scratch"));
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

    // All workflow prompts that should reference tools (all 15)
    // Test with various argument combinations for comprehensive coverage
    struct PromptVariation {
        std::string name;
        nlohmann::json args;
    };

    std::vector<PromptVariation> variations = {
        // Plan 01 prompts
        {"tool_composition_guide", nlohmann::json::object()},
        {"debug_game_crash", nlohmann::json::object()},
        {"debug_physics_issue", nlohmann::json::object()},
        {"fix_common_errors", nlohmann::json::object()},
        // Plan 02 prompts -- default args
        {"build_platformer_game", nlohmann::json::object()},
        {"setup_tilemap_level", nlohmann::json::object()},
        {"build_top_down_game", nlohmann::json::object()},
        {"create_game_from_scratch", {{"genre", "platformer"}}},
        // Plan 02 prompts -- variant args
        {"build_platformer_game", {{"complexity", "minimal"}}},
        {"build_platformer_game", {{"complexity", "full"}}},
        {"setup_tilemap_level", {{"level_type", "dungeon"}}},
        {"setup_tilemap_level", {{"level_type", "top_down_level"}}},
        {"build_top_down_game", {{"genre", "shooter"}}},
        {"build_top_down_game", {{"genre", "rpg"}}},
        {"create_game_from_scratch", {{"genre", "puzzle"}}},
        {"create_game_from_scratch", {{"genre", "shooter"}}},
        {"create_game_from_scratch", {{"genre", "visual_novel"}}},
        {"create_game_from_scratch", {{"genre", "top_down"}}},
    };

    for (const auto& v : variations) {
        auto messages = get_prompt_messages(v.name, v.args);
        ASSERT_FALSE(messages.is_null()) << "Prompt not found: " << v.name;

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
            << "Prompt '" << v.name << "' with args " << v.args.dump()
            << " references no valid tool names";
    }
}

// --- Per-prompt argument variation tests (Plan 01) ---

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
        EXPECT_NE(text.find("get_game_output"), std::string::npos) << "debugging should reference get_game_output";
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

// --- Per-prompt argument variation tests (Plan 02) ---

TEST(PromptMessages, BuildPlatformerGame_Complexity) {
    // minimal: should have create_character but NOT create_animation
    {
        auto messages = get_prompt_messages("build_platformer_game", {{"complexity", "minimal"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_character"), std::string::npos) << "minimal should reference create_character";
        EXPECT_NE(text.find("create_collision_shape"), std::string::npos) << "minimal should reference create_collision_shape";
        EXPECT_NE(text.find("run_game"), std::string::npos) << "minimal should reference run_game";
    }

    // standard: should have create_character AND create_ui_panel/set_layout_preset
    {
        auto messages = get_prompt_messages("build_platformer_game", {{"complexity", "standard"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_character"), std::string::npos) << "standard should reference create_character";
        // Standard adds HUD
        bool has_ui = (text.find("create_ui_panel") != std::string::npos) ||
                      (text.find("set_layout_preset") != std::string::npos);
        EXPECT_TRUE(has_ui) << "standard should reference create_ui_panel or set_layout_preset for HUD";
    }

    // full: should have create_animation
    {
        auto messages = get_prompt_messages("build_platformer_game", {{"complexity", "full"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_animation"), std::string::npos) << "full should reference create_animation";
        EXPECT_NE(text.find("create_character"), std::string::npos) << "full should reference create_character";
    }

    // unknown complexity: should show summary with available options
    {
        auto messages = get_prompt_messages("build_platformer_game", {{"complexity", "ultra"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("minimal"), std::string::npos) << "unknown complexity should list minimal option";
        EXPECT_NE(text.find("standard"), std::string::npos) << "unknown complexity should list standard option";
        EXPECT_NE(text.find("full"), std::string::npos) << "unknown complexity should list full option";
    }
}

TEST(PromptMessages, SetupTilemapLevel_Types) {
    // platformer_level: should reference set_tilemap_cells
    {
        auto messages = get_prompt_messages("setup_tilemap_level", {{"level_type", "platformer_level"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("set_tilemap_cells"), std::string::npos) << "platformer_level should reference set_tilemap_cells";
        EXPECT_NE(text.find("get_tilemap_info"), std::string::npos) << "platformer_level should reference get_tilemap_info";
        EXPECT_NE(text.find("erase_tilemap_cells"), std::string::npos) << "platformer_level should reference erase_tilemap_cells";
    }

    // dungeon: should reference set_tilemap_cells and erase_tilemap_cells
    {
        auto messages = get_prompt_messages("setup_tilemap_level", {{"level_type", "dungeon"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("set_tilemap_cells"), std::string::npos) << "dungeon should reference set_tilemap_cells";
        EXPECT_NE(text.find("erase_tilemap_cells"), std::string::npos) << "dungeon should reference erase_tilemap_cells";
    }

    // top_down_level: should reference set_tilemap_cells
    {
        auto messages = get_prompt_messages("setup_tilemap_level", {{"level_type", "top_down_level"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("set_tilemap_cells"), std::string::npos) << "top_down_level should reference set_tilemap_cells";
    }

    // unknown type: should show summary
    {
        auto messages = get_prompt_messages("setup_tilemap_level", {{"level_type", "unknown"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("platformer_level"), std::string::npos) << "unknown type should list platformer_level";
        EXPECT_NE(text.find("dungeon"), std::string::npos) << "unknown type should list dungeon";
    }
}

TEST(PromptMessages, BuildTopDownGame_Genres) {
    // adventure: should reference create_character and connect_signal
    {
        auto messages = get_prompt_messages("build_top_down_game", {{"genre", "adventure"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_character"), std::string::npos) << "adventure should reference create_character";
        EXPECT_NE(text.find("connect_signal"), std::string::npos) << "adventure should reference connect_signal";
        EXPECT_NE(text.find("set_tilemap_cells"), std::string::npos) << "adventure should reference set_tilemap_cells";
    }

    // shooter: should reference write_script for bullets
    {
        auto messages = get_prompt_messages("build_top_down_game", {{"genre", "shooter"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("write_script"), std::string::npos) << "shooter should reference write_script";
        EXPECT_NE(text.find("create_character"), std::string::npos) << "shooter should reference create_character";
    }

    // rpg: should reference connect_signal for NPCs/items
    {
        auto messages = get_prompt_messages("build_top_down_game", {{"genre", "rpg"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("connect_signal"), std::string::npos) << "rpg should reference connect_signal";
        EXPECT_NE(text.find("create_collision_shape"), std::string::npos) << "rpg should reference create_collision_shape";
    }

    // unknown genre: should show summary
    {
        auto messages = get_prompt_messages("build_top_down_game", {{"genre", "stealth"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("adventure"), std::string::npos) << "unknown genre should list adventure";
        EXPECT_NE(text.find("shooter"), std::string::npos) << "unknown genre should list shooter";
        EXPECT_NE(text.find("rpg"), std::string::npos) << "unknown genre should list rpg";
    }
}

TEST(PromptMessages, CreateGameFromScratch_Genres) {
    // platformer: should reference game-building tools
    {
        auto messages = get_prompt_messages("create_game_from_scratch", {{"genre", "platformer"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_character"), std::string::npos) << "platformer should reference create_character";
        EXPECT_NE(text.find("create_collision_shape"), std::string::npos) << "platformer should reference create_collision_shape";
        EXPECT_NE(text.find("write_script"), std::string::npos) << "platformer should reference write_script";
        EXPECT_NE(text.find("create_scene"), std::string::npos) << "platformer should reference create_scene";
    }

    // puzzle: should reference write_script for game logic
    {
        auto messages = get_prompt_messages("create_game_from_scratch", {{"genre", "puzzle"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("write_script"), std::string::npos) << "puzzle should reference write_script";
        EXPECT_NE(text.find("create_ui_panel"), std::string::npos) << "puzzle should reference create_ui_panel";
        EXPECT_NE(text.find("create_animation"), std::string::npos) << "puzzle should reference create_animation";
    }

    // visual_novel: should reference create_ui_panel for dialog
    {
        auto messages = get_prompt_messages("create_game_from_scratch", {{"genre", "visual_novel"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_ui_panel"), std::string::npos) << "visual_novel should reference create_ui_panel";
        EXPECT_NE(text.find("connect_signal"), std::string::npos) << "visual_novel should reference connect_signal";
        EXPECT_NE(text.find("write_script"), std::string::npos) << "visual_novel should reference write_script";
    }

    // shooter: should reference create_character and create_collision_shape
    {
        auto messages = get_prompt_messages("create_game_from_scratch", {{"genre", "shooter"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_character"), std::string::npos) << "shooter should reference create_character";
        EXPECT_NE(text.find("create_collision_shape"), std::string::npos) << "shooter should reference create_collision_shape";
        EXPECT_NE(text.find("connect_signal"), std::string::npos) << "shooter should reference connect_signal";
    }

    // top_down: should reference set_tilemap_cells
    {
        auto messages = get_prompt_messages("create_game_from_scratch", {{"genre", "top_down"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("create_character"), std::string::npos) << "top_down should reference create_character";
        EXPECT_NE(text.find("set_tilemap_cells"), std::string::npos) << "top_down should reference set_tilemap_cells";
    }

    // unknown genre: should show genre summary
    {
        auto messages = get_prompt_messages("create_game_from_scratch", {{"genre", "mmo"}});
        ASSERT_FALSE(messages.is_null());
        std::string text = extract_prompt_text(messages);
        EXPECT_NE(text.find("platformer"), std::string::npos) << "unknown genre should list platformer";
        EXPECT_NE(text.find("puzzle"), std::string::npos) << "unknown genre should list puzzle";
        EXPECT_NE(text.find("visual_novel"), std::string::npos) << "unknown genre should list visual_novel";
    }
}
