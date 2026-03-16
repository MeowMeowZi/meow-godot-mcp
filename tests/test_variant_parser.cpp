#include <gtest/gtest.h>
#include "variant_parser.h"

using json = nlohmann::json;

// --- Godot constructor format (str_to_var path at runtime) ---

TEST(VariantParser, Vector2ConstructorDetected) {
    auto result = parse_variant_hint("Vector2(100, 200)");
    EXPECT_EQ(result["type"], "godot_constructor");
    EXPECT_EQ(result["raw"], "Vector2(100, 200)");
}

TEST(VariantParser, ColorConstructorDetected) {
    auto result = parse_variant_hint("Color(1, 0, 0, 1)");
    EXPECT_EQ(result["type"], "godot_constructor");
    EXPECT_EQ(result["raw"], "Color(1, 0, 0, 1)");
}

// --- Hex color parsing ---

TEST(VariantParser, HexColor6Digit) {
    auto result = parse_variant_hint("#ff0000");
    EXPECT_EQ(result["type"], "color_hex");
    EXPECT_EQ(result["raw"], "#ff0000");
}

TEST(VariantParser, HexColor8DigitWithAlpha) {
    auto result = parse_variant_hint("#FF00FF80");
    EXPECT_EQ(result["type"], "color_hex");
    EXPECT_EQ(result["raw"], "#FF00FF80");
}

TEST(VariantParser, InvalidHexNotColor) {
    // "invalid-hex" starts with a letter, not '#', should NOT be color
    auto result = parse_variant_hint("invalid-hex");
    EXPECT_NE(result["type"], "color_hex");
    // Should fall through to string
    EXPECT_EQ(result["type"], "string");
}

// --- Type-hinted parsing ---

TEST(VariantParser, IntWithTypeHint) {
    auto result = parse_variant_hint("42", "int");
    EXPECT_EQ(result["type"], "int");
    EXPECT_EQ(result["value"], 42);
}

TEST(VariantParser, FloatWithTypeHint) {
    auto result = parse_variant_hint("3.14", "float");
    EXPECT_EQ(result["type"], "float");
    EXPECT_NEAR(result["value"].get<double>(), 3.14, 0.001);
}

TEST(VariantParser, BoolTrueWithTypeHint) {
    auto result = parse_variant_hint("true", "bool");
    EXPECT_EQ(result["type"], "bool");
    EXPECT_EQ(result["value"], true);
}

TEST(VariantParser, BoolFalseWithTypeHint) {
    auto result = parse_variant_hint("false", "bool");
    EXPECT_EQ(result["type"], "bool");
    EXPECT_EQ(result["value"], false);
}

// --- String fallback ---

TEST(VariantParser, PlainStringFallback) {
    auto result = parse_variant_hint("hello world");
    EXPECT_EQ(result["type"], "string");
    EXPECT_EQ(result["value"], "hello world");
}

TEST(VariantParser, EmptyStringReturnsString) {
    auto result = parse_variant_hint("");
    EXPECT_EQ(result["type"], "string");
    EXPECT_EQ(result["value"], "");
}

// --- Null ---

TEST(VariantParser, NullStringReturnsNil) {
    auto result = parse_variant_hint("null");
    EXPECT_EQ(result["type"], "nil");
}

// --- Additional edge cases ---

TEST(VariantParser, BoolTrueWithoutHint) {
    // "true" should be detected as bool even without type_hint
    auto result = parse_variant_hint("true");
    EXPECT_EQ(result["type"], "bool");
    EXPECT_EQ(result["value"], true);
}

TEST(VariantParser, BoolFalseWithoutHint) {
    auto result = parse_variant_hint("false");
    EXPECT_EQ(result["type"], "bool");
    EXPECT_EQ(result["value"], false);
}

TEST(VariantParser, NegativeInt) {
    auto result = parse_variant_hint("-123", "int");
    EXPECT_EQ(result["type"], "int");
    EXPECT_EQ(result["value"], -123);
}

TEST(VariantParser, NegativeFloat) {
    auto result = parse_variant_hint("-2.5", "float");
    EXPECT_EQ(result["type"], "float");
    EXPECT_NEAR(result["value"].get<double>(), -2.5, 0.001);
}

TEST(VariantParser, BareIntegerAutoDetected) {
    // Without type hint, a bare integer string should still be detected as int
    auto result = parse_variant_hint("999");
    EXPECT_EQ(result["type"], "int");
    EXPECT_EQ(result["value"], 999);
}

TEST(VariantParser, BareFloatAutoDetected) {
    // Without type hint, a number with decimal should be detected as float
    auto result = parse_variant_hint("1.5");
    EXPECT_EQ(result["type"], "float");
    EXPECT_NEAR(result["value"].get<double>(), 1.5, 0.001);
}

TEST(VariantParser, Transform2DConstructor) {
    auto result = parse_variant_hint("Transform2D(1, 0, 0, 1, 0, 0)");
    EXPECT_EQ(result["type"], "godot_constructor");
    EXPECT_EQ(result["raw"], "Transform2D(1, 0, 0, 1, 0, 0)");
}

TEST(VariantParser, Vector3Constructor) {
    auto result = parse_variant_hint("Vector3(1, 2, 3)");
    EXPECT_EQ(result["type"], "godot_constructor");
    EXPECT_EQ(result["raw"], "Vector3(1, 2, 3)");
}

TEST(VariantParser, HexColor3Digit) {
    // 3-digit shorthand: #RGB
    auto result = parse_variant_hint("#f00");
    EXPECT_EQ(result["type"], "color_hex");
    EXPECT_EQ(result["raw"], "#f00");
}

TEST(VariantParser, HexColor4Digit) {
    // 4-digit shorthand: #RGBA
    auto result = parse_variant_hint("#f00f");
    EXPECT_EQ(result["type"], "color_hex");
    EXPECT_EQ(result["raw"], "#f00f");
}

TEST(VariantParser, InvalidHexTooShort) {
    // "#ab" is only 2 hex digits -- not a valid color format
    auto result = parse_variant_hint("#ab");
    EXPECT_NE(result["type"], "color_hex");
}

TEST(VariantParser, InvalidHexNonHexChars) {
    // "#gggggg" has non-hex characters
    auto result = parse_variant_hint("#gggggg");
    EXPECT_NE(result["type"], "color_hex");
}
