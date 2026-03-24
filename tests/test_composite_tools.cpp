#include <gtest/gtest.h>
#include "composite_tools.h"

// --- find_nodes_match_name tests (pure C++, no Godot dependency) ---

TEST(FindNodesMatchName, ExactMatch) {
    EXPECT_TRUE(find_nodes_match_name("Player", "Player"));
}

TEST(FindNodesMatchName, ExactMatch_NoMatch) {
    EXPECT_FALSE(find_nodes_match_name("Enemy", "Player"));
}

TEST(FindNodesMatchName, GlobStarSuffix) {
    // "Player*" should match "PlayerSprite"
    EXPECT_TRUE(find_nodes_match_name("Player*", "PlayerSprite"));
    EXPECT_TRUE(find_nodes_match_name("Player*", "Player"));
    EXPECT_FALSE(find_nodes_match_name("Player*", "EnemyPlayer"));
}

TEST(FindNodesMatchName, GlobStarPrefix) {
    // "*Sprite" should match "PlayerSprite"
    EXPECT_TRUE(find_nodes_match_name("*Sprite", "PlayerSprite"));
    EXPECT_TRUE(find_nodes_match_name("*Sprite", "Sprite"));
    EXPECT_FALSE(find_nodes_match_name("*Sprite", "SpriteSheet"));
}

TEST(FindNodesMatchName, GlobContains) {
    // "*Sprite*" should match "PlayerSprite2D"
    EXPECT_TRUE(find_nodes_match_name("*Sprite*", "PlayerSprite"));
    EXPECT_TRUE(find_nodes_match_name("*Sprite*", "PlayerSprite2D"));
    EXPECT_TRUE(find_nodes_match_name("*Sprite*", "Sprite"));
}

TEST(FindNodesMatchName, CaseInsensitive) {
    EXPECT_TRUE(find_nodes_match_name("player", "Player"));
    EXPECT_TRUE(find_nodes_match_name("PLAYER", "player"));
    EXPECT_TRUE(find_nodes_match_name("Player", "PLAYER"));
}

TEST(FindNodesMatchName, CaseInsensitiveGlob) {
    EXPECT_TRUE(find_nodes_match_name("*label*", "ScoreLabel"));
    EXPECT_TRUE(find_nodes_match_name("*LABEL*", "ScoreLabel"));
    EXPECT_TRUE(find_nodes_match_name("*Label*", "scorelabel"));
}

TEST(FindNodesMatchName, SubstringMatchNoWildcard) {
    // Without wildcards, should do substring match
    EXPECT_TRUE(find_nodes_match_name("Sprite", "PlayerSprite2D"));
    EXPECT_TRUE(find_nodes_match_name("play", "PlayerNode"));
    EXPECT_FALSE(find_nodes_match_name("Enemy", "PlayerNode"));
}

TEST(FindNodesMatchName, EmptyPatternMatchesAll) {
    EXPECT_TRUE(find_nodes_match_name("", "AnyNode"));
    EXPECT_TRUE(find_nodes_match_name("", ""));
}

TEST(FindNodesMatchName, MultipleWildcards) {
    // "P*er*te" should match "PlayerSprite"
    EXPECT_TRUE(find_nodes_match_name("P*er*te", "PlayerSprite"));
    // "A*B*C" should match "AxxByyC"
    EXPECT_TRUE(find_nodes_match_name("A*B*C", "AxxByyC"));
    EXPECT_FALSE(find_nodes_match_name("A*B*C", "AxxByyD"));
}
