#include <gtest/gtest.h>
#include "script_tools.h"
#include <vector>
#include <string>

// --- edit_lines tests ---

TEST(EditLines, InsertAtLine2) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "insert", 2, "new line");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 4);
    EXPECT_EQ(result.lines[0], "a");
    EXPECT_EQ(result.lines[1], "new line");
    EXPECT_EQ(result.lines[2], "b");
    EXPECT_EQ(result.lines[3], "c");
}

TEST(EditLines, InsertAtEnd) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "insert", 4, "d");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 4);
    EXPECT_EQ(result.lines[3], "d");
}

TEST(EditLines, InsertAtLine1) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "insert", 1, "first");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 4);
    EXPECT_EQ(result.lines[0], "first");
    EXPECT_EQ(result.lines[1], "a");
}

TEST(EditLines, ReplaceSingleLine) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "replace", 2, "B");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 3);
    EXPECT_EQ(result.lines[0], "a");
    EXPECT_EQ(result.lines[1], "B");
    EXPECT_EQ(result.lines[2], "c");
}

TEST(EditLines, ReplaceRange) {
    std::vector<std::string> lines = {"a", "b", "c", "d"};
    auto result = edit_lines(lines, "replace", 2, "X", 3);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 3);
    EXPECT_EQ(result.lines[0], "a");
    EXPECT_EQ(result.lines[1], "X");
    EXPECT_EQ(result.lines[2], "d");
}

TEST(EditLines, DeleteSingleLine) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "delete", 2);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 2);
    EXPECT_EQ(result.lines[0], "a");
    EXPECT_EQ(result.lines[1], "c");
}

TEST(EditLines, DeleteRange) {
    std::vector<std::string> lines = {"a", "b", "c", "d"};
    auto result = edit_lines(lines, "delete", 2, "", 3);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.lines.size(), 2);
    EXPECT_EQ(result.lines[0], "a");
    EXPECT_EQ(result.lines[1], "d");
}

TEST(EditLines, InvalidLine0) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "insert", 0, "x");
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST(EditLines, InvalidNegativeLine) {
    std::vector<std::string> lines = {"a", "b"};
    auto result = edit_lines(lines, "replace", -1, "x");
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST(EditLines, OutOfRangeReplace) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "replace", 5, "x");
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST(EditLines, OutOfRangeDelete) {
    std::vector<std::string> lines = {"a", "b", "c"};
    auto result = edit_lines(lines, "delete", 5);
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

// --- validate_res_path tests ---

TEST(ValidateResPath, RejectsNonResPath) {
    std::string error;
    bool valid = validate_res_path("C:/foo.gd", error);
    EXPECT_FALSE(valid);
    EXPECT_FALSE(error.empty());
}

TEST(ValidateResPath, AcceptsResPath) {
    std::string error;
    bool valid = validate_res_path("res://foo.gd", error);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(error.empty());
}

TEST(ValidateResPath, AcceptsNestedResPath) {
    std::string error;
    bool valid = validate_res_path("res://scripts/player.gd", error);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(error.empty());
}

TEST(ValidateResPath, RejectsEmptyPath) {
    std::string error;
    bool valid = validate_res_path("", error);
    EXPECT_FALSE(valid);
    EXPECT_FALSE(error.empty());
}
