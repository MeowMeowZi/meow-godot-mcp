#include <gtest/gtest.h>
#include "error_enrichment.h"

// ========================================================================
// Levenshtein distance tests
// ========================================================================

TEST(LevenshteinDistance, KnownPairKittenSitting) {
    EXPECT_EQ(levenshtein_distance("kitten", "sitting"), 3);
}

TEST(LevenshteinDistance, SingleCharTypoSprite2D) {
    EXPECT_EQ(levenshtein_distance("Sprit2D", "Sprite2D"), 1);
}

TEST(LevenshteinDistance, ExactMatch) {
    EXPECT_EQ(levenshtein_distance("abc", "abc"), 0);
}

TEST(LevenshteinDistance, EmptyFirst) {
    EXPECT_EQ(levenshtein_distance("", "abc"), 3);
}

TEST(LevenshteinDistance, EmptySecond) {
    EXPECT_EQ(levenshtein_distance("abc", ""), 3);
}

TEST(LevenshteinDistance, MissingCharacterBody) {
    EXPECT_EQ(levenshtein_distance("CharacterBody2D", "CharacterBdy2D"), 1);
}

// ========================================================================
// Error categorization tests
// ========================================================================

TEST(CategorizeError, NodeNotFoundPrefix) {
    EXPECT_EQ(categorize_error("Node not found: Player/Sprite2D", "set_node_property"),
              ErrorCategory::NODE_NOT_FOUND);
}

TEST(CategorizeError, ParentNotFoundPrefix) {
    EXPECT_EQ(categorize_error("Parent not found: Player", "create_node"),
              ErrorCategory::NODE_NOT_FOUND);
}

TEST(CategorizeError, SourceNodeNotFoundPrefix) {
    EXPECT_EQ(categorize_error("Source node not found: Player", "connect_signal"),
              ErrorCategory::NODE_NOT_FOUND);
}

TEST(CategorizeError, NoSceneOpen) {
    EXPECT_EQ(categorize_error("No scene open", "create_node"),
              ErrorCategory::NO_SCENE_OPEN);
}

TEST(CategorizeError, GameBridgeNotInitialized) {
    EXPECT_EQ(categorize_error("Game bridge not initialized", "inject_input"),
              ErrorCategory::GAME_NOT_RUNNING);
}

TEST(CategorizeError, NoGameRunningOrBridgeNotConnected) {
    EXPECT_EQ(categorize_error("No game running or bridge not connected", "click_node"),
              ErrorCategory::GAME_NOT_RUNNING);
}

TEST(CategorizeError, GameIsNotCurrentlyRunning) {
    EXPECT_EQ(categorize_error("Game is not currently running", "stop_game"),
              ErrorCategory::GAME_NOT_RUNNING);
}

TEST(CategorizeError, UnknownClassPrefix) {
    EXPECT_EQ(categorize_error("Unknown class: Sprit2D", "create_node"),
              ErrorCategory::UNKNOWN_CLASS);
}

TEST(CategorizeError, IsNotANodeTypeSuffix) {
    EXPECT_EQ(categorize_error("Sprit2D is not a Node type", "create_node"),
              ErrorCategory::UNKNOWN_CLASS);
}

TEST(CategorizeError, UnknownPreset) {
    EXPECT_EQ(categorize_error("Unknown preset: blargh", "set_layout_preset"),
              ErrorCategory::TYPE_MISMATCH);
}

TEST(CategorizeError, UnknownInputType) {
    EXPECT_EQ(categorize_error("Unknown input type: blah", "inject_input"),
              ErrorCategory::TYPE_MISMATCH);
}

TEST(CategorizeError, ScriptFileNotFound) {
    EXPECT_EQ(categorize_error("File not found: res://foo.gd", "read_script"),
              ErrorCategory::SCRIPT_ERROR);
}

TEST(CategorizeError, ResourceNotFound) {
    EXPECT_EQ(categorize_error("Resource not found: res://bar.tres", "get_resource_info"),
              ErrorCategory::RESOURCE_ERROR);
}

TEST(CategorizeError, DeferredPending) {
    EXPECT_EQ(categorize_error("Another deferred request is already pending", "capture_game_viewport"),
              ErrorCategory::DEFERRED_PENDING);
}

TEST(CategorizeError, GenericFallback) {
    EXPECT_EQ(categorize_error("Something unexpected happened", "create_node"),
              ErrorCategory::GENERIC);
}

// ========================================================================
// Enrichment output tests (pure C++ path)
// ========================================================================

TEST(EnrichError, NoSceneOpenSuggestsOpenScene) {
    auto result = enrich_error("No scene open", "create_node");
    EXPECT_NE(result.find("open_scene"), std::string::npos);
}

TEST(EnrichError, NoSceneOpenSuggestsCreateScene) {
    auto result = enrich_error("No scene open", "create_node");
    EXPECT_NE(result.find("create_scene"), std::string::npos);
}

TEST(EnrichError, GameNotRunningSuggestsRunGame) {
    auto result = enrich_error("Game bridge not initialized", "inject_input");
    EXPECT_NE(result.find("run_game"), std::string::npos);
}

TEST(EnrichError, ScriptErrorSuggestsListProjectFiles) {
    auto result = enrich_error("File not found: res://foo.gd", "read_script");
    EXPECT_NE(result.find("list_project_files"), std::string::npos);
}

TEST(EnrichError, ResourceErrorSuggestsListProjectFiles) {
    auto result = enrich_error("Resource not found: res://bar.tres", "get_resource_info");
    EXPECT_NE(result.find("list_project_files"), std::string::npos);
}

TEST(EnrichError, DeferredPendingSuggestsWait) {
    auto result = enrich_error("Another deferred request is already pending", "click_node");
    EXPECT_NE(result.find("Wait"), std::string::npos);
}

TEST(EnrichError, GenericPreservesOriginalMessage) {
    auto result = enrich_error("Something unexpected", "create_node");
    // Original message should be at the start
    EXPECT_EQ(result.find("Something unexpected"), 0u);
}

TEST(EnrichError, EveryEnrichmentContainsOriginalMessage) {
    // Test that all categories preserve the original message as prefix
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"No scene open", "create_node"},
        {"Game bridge not initialized", "inject_input"},
        {"Node not found: Player", "set_node_property"},
        {"Unknown class: Foo", "create_node"},
        {"Unknown preset: bad", "set_layout_preset"},
        {"File not found: res://x.gd", "read_script"},
        {"Resource not found: res://x.tres", "get_resource_info"},
        {"Another deferred request is already pending", "click_node"},
        {"Something unexpected", "create_node"},
    };
    for (const auto& [msg, tool] : test_cases) {
        auto result = enrich_error(msg, tool);
        EXPECT_EQ(result.find(msg), 0u) << "Original message not preserved for: " << msg;
    }
}

// ========================================================================
// enrich_node_not_found with mock sibling list tests
// ========================================================================

TEST(EnrichNodeNotFound, FuzzyMatchSuggestions) {
    std::vector<std::string> siblings = {"Sprite2D", "Camera2D", "CollisionShape2D"};
    auto result = enrich_node_not_found("Node not found: Sprit2D", "set_node_property", siblings);
    EXPECT_NE(result.find("Did you mean"), std::string::npos);
    EXPECT_NE(result.find("Sprite2D"), std::string::npos);
}

TEST(EnrichNodeNotFound, Max3Suggestions) {
    std::vector<std::string> siblings = {"ab", "ac", "ad", "ae", "af"};
    auto result = enrich_node_not_found("Node not found: aa", "set_node_property", siblings);
    // Should contain "Did you mean" with at most 3 suggestions
    EXPECT_NE(result.find("Did you mean"), std::string::npos);
    // Count commas in "Did you mean" section - max 2 commas for 3 items
    auto did_you_mean_pos = result.find("Did you mean: ");
    auto question_mark_pos = result.find("?", did_you_mean_pos);
    std::string suggestion_section = result.substr(did_you_mean_pos, question_mark_pos - did_you_mean_pos);
    int comma_count = 0;
    for (char c : suggestion_section) if (c == ',') comma_count++;
    EXPECT_LE(comma_count, 2);
}

TEST(EnrichNodeNotFound, ListsAvailableChildren) {
    std::vector<std::string> siblings = {"Sprite2D", "Camera2D"};
    auto result = enrich_node_not_found("Node not found: Player", "set_node_property", siblings);
    EXPECT_NE(result.find("Available children"), std::string::npos);
    EXPECT_NE(result.find("Sprite2D"), std::string::npos);
    EXPECT_NE(result.find("Camera2D"), std::string::npos);
}

TEST(EnrichNodeNotFound, SuggestsGetSceneTree) {
    std::vector<std::string> siblings = {"Sprite2D"};
    auto result = enrich_node_not_found("Node not found: Player", "set_node_property", siblings);
    EXPECT_NE(result.find("get_scene_tree"), std::string::npos);
}

// ========================================================================
// enrich_unknown_class with mock class list tests
// ========================================================================

TEST(EnrichUnknownClass, FuzzyMatchSuggestions) {
    std::vector<std::string> classes = {"Sprite2D", "Sprite3D", "Node2D", "Node3D"};
    auto result = enrich_unknown_class("Unknown class: Sprit2D", "create_node", classes);
    EXPECT_NE(result.find("Did you mean"), std::string::npos);
    EXPECT_NE(result.find("Sprite2D"), std::string::npos);
}

TEST(EnrichUnknownClass, IsNotANodeTypePattern) {
    std::vector<std::string> classes = {"Sprite2D", "Sprite3D"};
    auto result = enrich_unknown_class("Sprit2D is not a Node type", "create_node", classes);
    EXPECT_NE(result.find("Did you mean"), std::string::npos);
}

TEST(EnrichUnknownClass, SuggestsGetSceneTree) {
    std::vector<std::string> classes = {"Node2D"};
    auto result = enrich_unknown_class("Unknown class: Foo", "create_node", classes);
    EXPECT_NE(result.find("get_scene_tree"), std::string::npos);
}
