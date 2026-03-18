#include "ui_tools.h"
#include "variant_parser.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/container.hpp>
#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/core/object.hpp>

#include <unordered_map>

using namespace godot;

// --- Helper: Node lookup + Control validation ---

static Node* get_scene_root_or_null() {
    return EditorInterface::get_singleton()->get_edited_scene_root();
}

static Node* find_node_or_null(Node* scene_root, const std::string& node_path) {
    return scene_root->get_node_or_null(NodePath(node_path.c_str()));
}

static Control* cast_to_control(Node* node) {
    return Object::cast_to<Control>(node);
}

// Returns {success, error_json, ctrl} - if success is false, error_json has the error response
struct ControlLookupResult {
    bool success;
    nlohmann::json error;
    Control* ctrl;
};

static ControlLookupResult lookup_control(const std::string& node_path) {
    Node* scene_root = get_scene_root_or_null();
    if (!scene_root) {
        return {false, {{"error", "No scene open"}}, nullptr};
    }
    Node* node = find_node_or_null(scene_root, node_path);
    if (!node) {
        return {false, {{"error", "Node not found: " + node_path}}, nullptr};
    }
    Control* ctrl = cast_to_control(node);
    if (!ctrl) {
        return {false, {{"error", "Node is not a Control: " + node_path}}, nullptr};
    }
    return {true, {}, ctrl};
}

// --- Helper: Color parsing ---

struct ColorParseResult {
    bool success;
    Color color;
};

static ColorParseResult parse_color(const std::string& color_str) {
    // Try hex color: #rrggbb or #rrggbbaa
    if (!color_str.empty() && color_str[0] == '#') {
        String godot_str(color_str.c_str());
        if (Color::html_is_valid(godot_str)) {
            return {true, Color::html(godot_str)};
        }
        return {false, Color()};
    }

    // Try Color() constructor via parse_variant
    if (color_str.find("Color(") == 0) {
        // Use a dummy node/property -- parse_variant handles Color() constructors via str_to_var
        Variant parsed = parse_variant(color_str, nullptr, "");
        if (parsed.get_type() == Variant::COLOR) {
            return {true, (Color)parsed};
        }
        return {false, Color()};
    }

    return {false, Color()};
}

// --- Helper: Detect theme override type from key name ---

enum class ThemeOverrideType {
    COLOR,
    FONT_SIZE,
    CONSTANT,
    UNKNOWN
};

static ThemeOverrideType detect_override_type(const std::string& key) {
    // Known color keys
    if (key == "font_color" || key == "icon_color" ||
        key == "font_shadow_color" || key == "font_outline_color" ||
        key == "font_pressed_color" || key == "font_hover_color" ||
        key == "font_focus_color" || key == "font_hover_pressed_color" ||
        key == "font_disabled_color" ||
        key == "icon_normal_color" || key == "icon_hover_color" ||
        key == "icon_pressed_color" || key == "icon_disabled_color" ||
        key == "icon_focus_color" ||
        key == "font_unselected_color" || key == "font_selected_color" ||
        key == "font_hovered_color" ||
        key == "selection_color" || key == "caret_color" ||
        key == "clear_button_color") {
        return ThemeOverrideType::COLOR;
    }
    // Suffix-based color detection
    if (key.size() > 6 && key.substr(key.size() - 6) == "_color") {
        return ThemeOverrideType::COLOR;
    }

    // Known font_size keys
    if (key == "font_size") {
        return ThemeOverrideType::FONT_SIZE;
    }
    // Suffix-based font_size detection (but not _color)
    if (key.size() > 5 && key.substr(key.size() - 5) == "_size") {
        return ThemeOverrideType::FONT_SIZE;
    }

    // Known constant keys
    if (key == "separation" || key == "h_separation" || key == "v_separation" ||
        key == "outline_size" || key == "icon_max_width" || key == "line_spacing" ||
        key == "margin_left" || key == "margin_top" ||
        key == "margin_right" || key == "margin_bottom") {
        return ThemeOverrideType::CONSTANT;
    }

    return ThemeOverrideType::UNKNOWN;
}

// ============================================================================
// 1. set_layout_preset (UISYS-01)
// ============================================================================

static const std::unordered_map<std::string, Control::LayoutPreset> preset_map = {
    {"top_left", Control::PRESET_TOP_LEFT},
    {"top_right", Control::PRESET_TOP_RIGHT},
    {"bottom_left", Control::PRESET_BOTTOM_LEFT},
    {"bottom_right", Control::PRESET_BOTTOM_RIGHT},
    {"center_left", Control::PRESET_CENTER_LEFT},
    {"center_top", Control::PRESET_CENTER_TOP},
    {"center_right", Control::PRESET_CENTER_RIGHT},
    {"center_bottom", Control::PRESET_CENTER_BOTTOM},
    {"center", Control::PRESET_CENTER},
    {"left_wide", Control::PRESET_LEFT_WIDE},
    {"top_wide", Control::PRESET_TOP_WIDE},
    {"right_wide", Control::PRESET_RIGHT_WIDE},
    {"bottom_wide", Control::PRESET_BOTTOM_WIDE},
    {"vcenter_wide", Control::PRESET_VCENTER_WIDE},
    {"hcenter_wide", Control::PRESET_HCENTER_WIDE},
    {"full_rect", Control::PRESET_FULL_RECT},
};

nlohmann::json set_layout_preset(const std::string& node_path,
                                  const std::string& preset,
                                  EditorUndoRedoManager* undo_redo) {
    auto lookup = lookup_control(node_path);
    if (!lookup.success) return lookup.error;
    Control* ctrl = lookup.ctrl;

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Lookup preset
    auto it = preset_map.find(preset);
    if (it == preset_map.end()) {
        std::string valid_presets;
        for (const auto& p : preset_map) {
            if (!valid_presets.empty()) valid_presets += ", ";
            valid_presets += p.first;
        }
        return {{"error", "Unknown preset: " + preset + ". Valid presets: " + valid_presets}};
    }
    Control::LayoutPreset preset_enum = it->second;

    // Capture old anchor/offset values for undo (8 values)
    float old_anchor_left = ctrl->get_anchor(SIDE_LEFT);
    float old_anchor_top = ctrl->get_anchor(SIDE_TOP);
    float old_anchor_right = ctrl->get_anchor(SIDE_RIGHT);
    float old_anchor_bottom = ctrl->get_anchor(SIDE_BOTTOM);
    float old_offset_left = ctrl->get_offset(SIDE_LEFT);
    float old_offset_top = ctrl->get_offset(SIDE_TOP);
    float old_offset_right = ctrl->get_offset(SIDE_RIGHT);
    float old_offset_bottom = ctrl->get_offset(SIDE_BOTTOM);

    // Build UndoRedo action
    undo_redo->create_action(String("MCP: Set layout preset"));

    // Do: apply the preset
    undo_redo->add_do_method(ctrl, "set_anchors_and_offsets_preset",
                              (int)preset_enum, (int)Control::PRESET_MODE_MINSIZE, 0);

    // Undo: restore all 8 old values
    undo_redo->add_undo_property(ctrl, StringName("anchor_left"), old_anchor_left);
    undo_redo->add_undo_property(ctrl, StringName("anchor_top"), old_anchor_top);
    undo_redo->add_undo_property(ctrl, StringName("anchor_right"), old_anchor_right);
    undo_redo->add_undo_property(ctrl, StringName("anchor_bottom"), old_anchor_bottom);
    undo_redo->add_undo_property(ctrl, StringName("offset_left"), old_offset_left);
    undo_redo->add_undo_property(ctrl, StringName("offset_top"), old_offset_top);
    undo_redo->add_undo_property(ctrl, StringName("offset_right"), old_offset_right);
    undo_redo->add_undo_property(ctrl, StringName("offset_bottom"), old_offset_bottom);

    undo_redo->commit_action();

    return {{"success", true}, {"preset", preset}, {"node_path", node_path}};
}

// ============================================================================
// 2. set_theme_override (UISYS-02)
// ============================================================================

nlohmann::json set_theme_override(const std::string& node_path,
                                   const nlohmann::json& overrides,
                                   EditorUndoRedoManager* undo_redo) {
    auto lookup = lookup_control(node_path);
    if (!lookup.success) return lookup.error;
    Control* ctrl = lookup.ctrl;

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    if (!overrides.is_object() || overrides.empty()) {
        return {{"error", "overrides must be a non-empty JSON object"}};
    }

    undo_redo->create_action(String("MCP: Set theme overrides"));
    ctrl->begin_bulk_theme_override();

    int applied_count = 0;
    nlohmann::json applied = nlohmann::json::object();

    for (auto& [key, val] : overrides.items()) {
        std::string val_str;
        if (val.is_string()) {
            val_str = val.get<std::string>();
        } else if (val.is_number_integer()) {
            val_str = std::to_string(val.get<int>());
        } else if (val.is_number_float()) {
            val_str = std::to_string(val.get<double>());
        } else {
            val_str = val.dump();
        }

        StringName sn_key(key.c_str());
        ThemeOverrideType detected_type = detect_override_type(key);

        // Fallback: try to detect from value format
        if (detected_type == ThemeOverrideType::UNKNOWN) {
            auto color_result = parse_color(val_str);
            if (color_result.success) {
                detected_type = ThemeOverrideType::COLOR;
            } else {
                // Try parse as integer -- treat as constant
                {
                    char* end = nullptr;
                    (void)std::strtol(val_str.c_str(), &end, 10);
                    if (end != val_str.c_str() && *end == '\0') {
                        detected_type = ThemeOverrideType::CONSTANT;
                    } else {
                        // Skip unknown override types
                        continue;
                    }
                }
            }
        }

        switch (detected_type) {
            case ThemeOverrideType::COLOR: {
                auto color_result = parse_color(val_str);
                if (!color_result.success) {
                    continue; // Skip invalid colors
                }
                Color color = color_result.color;

                // Capture old value for undo
                if (ctrl->has_theme_color_override(sn_key)) {
                    Color old_color = ctrl->get_theme_color(sn_key);
                    undo_redo->add_undo_method(ctrl, "add_theme_color_override", sn_key, old_color);
                } else {
                    undo_redo->add_undo_method(ctrl, "remove_theme_color_override", sn_key);
                }

                undo_redo->add_do_method(ctrl, "add_theme_color_override", sn_key, color);
                ctrl->add_theme_color_override(sn_key, color);
                applied[key] = "color";
                applied_count++;
                break;
            }
            case ThemeOverrideType::FONT_SIZE: {
                int size = 0;
                if (val.is_number_integer()) {
                    size = val.get<int>();
                } else {
                    char* end_fs = nullptr;
                    size = static_cast<int>(std::strtol(val_str.c_str(), &end_fs, 10));
                    if (end_fs == val_str.c_str() || *end_fs != '\0') {
                        continue; // Skip invalid font sizes
                    }
                }

                // Capture old value for undo
                if (ctrl->has_theme_font_size_override(sn_key)) {
                    int old_size = ctrl->get_theme_font_size(sn_key);
                    undo_redo->add_undo_method(ctrl, "add_theme_font_size_override", sn_key, old_size);
                } else {
                    undo_redo->add_undo_method(ctrl, "remove_theme_font_size_override", sn_key);
                }

                undo_redo->add_do_method(ctrl, "add_theme_font_size_override", sn_key, size);
                ctrl->add_theme_font_size_override(sn_key, size);
                applied[key] = "font_size";
                applied_count++;
                break;
            }
            case ThemeOverrideType::CONSTANT: {
                int constant = 0;
                if (val.is_number_integer()) {
                    constant = val.get<int>();
                } else {
                    char* end_c = nullptr;
                    constant = static_cast<int>(std::strtol(val_str.c_str(), &end_c, 10));
                    if (end_c == val_str.c_str() || *end_c != '\0') {
                        continue; // Skip invalid constants
                    }
                }

                // Capture old value for undo
                if (ctrl->has_theme_constant_override(sn_key)) {
                    int old_constant = ctrl->get_theme_constant(sn_key);
                    undo_redo->add_undo_method(ctrl, "add_theme_constant_override", sn_key, old_constant);
                } else {
                    undo_redo->add_undo_method(ctrl, "remove_theme_constant_override", sn_key);
                }

                undo_redo->add_do_method(ctrl, "add_theme_constant_override", sn_key, constant);
                ctrl->add_theme_constant_override(sn_key, constant);
                applied[key] = "constant";
                applied_count++;
                break;
            }
            default:
                break;
        }
    }

    ctrl->end_bulk_theme_override();
    undo_redo->commit_action();

    return {{"success", true}, {"node_path", node_path},
            {"applied_count", applied_count}, {"applied", applied}};
}

// ============================================================================
// 3. create_stylebox (UISYS-03)
// ============================================================================

nlohmann::json create_stylebox(const std::string& node_path,
                                const std::string& override_name,
                                const nlohmann::json& properties,
                                EditorUndoRedoManager* undo_redo) {
    auto lookup = lookup_control(node_path);
    if (!lookup.success) return lookup.error;
    Control* ctrl = lookup.ctrl;

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Create StyleBoxFlat
    Ref<StyleBoxFlat> sb;
    sb.instantiate();

    // Apply properties
    if (properties.contains("bg_color") && properties["bg_color"].is_string()) {
        auto result = parse_color(properties["bg_color"].get<std::string>());
        if (result.success) sb->set_bg_color(result.color);
    }

    if (properties.contains("corner_radius") && properties["corner_radius"].is_number_integer()) {
        sb->set_corner_radius_all(properties["corner_radius"].get<int>());
    }
    if (properties.contains("corner_radius_top_left") && properties["corner_radius_top_left"].is_number_integer()) {
        sb->set_corner_radius(CORNER_TOP_LEFT, properties["corner_radius_top_left"].get<int>());
    }
    if (properties.contains("corner_radius_top_right") && properties["corner_radius_top_right"].is_number_integer()) {
        sb->set_corner_radius(CORNER_TOP_RIGHT, properties["corner_radius_top_right"].get<int>());
    }
    if (properties.contains("corner_radius_bottom_right") && properties["corner_radius_bottom_right"].is_number_integer()) {
        sb->set_corner_radius(CORNER_BOTTOM_RIGHT, properties["corner_radius_bottom_right"].get<int>());
    }
    if (properties.contains("corner_radius_bottom_left") && properties["corner_radius_bottom_left"].is_number_integer()) {
        sb->set_corner_radius(CORNER_BOTTOM_LEFT, properties["corner_radius_bottom_left"].get<int>());
    }

    if (properties.contains("border_width") && properties["border_width"].is_number_integer()) {
        sb->set_border_width_all(properties["border_width"].get<int>());
    }
    if (properties.contains("border_color") && properties["border_color"].is_string()) {
        auto result = parse_color(properties["border_color"].get<std::string>());
        if (result.success) sb->set_border_color(result.color);
    }

    if (properties.contains("content_margin") && properties["content_margin"].is_number()) {
        double margin = properties["content_margin"].get<double>();
        sb->set_content_margin_all(margin);
    }

    if (properties.contains("shadow_color") && properties["shadow_color"].is_string()) {
        auto result = parse_color(properties["shadow_color"].get<std::string>());
        if (result.success) sb->set_shadow_color(result.color);
    }
    if (properties.contains("shadow_size") && properties["shadow_size"].is_number_integer()) {
        sb->set_shadow_size(properties["shadow_size"].get<int>());
    }

    if (properties.contains("anti_aliased") && properties["anti_aliased"].is_boolean()) {
        sb->set_anti_aliased(properties["anti_aliased"].get<bool>());
    }

    // UndoRedo: capture old stylebox override
    StringName sn_override(override_name.c_str());

    undo_redo->create_action(String("MCP: Create stylebox"));
    undo_redo->add_do_method(ctrl, "add_theme_stylebox_override", sn_override, sb);

    if (ctrl->has_theme_stylebox_override(sn_override)) {
        Ref<StyleBox> old_sb = ctrl->get_theme_stylebox(sn_override);
        undo_redo->add_undo_method(ctrl, "add_theme_stylebox_override", sn_override, old_sb);
    } else {
        undo_redo->add_undo_method(ctrl, "remove_theme_stylebox_override", sn_override);
    }

    undo_redo->commit_action();

    return {{"success", true}, {"node_path", node_path}, {"override_name", override_name}};
}

// ============================================================================
// 4. get_ui_properties (UISYS-04)
// ============================================================================

nlohmann::json get_ui_properties(const std::string& node_path) {
    auto lookup = lookup_control(node_path);
    if (!lookup.success) return lookup.error;
    Control* ctrl = lookup.ctrl;

    nlohmann::json result;
    result["success"] = true;
    result["node_path"] = node_path;

    // Anchors
    result["anchors"] = {
        {"left", ctrl->get_anchor(SIDE_LEFT)},
        {"top", ctrl->get_anchor(SIDE_TOP)},
        {"right", ctrl->get_anchor(SIDE_RIGHT)},
        {"bottom", ctrl->get_anchor(SIDE_BOTTOM)}
    };

    // Offsets
    result["offsets"] = {
        {"left", ctrl->get_offset(SIDE_LEFT)},
        {"top", ctrl->get_offset(SIDE_TOP)},
        {"right", ctrl->get_offset(SIDE_RIGHT)},
        {"bottom", ctrl->get_offset(SIDE_BOTTOM)}
    };

    // Size flags
    result["size_flags"] = {
        {"horizontal", (int)ctrl->get_h_size_flags()},
        {"vertical", (int)ctrl->get_v_size_flags()}
    };

    // Minimum size
    Vector2 min_size = ctrl->get_custom_minimum_size();
    result["minimum_size"] = {
        {"x", min_size.x},
        {"y", min_size.y}
    };

    // Pivot offset
    Vector2 pivot = ctrl->get_pivot_offset();
    result["pivot_offset"] = {
        {"x", pivot.x},
        {"y", pivot.y}
    };

    // Grow direction
    result["grow_direction"] = {
        {"horizontal", (int)ctrl->get_h_grow_direction()},
        {"vertical", (int)ctrl->get_v_grow_direction()}
    };

    // Focus neighbors
    result["focus_neighbors"] = {
        {"left", std::string(String(ctrl->get_focus_neighbor(SIDE_LEFT)).utf8().get_data())},
        {"top", std::string(String(ctrl->get_focus_neighbor(SIDE_TOP)).utf8().get_data())},
        {"right", std::string(String(ctrl->get_focus_neighbor(SIDE_RIGHT)).utf8().get_data())},
        {"bottom", std::string(String(ctrl->get_focus_neighbor(SIDE_BOTTOM)).utf8().get_data())}
    };

    // Layout direction
    result["layout_direction"] = (int)ctrl->get_layout_direction();

    return result;
}

// ============================================================================
// 5. set_container_layout (UISYS-05)
// ============================================================================

static const std::unordered_map<std::string, int> size_flags_map = {
    {"shrink_begin", (int)Control::SIZE_SHRINK_BEGIN},
    {"fill", (int)Control::SIZE_FILL},
    {"expand", (int)Control::SIZE_EXPAND},
    {"expand_fill", (int)Control::SIZE_EXPAND_FILL},
    {"shrink_center", (int)Control::SIZE_SHRINK_CENTER},
    {"shrink_end", (int)Control::SIZE_SHRINK_END},
};

static const std::unordered_map<std::string, BoxContainer::AlignmentMode> alignment_map = {
    {"begin", BoxContainer::ALIGNMENT_BEGIN},
    {"center", BoxContainer::ALIGNMENT_CENTER},
    {"end", BoxContainer::ALIGNMENT_END},
};

nlohmann::json set_container_layout(const std::string& node_path,
                                     const nlohmann::json& params,
                                     EditorUndoRedoManager* undo_redo) {
    Node* scene_root = get_scene_root_or_null();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }
    Node* node = find_node_or_null(scene_root, node_path);
    if (!node) {
        return {{"error", "Node not found: " + node_path}};
    }
    Container* container = Object::cast_to<Container>(node);
    if (!container) {
        return {{"error", "Node is not a Container: " + node_path}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Also cast to Control for theme constant overrides
    Control* ctrl = Object::cast_to<Control>(node);

    undo_redo->create_action(String("MCP: Set container layout"));

    nlohmann::json changes = nlohmann::json::object();

    // Check if BoxContainer (includes VBoxContainer, HBoxContainer)
    BoxContainer* box = Object::cast_to<BoxContainer>(node);
    if (box) {
        // Alignment
        if (params.contains("alignment") && params["alignment"].is_string()) {
            std::string align_str = params["alignment"].get<std::string>();
            auto it = alignment_map.find(align_str);
            if (it != alignment_map.end()) {
                int old_alignment = (int)box->get_alignment();
                undo_redo->add_do_property(box, StringName("alignment"), (int)it->second);
                undo_redo->add_undo_property(box, StringName("alignment"), old_alignment);
                changes["alignment"] = align_str;
            } else {
                undo_redo->commit_action();
                return {{"error", "Unknown alignment: " + align_str + ". Valid: begin, center, end"}};
            }
        }

        // Separation (via theme constant override)
        if (params.contains("separation") && params["separation"].is_number_integer()) {
            int sep = params["separation"].get<int>();
            StringName sep_key("separation");
            if (ctrl->has_theme_constant_override(sep_key)) {
                int old_sep = ctrl->get_theme_constant(sep_key);
                undo_redo->add_undo_method(ctrl, "add_theme_constant_override", sep_key, old_sep);
            } else {
                undo_redo->add_undo_method(ctrl, "remove_theme_constant_override", sep_key);
            }
            undo_redo->add_do_method(ctrl, "add_theme_constant_override", sep_key, sep);
            changes["separation"] = sep;
        }
    }

    // Check if GridContainer
    GridContainer* grid = Object::cast_to<GridContainer>(node);
    if (grid) {
        // Columns
        if (params.contains("columns") && params["columns"].is_number_integer()) {
            int cols = params["columns"].get<int>();
            int old_cols = grid->get_columns();
            undo_redo->add_do_property(grid, StringName("columns"), cols);
            undo_redo->add_undo_property(grid, StringName("columns"), old_cols);
            changes["columns"] = cols;
        }

        // h_separation
        if (params.contains("h_separation") && params["h_separation"].is_number_integer()) {
            int sep = params["h_separation"].get<int>();
            StringName sep_key("h_separation");
            if (ctrl->has_theme_constant_override(sep_key)) {
                int old_sep = ctrl->get_theme_constant(sep_key);
                undo_redo->add_undo_method(ctrl, "add_theme_constant_override", sep_key, old_sep);
            } else {
                undo_redo->add_undo_method(ctrl, "remove_theme_constant_override", sep_key);
            }
            undo_redo->add_do_method(ctrl, "add_theme_constant_override", sep_key, sep);
            changes["h_separation"] = sep;
        }

        // v_separation
        if (params.contains("v_separation") && params["v_separation"].is_number_integer()) {
            int sep = params["v_separation"].get<int>();
            StringName sep_key("v_separation");
            if (ctrl->has_theme_constant_override(sep_key)) {
                int old_sep = ctrl->get_theme_constant(sep_key);
                undo_redo->add_undo_method(ctrl, "add_theme_constant_override", sep_key, old_sep);
            } else {
                undo_redo->add_undo_method(ctrl, "remove_theme_constant_override", sep_key);
            }
            undo_redo->add_do_method(ctrl, "add_theme_constant_override", sep_key, sep);
            changes["v_separation"] = sep;
        }
    }

    // Handle child_size_flags_horizontal
    if (params.contains("child_size_flags_horizontal") && params["child_size_flags_horizontal"].is_string()) {
        std::string flag_str = params["child_size_flags_horizontal"].get<std::string>();
        auto it = size_flags_map.find(flag_str);
        if (it != size_flags_map.end()) {
            int flag_val = it->second;
            for (int i = 0; i < container->get_child_count(); i++) {
                Control* child = Object::cast_to<Control>(container->get_child(i));
                if (child) {
                    int old_flags = (int)child->get_h_size_flags();
                    undo_redo->add_do_property(child, StringName("size_flags_horizontal"), flag_val);
                    undo_redo->add_undo_property(child, StringName("size_flags_horizontal"), old_flags);
                }
            }
            changes["child_size_flags_horizontal"] = flag_str;
        } else {
            undo_redo->commit_action();
            return {{"error", "Unknown size flag: " + flag_str + ". Valid: shrink_begin, fill, expand, expand_fill, shrink_center, shrink_end"}};
        }
    }

    // Handle child_size_flags_vertical
    if (params.contains("child_size_flags_vertical") && params["child_size_flags_vertical"].is_string()) {
        std::string flag_str = params["child_size_flags_vertical"].get<std::string>();
        auto it = size_flags_map.find(flag_str);
        if (it != size_flags_map.end()) {
            int flag_val = it->second;
            for (int i = 0; i < container->get_child_count(); i++) {
                Control* child = Object::cast_to<Control>(container->get_child(i));
                if (child) {
                    int old_flags = (int)child->get_v_size_flags();
                    undo_redo->add_do_property(child, StringName("size_flags_vertical"), flag_val);
                    undo_redo->add_undo_property(child, StringName("size_flags_vertical"), old_flags);
                }
            }
            changes["child_size_flags_vertical"] = flag_str;
        } else {
            undo_redo->commit_action();
            return {{"error", "Unknown size flag: " + flag_str + ". Valid: shrink_begin, fill, expand, expand_fill, shrink_center, shrink_end"}};
        }
    }

    undo_redo->commit_action();

    // Trigger layout recalculation
    container->queue_sort();

    return {{"success", true}, {"node_path", node_path}, {"changes", changes}};
}

// ============================================================================
// 6. get_theme_overrides (UISYS-06 support)
// ============================================================================

nlohmann::json get_theme_overrides(const std::string& node_path) {
    auto lookup = lookup_control(node_path);
    if (!lookup.success) return lookup.error;
    Control* ctrl = lookup.ctrl;

    nlohmann::json colors = nlohmann::json::object();
    nlohmann::json font_sizes = nlohmann::json::object();
    nlohmann::json constants = nlohmann::json::object();
    nlohmann::json styles = nlohmann::json::object();

    // --- Color overrides ---
    static const char* color_keys[] = {
        "font_color", "font_shadow_color", "font_outline_color",
        "font_pressed_color", "font_hover_color", "font_focus_color",
        "font_hover_pressed_color", "font_disabled_color",
        "icon_normal_color", "icon_hover_color", "icon_pressed_color",
        "icon_disabled_color", "icon_focus_color", "icon_color",
        "font_unselected_color", "font_selected_color", "font_hovered_color",
        "selection_color", "caret_color", "clear_button_color"
    };
    for (const char* key : color_keys) {
        StringName sn(key);
        if (ctrl->has_theme_color_override(sn)) {
            Color c = ctrl->get_theme_color(sn);
            // Convert to hex string #rrggbbaa
            char hex[12];
            snprintf(hex, sizeof(hex), "#%02x%02x%02x%02x",
                     (int)(c.r * 255), (int)(c.g * 255),
                     (int)(c.b * 255), (int)(c.a * 255));
            colors[key] = std::string(hex);
        }
    }

    // --- Font size overrides ---
    static const char* font_size_keys[] = {"font_size"};
    for (const char* key : font_size_keys) {
        StringName sn(key);
        if (ctrl->has_theme_font_size_override(sn)) {
            font_sizes[key] = ctrl->get_theme_font_size(sn);
        }
    }

    // --- Constant overrides ---
    static const char* constant_keys[] = {
        "separation", "h_separation", "v_separation",
        "outline_size", "icon_max_width", "line_spacing"
    };
    for (const char* key : constant_keys) {
        StringName sn(key);
        if (ctrl->has_theme_constant_override(sn)) {
            constants[key] = ctrl->get_theme_constant(sn);
        }
    }

    // --- Style overrides ---
    static const char* style_keys[] = {
        "normal", "hover", "pressed", "disabled", "focus",
        "panel", "tab_selected", "tab_unselected", "tab_hovered",
        "tab_disabled", "grabber", "grabber_highlight", "slider", "scroll"
    };
    for (const char* key : style_keys) {
        StringName sn(key);
        if (ctrl->has_theme_stylebox_override(sn)) {
            Ref<StyleBox> sb = ctrl->get_theme_stylebox(sn);
            if (sb.is_valid()) {
                nlohmann::json style_info;
                style_info["type"] = std::string(String(sb->get_class()).utf8().get_data());

                // If StyleBoxFlat, include detailed properties
                Ref<StyleBoxFlat> sbf = sb;
                if (sbf.is_valid()) {
                    Color bg = sbf->get_bg_color();
                    char bg_hex[12];
                    snprintf(bg_hex, sizeof(bg_hex), "#%02x%02x%02x%02x",
                             (int)(bg.r * 255), (int)(bg.g * 255),
                             (int)(bg.b * 255), (int)(bg.a * 255));
                    style_info["bg_color"] = std::string(bg_hex);
                    style_info["corner_radius_top_left"] = sbf->get_corner_radius(CORNER_TOP_LEFT);
                    style_info["corner_radius_top_right"] = sbf->get_corner_radius(CORNER_TOP_RIGHT);
                    style_info["corner_radius_bottom_right"] = sbf->get_corner_radius(CORNER_BOTTOM_RIGHT);
                    style_info["corner_radius_bottom_left"] = sbf->get_corner_radius(CORNER_BOTTOM_LEFT);
                    style_info["border_width_left"] = sbf->get_border_width(SIDE_LEFT);
                    style_info["border_width_top"] = sbf->get_border_width(SIDE_TOP);
                    style_info["border_width_right"] = sbf->get_border_width(SIDE_RIGHT);
                    style_info["border_width_bottom"] = sbf->get_border_width(SIDE_BOTTOM);

                    Color bc = sbf->get_border_color();
                    char bc_hex[12];
                    snprintf(bc_hex, sizeof(bc_hex), "#%02x%02x%02x%02x",
                             (int)(bc.r * 255), (int)(bc.g * 255),
                             (int)(bc.b * 255), (int)(bc.a * 255));
                    style_info["border_color"] = std::string(bc_hex);
                    style_info["anti_aliased"] = sbf->is_anti_aliased();
                }

                styles[key] = style_info;
            }
        }
    }

    return {
        {"success", true},
        {"node_path", node_path},
        {"colors", colors},
        {"font_sizes", font_sizes},
        {"constants", constants},
        {"styles", styles}
    };
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
