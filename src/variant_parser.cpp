#include "variant_parser.h"

#include <algorithm>
#include <cctype>
#include <string>

// Helper: check if all characters in a string are hexadecimal digits
static bool all_hex_digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isxdigit(c);
    });
}

// Helper: check if string looks like a Godot constructor format
// Matches: uppercase letter followed by letters/digits, then '('
// Examples: "Vector2(100, 200)", "Color(1, 0, 0, 1)", "Transform2D(1, 0, 0, 1, 0, 0)"
static bool is_godot_constructor(const std::string& s) {
    if (s.empty() || !std::isupper(static_cast<unsigned char>(s[0]))) {
        return false;
    }
    // Find the opening parenthesis
    auto paren_pos = s.find('(');
    if (paren_pos == std::string::npos || paren_pos < 2) {
        return false;
    }
    // Check that everything before '(' is alphanumeric
    for (size_t i = 1; i < paren_pos; ++i) {
        if (!std::isalnum(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    // Check that the string ends with ')'
    if (s.back() != ')') {
        return false;
    }
    return true;
}

// Helper: check if string is a valid integer (optional leading minus, then digits)
static bool is_integer_string(const std::string& s) {
    if (s.empty()) return false;
    size_t start = 0;
    if (s[0] == '-') {
        if (s.size() < 2) return false;
        start = 1;
    }
    for (size_t i = start; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

// Helper: check if string is a valid float (optional minus, digits, one decimal point, digits)
static bool is_float_string(const std::string& s) {
    if (s.empty()) return false;
    size_t start = 0;
    if (s[0] == '-') {
        if (s.size() < 2) return false;
        start = 1;
    }
    bool has_dot = false;
    bool has_digit = false;
    for (size_t i = start; i < s.size(); ++i) {
        if (s[i] == '.') {
            if (has_dot) return false; // multiple dots
            has_dot = true;
        } else if (std::isdigit(static_cast<unsigned char>(s[i]))) {
            has_digit = true;
        } else {
            return false;
        }
    }
    return has_dot && has_digit;
}

nlohmann::json parse_variant_hint(const std::string& value_str, const std::string& type_hint) {
    // a. "null" -> nil
    if (value_str == "null") {
        return {{"type", "nil"}};
    }

    // b. Godot constructor format (e.g., "Vector2(100, 200)", "Color(1, 0, 0, 1)")
    if (is_godot_constructor(value_str)) {
        return {{"type", "godot_constructor"}, {"raw", value_str}};
    }

    // c. Hex color: starts with '#', valid lengths (4, 5, 7, 9 including '#'),
    //    and all chars after '#' are hex digits
    if (!value_str.empty() && value_str[0] == '#') {
        size_t hex_len = value_str.size() - 1; // length excluding '#'
        // Valid hex color lengths: 3 (#RGB), 4 (#RGBA), 6 (#RRGGBB), 8 (#RRGGBBAA)
        if ((hex_len == 3 || hex_len == 4 || hex_len == 6 || hex_len == 8) &&
            all_hex_digits(value_str.substr(1))) {
            return {{"type", "color_hex"}, {"raw", value_str}};
        }
    }

    // d. Bool: type_hint is "bool" OR string is exactly "true"/"false"
    if (type_hint == "bool" || value_str == "true" || value_str == "false") {
        bool val = (value_str == "true");
        return {{"type", "bool"}, {"value", val}};
    }

    // e. Int: type_hint is "int" OR string is all digits with optional leading minus
    if (type_hint == "int" || (type_hint.empty() && is_integer_string(value_str))) {
        try {
            long long val = std::stoll(value_str);
            return {{"type", "int"}, {"value", val}};
        } catch (...) {
            // Fall through to string
        }
    }

    // f. Float: type_hint is "float" OR string matches number with decimal point
    if (type_hint == "float" || (type_hint.empty() && is_float_string(value_str))) {
        try {
            double val = std::stod(value_str);
            return {{"type", "float"}, {"value", val}};
        } catch (...) {
            // Fall through to string
        }
    }

    // g. Fallback: return as string
    return {{"type", "string"}, {"value", value_str}};
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

godot::Variant parse_variant(const std::string& value_str, godot::Node* node, const std::string& property) {
    godot::String godot_str(value_str.c_str());

    // 1. Try Godot's built-in parser (handles "Vector2(1,2)", "Color(1,0,0,1)", etc.)
    godot::Variant parsed = godot::UtilityFunctions::str_to_var(godot_str);
    if (parsed.get_type() != godot::Variant::NIL || value_str == "null") {
        return parsed;
    }

    // 2. Try hex color (str_to_var doesn't handle "#ff0000")
    if (!value_str.empty() && value_str[0] == '#') {
        if (godot::Color::html_is_valid(godot_str)) {
            return godot::Variant(godot::Color::html(godot_str));
        }
    }

    // 3. Type-aware parsing based on current property type
    if (node != nullptr && !property.empty()) {
        godot::Variant current = node->get(godot::StringName(property.c_str()));
        godot::Variant::Type target_type = current.get_type();

        switch (target_type) {
            case godot::Variant::BOOL:
                if (value_str == "true") return godot::Variant(true);
                if (value_str == "false") return godot::Variant(false);
                break;
            case godot::Variant::INT:
                try {
                    return godot::Variant(static_cast<int64_t>(std::stoll(value_str)));
                } catch (...) {}
                break;
            case godot::Variant::FLOAT:
                try {
                    return godot::Variant(std::stod(value_str));
                } catch (...) {}
                break;
            default:
                break;
        }
    }

    // 4. Fallback: return as Godot String
    return godot::Variant(godot_str);
}

#endif
