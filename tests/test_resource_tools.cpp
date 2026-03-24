#include <gtest/gtest.h>
#include "resource_tools.h"

// --- classify_file_type tests ---

TEST(ClassifyFileType, SceneFiles) {
    EXPECT_EQ(classify_file_type(".tscn"), "scene");
    EXPECT_EQ(classify_file_type(".scn"), "scene");
}

TEST(ClassifyFileType, ScriptFiles) {
    EXPECT_EQ(classify_file_type(".gd"), "script");
}

TEST(ClassifyFileType, ResourceFiles) {
    EXPECT_EQ(classify_file_type(".tres"), "resource");
    EXPECT_EQ(classify_file_type(".res"), "resource");
}

TEST(ClassifyFileType, ImageFiles) {
    EXPECT_EQ(classify_file_type(".png"), "image");
    EXPECT_EQ(classify_file_type(".jpg"), "image");
    EXPECT_EQ(classify_file_type(".jpeg"), "image");
    EXPECT_EQ(classify_file_type(".svg"), "image");
    EXPECT_EQ(classify_file_type(".webp"), "image");
}

TEST(ClassifyFileType, AudioFiles) {
    EXPECT_EQ(classify_file_type(".ogg"), "audio");
    EXPECT_EQ(classify_file_type(".wav"), "audio");
    EXPECT_EQ(classify_file_type(".mp3"), "audio");
}

TEST(ClassifyFileType, OtherFiles) {
    EXPECT_EQ(classify_file_type(".txt"), "other");
    EXPECT_EQ(classify_file_type(""), "other");
    EXPECT_EQ(classify_file_type(".md"), "other");
    EXPECT_EQ(classify_file_type(".json"), "other");
}

// --- truncate_script_source tests ---

TEST(TruncateScriptSource, ShortContentUnchanged) {
    // Build a 50-line script
    std::string source;
    for (int i = 1; i <= 50; i++) {
        source += "line " + std::to_string(i) + "\n";
    }
    std::string result = truncate_script_source(source, 50);
    EXPECT_EQ(result, source);
}

TEST(TruncateScriptSource, ExactlyHundredLinesUnchanged) {
    std::string source;
    for (int i = 1; i <= 100; i++) {
        source += "line " + std::to_string(i) + "\n";
    }
    std::string result = truncate_script_source(source, 100);
    EXPECT_EQ(result, source);
}

TEST(TruncateScriptSource, LongContentTruncated) {
    // Build a 150-line script
    std::string source;
    for (int i = 1; i <= 150; i++) {
        source += "line " + std::to_string(i) + "\n";
    }
    std::string result = truncate_script_source(source, 150);

    // Should contain first 50 lines
    EXPECT_NE(result.find("line 1\n"), std::string::npos);
    EXPECT_NE(result.find("line 50\n"), std::string::npos);

    // Should NOT contain line 51+
    EXPECT_EQ(result.find("line 51\n"), std::string::npos);

    // Should contain truncation notice
    EXPECT_NE(result.find("[...truncated, 150 total lines...]"), std::string::npos);
}

TEST(TruncateScriptSource, EmptyContent) {
    std::string result = truncate_script_source("", 0);
    EXPECT_EQ(result, "");
}
