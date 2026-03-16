#include "variant_parser.h"

nlohmann::json parse_variant_hint(const std::string& value_str, const std::string& type_hint) {
    // Stub: always returns string fallback -- tests should FAIL
    return {{"type", "string"}, {"value", value_str}};
}

#ifdef GODOT_MCP_MEOW_GODOT_ENABLED

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

godot::Variant parse_variant(const std::string& value_str, godot::Node* node, const std::string& property) {
    // TODO: implement in GREEN phase (Godot-dependent, not unit-testable)
    return godot::Variant();
}

#endif
