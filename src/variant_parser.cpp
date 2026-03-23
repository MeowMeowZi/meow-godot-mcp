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

    // a2. Resource path: starts with "res://"
    if (value_str.size() >= 6 && value_str.compare(0, 6, "res://") == 0) {
        return {{"type", "resource_path"}, {"raw", value_str}};
    }

    // a3. Inline resource creation: starts with "new:"
    if (value_str.size() >= 4 && value_str.compare(0, 4, "new:") == 0) {
        return {{"type", "resource_new"}, {"raw", value_str}};
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
        char* end = nullptr;
        long long val = std::strtoll(value_str.c_str(), &end, 10);
        if (end != value_str.c_str() && *end == '\0') {
            return {{"type", "int"}, {"value", val}};
        }
        // Fall through to string
    }

    // f. Float: type_hint is "float" OR string matches number with decimal point
    if (type_hint == "float" || (type_hint.empty() && is_float_string(value_str))) {
        char* end = nullptr;
        double val = std::strtod(value_str.c_str(), &end);
        if (end != value_str.c_str() && *end == '\0') {
            return {{"type", "float"}, {"value", val}};
        }
        // Fall through to string
    }

    // g. Fallback: return as string
    return {{"type", "string"}, {"value", value_str}};
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>

// Helper: split "key=value, key2=value2" respecting nested parentheses
static std::vector<std::pair<std::string, std::string>> parse_inline_properties(const std::string& content) {
    std::vector<std::pair<std::string, std::string>> result;
    if (content.empty()) return result;

    int depth = 0;
    size_t start = 0;

    // Split by commas at depth 0
    std::vector<std::string> segments;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '(') ++depth;
        else if (content[i] == ')') --depth;
        else if (content[i] == ',' && depth == 0) {
            segments.push_back(content.substr(start, i - start));
            start = i + 1;
        }
    }
    segments.push_back(content.substr(start));

    // Parse each "key=value" segment
    for (auto& seg : segments) {
        // Trim whitespace
        size_t s = seg.find_first_not_of(" \t");
        size_t e = seg.find_last_not_of(" \t");
        if (s == std::string::npos) continue;
        seg = seg.substr(s, e - s + 1);

        auto eq_pos = seg.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = seg.substr(0, eq_pos);
        std::string val = seg.substr(eq_pos + 1);

        // Trim key and value
        size_t ks = key.find_first_not_of(" \t");
        size_t ke = key.find_last_not_of(" \t");
        if (ks != std::string::npos) key = key.substr(ks, ke - ks + 1);

        size_t vs = val.find_first_not_of(" \t");
        size_t ve = val.find_last_not_of(" \t");
        if (vs != std::string::npos) val = val.substr(vs, ve - vs + 1);

        if (!key.empty()) {
            result.push_back({key, val});
        }
    }
    return result;
}

godot::Variant parse_variant(const std::string& value_str, godot::Node* node, const std::string& property) {
    godot::String godot_str(value_str.c_str());

    // 1. Resource path: "res://..." -> load via ResourceLoader
    if (value_str.size() >= 6 && value_str.compare(0, 6, "res://") == 0) {
        if (godot::ResourceLoader::get_singleton()->exists(godot_str)) {
            godot::Ref<godot::Resource> res = godot::ResourceLoader::get_singleton()->load(godot_str);
            if (res.is_valid()) {
                return godot::Variant(res);
            }
        }
        // Return nil with error context (caller will see property unchanged)
        godot::UtilityFunctions::push_warning(godot::String(("MCP Meow: Resource not found: " + value_str).c_str()));
        return godot::Variant();
    }

    // 2. Inline resource creation: "new:ClassName(prop=val, ...)"
    if (value_str.size() >= 4 && value_str.compare(0, 4, "new:") == 0) {
        std::string spec = value_str.substr(4); // "ClassName(prop=val, ...)"

        // Extract class name and properties content
        auto paren_pos = spec.find('(');
        std::string class_name;
        std::string props_content;

        if (paren_pos != std::string::npos && spec.back() == ')') {
            class_name = spec.substr(0, paren_pos);
            props_content = spec.substr(paren_pos + 1, spec.size() - paren_pos - 2);
        } else {
            // No parentheses: just a class name like "new:RectangleShape2D"
            class_name = spec;
        }

        // Validate class exists and is a Resource subclass
        godot::StringName gd_class(class_name.c_str());
        if (!godot::ClassDB::class_exists(gd_class)) {
            godot::UtilityFunctions::push_warning(godot::String(("MCP Meow: Inline resource creation failed - unknown class: " + class_name).c_str()));
            return godot::Variant(); // Unknown class
        }
        if (!godot::ClassDB::is_parent_class(gd_class, godot::StringName("Resource"))) {
            godot::UtilityFunctions::push_warning(godot::String(("MCP Meow: Inline resource creation failed - " + class_name + " is not a Resource subclass").c_str()));
            return godot::Variant(); // Not a Resource
        }

        // Instantiate the resource
        godot::Variant instance = godot::ClassDB::instantiate(gd_class);
        godot::Object* obj = instance.operator godot::Object*();
        if (!obj) {
            godot::UtilityFunctions::push_warning(godot::String(("MCP Meow: Failed to instantiate resource: " + class_name).c_str()));
            return godot::Variant();
        }

        // Set properties
        if (!props_content.empty()) {
            auto props = parse_inline_properties(props_content);
            for (auto& [key, val] : props) {
                // Recursively parse property values (handles Vector2, float, etc.)
                godot::Variant prop_val = parse_variant(val, nullptr, "");
                obj->set(godot::StringName(key.c_str()), prop_val);
            }
        }

        // Wrap in Ref to keep alive
        godot::Ref<godot::Resource> ref(godot::Object::cast_to<godot::Resource>(obj));
        if (ref.is_valid()) {
            return godot::Variant(ref);
        }
        return godot::Variant();
    }

    // 3. Try Godot's built-in parser (handles "Vector2(1,2)", "Color(1,0,0,1)", etc.)
    godot::Variant parsed = godot::UtilityFunctions::str_to_var(godot_str);
    if (parsed.get_type() != godot::Variant::NIL || value_str == "null") {
        return parsed;
    }

    // 4. Try hex color (str_to_var doesn't handle "#ff0000")
    if (!value_str.empty() && value_str[0] == '#') {
        if (godot::Color::html_is_valid(godot_str)) {
            return godot::Variant(godot::Color::html(godot_str));
        }
    }

    // 5. Type-aware parsing based on current property type
    if (node != nullptr && !property.empty()) {
        godot::Variant current = node->get(godot::StringName(property.c_str()));
        godot::Variant::Type target_type = current.get_type();

        switch (target_type) {
            case godot::Variant::BOOL:
                if (value_str == "true") return godot::Variant(true);
                if (value_str == "false") return godot::Variant(false);
                break;
            case godot::Variant::INT: {
                char* end = nullptr;
                long long ival = std::strtoll(value_str.c_str(), &end, 10);
                if (end != value_str.c_str() && *end == '\0') {
                    return godot::Variant(static_cast<int64_t>(ival));
                }
                break;
            }
            case godot::Variant::FLOAT: {
                char* end = nullptr;
                double dval = std::strtod(value_str.c_str(), &end);
                if (end != value_str.c_str() && *end == '\0') {
                    return godot::Variant(dval);
                }
                break;
            }
            case godot::Variant::OBJECT: {
                // Target property is a Resource — try loading from path or inline creation
                if (value_str.size() >= 6 && value_str.compare(0, 6, "res://") == 0) {
                    if (godot::ResourceLoader::get_singleton()->exists(godot_str)) {
                        godot::Ref<godot::Resource> res = godot::ResourceLoader::get_singleton()->load(godot_str);
                        if (res.is_valid()) return godot::Variant(res);
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    // 6. Fallback: return as Godot String
    return godot::Variant(godot_str);
}

#endif
