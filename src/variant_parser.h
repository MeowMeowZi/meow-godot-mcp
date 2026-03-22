#ifndef MEOW_GODOT_MCP_VARIANT_PARSER_H
#define MEOW_GODOT_MCP_VARIANT_PARSER_H

#include <nlohmann/json.hpp>
#include <string>

// Pure C++ parsing (no Godot dependency) -- for unit testing and pre-validation
// Returns a JSON representation of the parsed value with type info
// Used by unit tests and argument validation
//
// Return format examples:
//   {"type": "nil"}
//   {"type": "resource_path", "raw": "res://icon.svg"}
//   {"type": "resource_new", "raw": "new:RectangleShape2D(size=Vector2(100,50))"}
//   {"type": "godot_constructor", "raw": "Vector2(100,200)"}
//   {"type": "color_hex", "raw": "#ff0000"}
//   {"type": "bool", "value": true}
//   {"type": "int", "value": 42}
//   {"type": "float", "value": 3.14}
//   {"type": "string", "value": "hello"}
nlohmann::json parse_variant_hint(const std::string& value_str, const std::string& type_hint = "");

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
// Godot-dependent parsing -- used at runtime by scene_mutation.cpp
// Two-layer strategy:
//   1. Try UtilityFunctions::str_to_var() (handles Vector2, Color, etc.)
//   2. Try Color::html() for hex colors
//   3. Type-aware parsing based on current property type
//   4. Fallback: return as Godot String
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/node.hpp>

godot::Variant parse_variant(const std::string& value_str, godot::Node* node, const std::string& property);
#endif

#endif // MEOW_GODOT_MCP_VARIANT_PARSER_H
