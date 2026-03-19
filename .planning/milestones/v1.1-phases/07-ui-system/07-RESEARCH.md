# Phase 7: UI System - Research

**Researched:** 2026-03-18
**Domain:** Godot 4.x Control/UI system -- layout presets, theme overrides, StyleBoxFlat, Container layout
**Confidence:** HIGH

## Summary

Phase 7 adds 6 MCP tools for AI-driven UI construction in Godot: layout preset application, theme overrides (colors/fonts/font sizes/styleboxes), StyleBoxFlat creation, UI property queries, Container layout configuration, and theme override queries. All Godot APIs needed are well-documented, stable since Godot 4.0, and fully exposed through godot-cpp generated bindings.

The implementation follows the established project pattern: a new `ui_tools.h`/`ui_tools.cpp` module with free functions returning `nlohmann::json`, registered via `ToolDef` entries in `mcp_tool_registry.cpp`, and dispatched in `mcp_server.cpp::handle_request()`. The Godot C++ APIs (`Control::set_anchors_and_offsets_preset()`, `Control::add_theme_*_override()`, `StyleBoxFlat` setters, `Container::queue_sort()`) are all verified present in the godot-cpp v10 headers at `godot-cpp/gen/include/godot_cpp/classes/`.

**Primary recommendation:** Implement all 6 tools in a single `ui_tools.h/.cpp` module following the `scene_file_tools` pattern. Use string-to-enum mapping for layout presets, key-suffix heuristics for theme override type detection, and `Object::cast_to<>` for Container type detection.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Tool Architecture -- 6 Dedicated UI Tools**: set_layout_preset, set_theme_override, create_stylebox, get_ui_properties, set_container_layout, get_theme_overrides
- **Theme Override Design**: Batch model for set_theme_override, auto-detected override types from key names, create_stylebox auto-applies to target node, only StyleBoxFlat supported, get_theme_overrides returns categorized output
- **Layout & Container Design**: set_layout_preset supports ALL 16 LayoutPreset enum values as string names, set_container_layout includes optional child_size_flags batch parameter, get_ui_properties returns comprehensive data (anchors, offsets, size_flags, minimum_size, pivot_offset, grow_direction, focus_neighbors, layout_direction)
- **Code Organization**: New ui_tools.h / ui_tools.cpp file pair, free functions returning nlohmann::json, all mutation tools take EditorUndoRedoManager*

### Claude's Discretion
- Exact preset string names and parsing strategy
- Theme override key detection heuristics
- StyleBoxFlat property names and ranges
- get_ui_properties inclusion of optional vs always-present fields
- Container type detection (VBoxContainer, HBoxContainer, GridContainer, etc.)
- Error messages and validation details

### Deferred Ideas (OUT OF SCOPE)
- Theme resource (.tres) creation/editing
- Global theme management
- Custom theme types
- StyleBoxTexture / StyleBoxLine support
- Theme inheritance visualization

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UISYS-01 | AI can set Control node anchor/margin layout preset | `Control::set_anchors_and_offsets_preset()` with `LayoutPreset` enum (16 values). Verified in godot-cpp control.hpp. Maps to `set_layout_preset` tool. |
| UISYS-02 | AI can set Control theme overrides (colors, fonts, styles) | `Control::add_theme_color_override()`, `add_theme_font_size_override()`, `add_theme_stylebox_override()` etc. All present in godot-cpp. Maps to `set_theme_override` tool. |
| UISYS-03 | AI can create/edit StyleBox resources (StyleBoxFlat) | `StyleBoxFlat` with 23 properties (bg_color, corner_radius_*, border_width_*, etc.). Verified in style_box_flat.hpp. Maps to `create_stylebox` tool. |
| UISYS-04 | AI can query Control UI-specific properties | `Control::get_anchor()`, `get_offset()`, `get_h_size_flags()`, `get_custom_minimum_size()`, `get_focus_neighbor()` etc. All verified in control.hpp. Maps to `get_ui_properties` tool. |
| UISYS-05 | AI can configure Container layout parameters | `BoxContainer::set_alignment()`, `add_theme_constant_override("separation", N)`, `GridContainer::set_columns()`. Theme constant "separation" for Box/Grid. Maps to `set_container_layout` tool. |
| UISYS-06 | AI can set Control focus neighbors for keyboard/gamepad nav | `Control::set_focus_neighbor(Side, NodePath)` for 4 directions. Verified in control.hpp. Covered by `get_ui_properties` (query) + `set_node_property` (existing tool for setting) or dedicated parameter in another tool. |

</phase_requirements>

## Standard Stack

### Core (Already in Project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ | C++ GDExtension bindings | Project foundation -- all Godot API access |
| nlohmann/json | 3.12.0 | JSON serialization | Project standard for all tool I/O |

### Godot API Headers Required
| Header | Purpose |
|--------|---------|
| `godot_cpp/classes/control.hpp` | LayoutPreset enum, theme override methods, anchor/offset/size_flag getters |
| `godot_cpp/classes/style_box_flat.hpp` | StyleBoxFlat property setters (bg_color, corner_radius, border_width, etc.) |
| `godot_cpp/classes/style_box.hpp` | StyleBox base class (content_margin) |
| `godot_cpp/classes/container.hpp` | Container::queue_sort() |
| `godot_cpp/classes/box_container.hpp` | BoxContainer::AlignmentMode enum, set_alignment() |
| `godot_cpp/classes/grid_container.hpp` | GridContainer::set_columns() |
| `godot_cpp/classes/editor_undo_redo_manager.hpp` | UndoRedo support for mutation tools |
| `godot_cpp/core/class_db.hpp` | ClassDB::is_parent_class() for type validation |

### Alternatives Considered
None -- the stack is locked by project architecture. No new dependencies needed.

## Architecture Patterns

### Recommended Project Structure
```
src/
  ui_tools.h           # 6 function declarations (new)
  ui_tools.cpp         # 6 function implementations (new)
  mcp_tool_registry.cpp  # Add 6 new ToolDef entries (modify)
  mcp_server.h         # Add #include "ui_tools.h" (modify)
  mcp_server.cpp       # Add 6 tool dispatch handlers (modify)
```

### Pattern 1: Tool Module (Free Functions + JSON Return)
**What:** Each tool is a free function taking parameters + optional EditorUndoRedoManager*, returning nlohmann::json
**When to use:** All new tools -- this is the established project pattern
**Example (from scene_file_tools.h):**
```cpp
// Source: src/scene_file_tools.h (existing project pattern)
nlohmann::json save_scene(const std::string& path);
nlohmann::json instantiate_scene(const std::string& scene_path,
                                  const std::string& parent_path,
                                  const std::string& name,
                                  godot::EditorUndoRedoManager* undo_redo);
```

### Pattern 2: Node Lookup + Type Validation
**What:** Find node by path, validate it's a Control/Container, then operate
**When to use:** All 6 UI tools need to find the target node and validate its type
**Example:**
```cpp
// Source: project pattern from scene_mutation.cpp
Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
if (!scene_root) return {{"error", "No scene open"}};
Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
if (!node) return {{"error", "Node not found: " + node_path}};
Control* ctrl = Object::cast_to<Control>(node);
if (!ctrl) return {{"error", "Node is not a Control: " + node_path}};
```

### Pattern 3: UndoRedo for Mutations
**What:** Wrap property changes in UndoRedo actions for editor integration
**When to use:** set_layout_preset, set_theme_override, create_stylebox, set_container_layout
**Example:**
```cpp
// Source: src/scene_mutation.cpp (existing UndoRedo pattern)
undo_redo->create_action(String("MCP: Set layout preset"));
// Store old values for undo
Variant old_value = node->get(StringName(property.c_str()));
undo_redo->add_do_property(node, StringName(property.c_str()), new_value);
undo_redo->add_undo_property(node, StringName(property.c_str()), old_value);
undo_redo->commit_action();
```

### Pattern 4: String-to-Enum Mapping
**What:** Map user-friendly string names to Godot enum integer values
**When to use:** Layout preset names, size flag names, alignment mode names
**Example (recommended):**
```cpp
// Recommended pattern for preset name mapping
static const std::unordered_map<std::string, Control::LayoutPreset> preset_map = {
    {"top_left",       Control::PRESET_TOP_LEFT},
    {"top_right",      Control::PRESET_TOP_RIGHT},
    {"bottom_left",    Control::PRESET_BOTTOM_LEFT},
    {"bottom_right",   Control::PRESET_BOTTOM_RIGHT},
    {"center_left",    Control::PRESET_CENTER_LEFT},
    {"center_top",     Control::PRESET_CENTER_TOP},
    {"center_right",   Control::PRESET_CENTER_RIGHT},
    {"center_bottom",  Control::PRESET_CENTER_BOTTOM},
    {"center",         Control::PRESET_CENTER},
    {"left_wide",      Control::PRESET_LEFT_WIDE},
    {"top_wide",       Control::PRESET_TOP_WIDE},
    {"right_wide",     Control::PRESET_RIGHT_WIDE},
    {"bottom_wide",    Control::PRESET_BOTTOM_WIDE},
    {"vcenter_wide",   Control::PRESET_VCENTER_WIDE},
    {"hcenter_wide",   Control::PRESET_HCENTER_WIDE},
    {"full_rect",      Control::PRESET_FULL_RECT},
};
```

### Pattern 5: Bulk Theme Override
**What:** Use `begin_bulk_theme_override()` / `end_bulk_theme_override()` to batch multiple overrides efficiently
**When to use:** set_theme_override when applying multiple overrides in one call
**Example:**
```cpp
// Source: godot-cpp control.hpp (verified API exists)
ctrl->begin_bulk_theme_override();
ctrl->add_theme_color_override(StringName("font_color"), color);
ctrl->add_theme_font_size_override(StringName("font_size"), 16);
ctrl->end_bulk_theme_override();
```

### Anti-Patterns to Avoid
- **Setting anchors individually instead of using preset:** `set_anchors_and_offsets_preset()` handles all 4 anchors + all 4 offsets atomically. Never set anchor_left/right/top/bottom individually when applying a preset.
- **Forgetting queue_sort after Container changes:** After modifying Container properties (separation, alignment, child size_flags), call `Container::queue_sort()` to trigger re-layout.
- **Creating StyleBoxFlat without applying it:** The create_stylebox tool should auto-apply via `add_theme_stylebox_override()`. Don't just create and return -- the user expects it applied.
- **UndoRedo for set_anchors_and_offsets_preset:** This method sets multiple properties internally. Rather than wrapping it in UndoRedo with individual property undo, consider saving/restoring anchor+offset values manually before/after calling the method.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Layout preset application | Manual anchor/offset calculation | `Control::set_anchors_and_offsets_preset()` | Handles all 16 presets with proper resize mode |
| Theme type detection | Manual property name categorization | Key suffix heuristics + `has_theme_*_override()` | Godot convention: `_color` suffix = color, `_size` = font_size, etc. |
| Container type checking | String comparison on class name | `ClassDB::is_parent_class(class_name, "Container")` + `Object::cast_to<BoxContainer>()` | Type-safe, handles inheritance |
| Bulk theme override batching | Individual override calls with no batching | `begin_bulk_theme_override()` / `end_bulk_theme_override()` | Built-in Godot optimization, avoids redundant theme recalculations |

**Key insight:** The Godot API already provides all high-level operations needed. The MCP tools are thin wrappers that translate JSON parameters to Godot API calls.

## Common Pitfalls

### Pitfall 1: UndoRedo for set_anchors_and_offsets_preset
**What goes wrong:** `set_anchors_and_offsets_preset()` internally modifies 8 properties (4 anchors + 4 offsets). A simple UndoRedo action wrapping just the method call won't properly undo.
**Why it happens:** The method doesn't go through UndoRedo-aware property setters.
**How to avoid:** Before calling the method, capture all 8 values (`get_anchor(SIDE_LEFT/RIGHT/TOP/BOTTOM)` and `get_offset(SIDE_LEFT/RIGHT/TOP/BOTTOM)`). In UndoRedo, use `add_do_method()` for the preset call, and `add_undo_property()` for each of the 8 saved values.
**Warning signs:** Ctrl+Z after set_layout_preset doesn't fully revert the layout.

### Pitfall 2: Theme Override Key Type Ambiguity
**What goes wrong:** Some theme override keys don't follow naming conventions. E.g., "font_color" is a Color, "normal" is a StyleBox, "separation" is a constant.
**Why it happens:** Godot theme keys are convention-based, not strictly typed by name suffix.
**How to avoid:** Use a multi-strategy approach: (1) check known key lists per type, (2) fall back to suffix heuristics (`*_color` = color, `*_size` = font_size), (3) attempt value parsing to disambiguate. For set_theme_override, accept an explicit `type` parameter as optional override.
**Warning signs:** Theme override silently applied to wrong category (e.g., setting an int as a color).

### Pitfall 3: StyleBoxFlat Corner/Side Enumeration
**What goes wrong:** Corner and Side enums are different in Godot. Corners use `CORNER_TOP_LEFT=0, CORNER_TOP_RIGHT=1, CORNER_BOTTOM_RIGHT=2, CORNER_BOTTOM_LEFT=3`. Sides use `SIDE_LEFT=0, SIDE_TOP=1, SIDE_RIGHT=2, SIDE_BOTTOM=3`.
**Why it happens:** The enum order is not intuitive -- CORNER goes clockwise from top-left, SIDE goes left-top-right-bottom.
**How to avoid:** Use the godot-cpp `Corner` and `Side` enums directly. Expose JSON keys as explicit names (`corner_radius_top_left`, `border_width_left`, etc.) rather than indices.
**Warning signs:** Border widths or corner radii applied to wrong edges.

### Pitfall 4: Container queue_sort Timing
**What goes wrong:** After programmatically changing Container properties or child size_flags, the layout doesn't update immediately.
**Why it happens:** Container layout is deferred -- it recalculates during the next frame's notification cycle.
**How to avoid:** Call `Container::queue_sort()` after modifications. The MCP response will be sent before the visual update occurs, but the properties will be correct. The layout update happens on the next frame.
**Warning signs:** Test queries immediately after setting show correct property values but visual layout appears wrong until next frame.

### Pitfall 5: Font Override Requires Font Resource
**What goes wrong:** Setting a font theme override requires a `Ref<Font>` object, not a string path. Cannot use simple string value like color overrides.
**Why it happens:** Fonts are resources, not primitive values.
**How to avoid:** For set_theme_override, font overrides should accept a `res://` path and load it with `ResourceLoader`. This is more complex than color/font_size overrides. Alternatively, defer font overrides to a future enhancement and document the limitation.
**Warning signs:** Crash or silent failure when trying to set font override with a string.

### Pitfall 6: UISYS-06 Focus Neighbor Already Achievable
**What goes wrong:** UISYS-06 (set focus neighbors) may appear to need a dedicated tool, but `set_node_property` already works for setting `focus_neighbor_left` etc.
**Why it happens:** Focus neighbor properties are regular Node properties that accept NodePath strings.
**How to avoid:** Verify that `set_node_property` with property names like "focus_neighbor_left" and NodePath values works correctly. If it does, UISYS-06 is satisfied by the combination of `get_ui_properties` (query) + existing `set_node_property` (mutation). Document this in the tool description for get_ui_properties.
**Warning signs:** Creating a redundant tool when existing infrastructure suffices.

## Code Examples

Verified patterns from godot-cpp headers:

### Setting Layout Preset (UISYS-01)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/control.hpp
Control* ctrl = Object::cast_to<Control>(node);
ctrl->set_anchors_and_offsets_preset(
    Control::PRESET_FULL_RECT,
    Control::PRESET_MODE_MINSIZE,  // resize_mode (default)
    0                               // margin (default)
);
```

### Setting Theme Color Override (UISYS-02)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/control.hpp
Control* ctrl = Object::cast_to<Control>(node);
ctrl->begin_bulk_theme_override();
ctrl->add_theme_color_override(StringName("font_color"), Color(1, 0, 0, 1));
ctrl->add_theme_font_size_override(StringName("font_size"), 24);
ctrl->end_bulk_theme_override();
```

### Creating StyleBoxFlat (UISYS-03)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/style_box_flat.hpp
Ref<StyleBoxFlat> sb;
sb.instantiate();
sb->set_bg_color(Color(0.2, 0.3, 0.8, 1.0));
sb->set_corner_radius_all(8);
sb->set_border_width_all(2);
sb->set_border_color(Color(1, 1, 1, 1));
sb->set_content_margin_all(10.0f);  // Inherited from StyleBox
sb->set_anti_aliased(true);

// Apply to node
ctrl->add_theme_stylebox_override(StringName("panel"), sb);
```

### Querying UI Properties (UISYS-04)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/control.hpp
Control* ctrl = Object::cast_to<Control>(node);

// Anchors (4 values using Side enum)
float anchor_left   = ctrl->get_anchor(SIDE_LEFT);
float anchor_top    = ctrl->get_anchor(SIDE_TOP);
float anchor_right  = ctrl->get_anchor(SIDE_RIGHT);
float anchor_bottom = ctrl->get_anchor(SIDE_BOTTOM);

// Offsets (4 values)
float offset_left   = ctrl->get_offset(SIDE_LEFT);
float offset_top    = ctrl->get_offset(SIDE_TOP);
float offset_right  = ctrl->get_offset(SIDE_RIGHT);
float offset_bottom = ctrl->get_offset(SIDE_BOTTOM);

// Size flags
BitField<Control::SizeFlags> h_flags = ctrl->get_h_size_flags();
BitField<Control::SizeFlags> v_flags = ctrl->get_v_size_flags();

// Custom minimum size
Vector2 min_size = ctrl->get_custom_minimum_size();

// Focus neighbors (4 directions)
NodePath focus_left   = ctrl->get_focus_neighbor(SIDE_LEFT);
NodePath focus_top    = ctrl->get_focus_neighbor(SIDE_TOP);
NodePath focus_right  = ctrl->get_focus_neighbor(SIDE_RIGHT);
NodePath focus_bottom = ctrl->get_focus_neighbor(SIDE_BOTTOM);

// Grow directions
Control::GrowDirection h_grow = ctrl->get_h_grow_direction();
Control::GrowDirection v_grow = ctrl->get_v_grow_direction();

// Pivot and layout direction
Vector2 pivot = ctrl->get_pivot_offset();
Control::LayoutDirection layout_dir = ctrl->get_layout_direction();
```

### Configuring Container Layout (UISYS-05)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/box_container.hpp + container.hpp
Container* container = Object::cast_to<Container>(node);

// For BoxContainer (VBox/HBox): set alignment and separation
BoxContainer* box = Object::cast_to<BoxContainer>(node);
if (box) {
    box->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    box->add_theme_constant_override(StringName("separation"), 10);
}

// For GridContainer: set columns and separations
GridContainer* grid = Object::cast_to<GridContainer>(node);
if (grid) {
    grid->set_columns(3);
    grid->add_theme_constant_override(StringName("h_separation"), 8);
    grid->add_theme_constant_override(StringName("v_separation"), 8);
}

// Batch set child size_flags
for (int i = 0; i < container->get_child_count(); i++) {
    Control* child = Object::cast_to<Control>(container->get_child(i));
    if (child) {
        child->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    }
}

// Trigger re-layout
container->queue_sort();
```

### Querying Theme Overrides (get_theme_overrides support)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/control.hpp
Control* ctrl = Object::cast_to<Control>(node);

// Check and get color overrides by known key names
if (ctrl->has_theme_color_override(StringName("font_color"))) {
    Color c = ctrl->get_theme_color(StringName("font_color"));
    // add to result JSON
}

// Check stylebox overrides
if (ctrl->has_theme_stylebox_override(StringName("panel"))) {
    Ref<StyleBox> sb = ctrl->get_theme_stylebox(StringName("panel"));
    // Inspect StyleBoxFlat properties if applicable
}
```

## Godot API Reference: Complete Enum Values

### LayoutPreset (16 values)
| Name | Value | Description |
|------|-------|-------------|
| PRESET_TOP_LEFT | 0 | Snap to top-left corner |
| PRESET_TOP_RIGHT | 1 | Snap to top-right corner |
| PRESET_BOTTOM_LEFT | 2 | Snap to bottom-left corner |
| PRESET_BOTTOM_RIGHT | 3 | Snap to bottom-right corner |
| PRESET_CENTER_LEFT | 4 | Center vertically on left edge |
| PRESET_CENTER_TOP | 5 | Center horizontally on top edge |
| PRESET_CENTER_RIGHT | 6 | Center vertically on right edge |
| PRESET_CENTER_BOTTOM | 7 | Center horizontally on bottom edge |
| PRESET_CENTER | 8 | Center in parent |
| PRESET_LEFT_WIDE | 9 | Fill left edge, full height |
| PRESET_TOP_WIDE | 10 | Fill top edge, full width |
| PRESET_RIGHT_WIDE | 11 | Fill right edge, full height |
| PRESET_BOTTOM_WIDE | 12 | Fill bottom edge, full width |
| PRESET_VCENTER_WIDE | 13 | Center vertically, full height |
| PRESET_HCENTER_WIDE | 14 | Center horizontally, full width |
| PRESET_FULL_RECT | 15 | Fill entire parent rect |

### SizeFlags (bitfield)
| Name | Value | Description |
|------|-------|-------------|
| SIZE_SHRINK_BEGIN | 0 | Shrink to beginning (default) |
| SIZE_FILL | 1 | Fill available space |
| SIZE_EXPAND | 2 | Expand to take available space |
| SIZE_EXPAND_FILL | 3 | SIZE_EXPAND | SIZE_FILL |
| SIZE_SHRINK_CENTER | 4 | Shrink and center |
| SIZE_SHRINK_END | 8 | Shrink to end |

### BoxContainer::AlignmentMode
| Name | Value | Description |
|------|-------|-------------|
| ALIGNMENT_BEGIN | 0 | Children at start |
| ALIGNMENT_CENTER | 1 | Children centered |
| ALIGNMENT_END | 2 | Children at end |

### GrowDirection
| Name | Value | Description |
|------|-------|-------------|
| GROW_DIRECTION_BEGIN | 0 | Grow toward beginning |
| GROW_DIRECTION_END | 1 | Grow toward end |
| GROW_DIRECTION_BOTH | 2 | Grow in both directions |

### StyleBoxFlat Properties (23 properties)
| Property | Type | Default | Setter |
|----------|------|---------|--------|
| bg_color | Color | Color(0.6, 0.6, 0.6, 1) | set_bg_color() |
| border_color | Color | Color(0.8, 0.8, 0.8, 1) | set_border_color() |
| border_width_left | int | 0 | set_border_width(SIDE_LEFT, N) |
| border_width_top | int | 0 | set_border_width(SIDE_TOP, N) |
| border_width_right | int | 0 | set_border_width(SIDE_RIGHT, N) |
| border_width_bottom | int | 0 | set_border_width(SIDE_BOTTOM, N) |
| border_blend | bool | false | set_border_blend() |
| corner_radius_top_left | int | 0 | set_corner_radius(CORNER_TOP_LEFT, N) |
| corner_radius_top_right | int | 0 | set_corner_radius(CORNER_TOP_RIGHT, N) |
| corner_radius_bottom_right | int | 0 | set_corner_radius(CORNER_BOTTOM_RIGHT, N) |
| corner_radius_bottom_left | int | 0 | set_corner_radius(CORNER_BOTTOM_LEFT, N) |
| corner_detail | int | 8 | set_corner_detail() |
| expand_margin_left | float | 0.0 | set_expand_margin(SIDE_LEFT, N) |
| expand_margin_top | float | 0.0 | set_expand_margin(SIDE_TOP, N) |
| expand_margin_right | float | 0.0 | set_expand_margin(SIDE_RIGHT, N) |
| expand_margin_bottom | float | 0.0 | set_expand_margin(SIDE_BOTTOM, N) |
| draw_center | bool | true | set_draw_center() |
| shadow_color | Color | Color(0, 0, 0, 0.6) | set_shadow_color() |
| shadow_size | int | 0 | set_shadow_size() |
| shadow_offset | Vector2 | Vector2(0, 0) | set_shadow_offset() |
| anti_aliasing | bool | true | set_anti_aliased() |
| anti_aliasing_size | float | 1.0 | set_aa_size() |
| skew | Vector2 | Vector2(0, 0) | set_skew() |

Note: StyleBox base class adds content_margin_* (4 sides) via `set_content_margin(Side, float)`.

### Container Theme Constants
| Container Type | Constant Name | Default | Description |
|---------------|---------------|---------|-------------|
| BoxContainer (VBox/HBox) | "separation" | 4 | Space between children |
| GridContainer | "h_separation" | 4 | Horizontal space between cells |
| GridContainer | "v_separation" | 4 | Vertical space between cells |

### Theme Override Key Detection Heuristics (Recommended)
| Override Type | Detection Strategy | Examples |
|--------------|-------------------|----------|
| Color | Key ends with `_color` OR is known color key ("font_color", "font_outline_color") | "font_color", "icon_color" |
| Font Size | Key ends with `_size` OR is "font_size" | "font_size" |
| Font | Key equals "font" or ends with `_font` | "font", "bold_font" |
| Constant | Key is "separation", "h_separation", "v_separation", "outline_size", "margin_*" | "separation", "outline_size" |
| StyleBox | Key matches known stylebox names: "normal", "hover", "pressed", "disabled", "focus", "panel" | "normal", "panel" |

**Fallback strategy:** If suffix detection is ambiguous, check if the value parses as a color (hex or Color()), an integer, or a font path. Accept optional explicit `type` parameter on the API.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `set_margin()` | `set_offset()` | Godot 4.0 | Margins renamed to offsets in Control |
| `rect_min_size` | `custom_minimum_size` | Godot 4.0 | Property renamed |
| `rect_position` | `position` | Godot 4.0 | Property renamed |
| `MARGIN_*` enum | `SIDE_*` enum | Godot 4.0 | Enum renamed globally |
| Individual add_*_override | begin/end_bulk_theme_override | Godot 4.1 | Batching API added |

**Deprecated/outdated:**
- `LAYOUT_DIRECTION_LOCALE` is deprecated alias for `LAYOUT_DIRECTION_APPLICATION_LOCALE` (value 1)

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest (unit) + Python TCP UAT (integration) |
| Config file | tests/CMakeLists.txt (unit), tests/uat_phase7.py (UAT) |
| Quick run command | `cd tests/build && ctest --output-on-failure` |
| Full suite command | `cd tests/build && ctest --output-on-failure && python tests/uat_phase7.py` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| UISYS-01 | set_layout_preset applies preset correctly | UAT | `python tests/uat_phase7.py` (test: preset) | No -- Wave 0 |
| UISYS-02 | set_theme_override batch-sets overrides | UAT | `python tests/uat_phase7.py` (test: theme override) | No -- Wave 0 |
| UISYS-03 | create_stylebox creates+applies StyleBoxFlat | UAT | `python tests/uat_phase7.py` (test: stylebox) | No -- Wave 0 |
| UISYS-04 | get_ui_properties returns anchors, size_flags, etc. | UAT | `python tests/uat_phase7.py` (test: ui props) | No -- Wave 0 |
| UISYS-05 | set_container_layout sets separation, alignment | UAT | `python tests/uat_phase7.py` (test: container) | No -- Wave 0 |
| UISYS-06 | Focus neighbors queryable + settable | UAT | `python tests/uat_phase7.py` (test: focus) | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && ctest --output-on-failure`
- **Per wave merge:** Full GoogleTest suite + UAT script
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/uat_phase7.py` -- UAT test script covering all 6 UISYS requirements
- [ ] `tests/test_tool_registry.cpp` update -- verify tool count increases to 29 (23 + 6)
- [ ] Unit tests not practical for UI tools (require live Godot instance) -- UAT is primary validation

## Open Questions

1. **Theme override key discovery for get_theme_overrides**
   - What we know: Godot provides `has_theme_*_override()` to check if a specific named override exists, but there is no `list_theme_*_overrides()` method to enumerate all overrides on a node.
   - What's unclear: Without enumeration, get_theme_overrides would need to check a predefined list of known key names.
   - Recommendation: Maintain a static list of common theme override keys per Control subtype (Label: font_color, font_size, font; Button: normal, hover, pressed, disabled, focus; Panel: panel; etc.). Check each with `has_theme_*_override()`. This covers 95%+ of practical cases. Alternatively, use the Godot `Object::get_property_list()` API to discover overrides (theme overrides appear as properties with specific usage flags).

2. **UISYS-06: Dedicated tool vs existing set_node_property**
   - What we know: Focus neighbors (focus_neighbor_left/top/right/bottom) are regular Control properties that take NodePath values.
   - What's unclear: Whether `set_node_property` already handles NodePath strings correctly.
   - Recommendation: Test `set_node_property` with focus_neighbor_* first. If it works, UISYS-06 is satisfied by get_ui_properties (query) + set_node_property (set). If not, add focus_neighbor setting to set_layout_preset or a separate parameter.

3. **UndoRedo granularity for set_theme_override batch**
   - What we know: set_theme_override applies multiple overrides in one call.
   - What's unclear: Should undo revert all overrides from that call atomically, or individually?
   - Recommendation: Single UndoRedo action for the entire batch -- this matches user expectation of "undo the last tool call."

## Sources

### Primary (HIGH confidence)
- `godot-cpp/gen/include/godot_cpp/classes/control.hpp` -- Complete LayoutPreset enum (16 values), SizeFlags bitfield, all theme override methods, anchor/offset/focus_neighbor getters/setters. **Verified in local repository.**
- `godot-cpp/gen/include/godot_cpp/classes/style_box_flat.hpp` -- All 23 StyleBoxFlat property setters/getters. **Verified in local repository.**
- `godot-cpp/gen/include/godot_cpp/classes/style_box.hpp` -- StyleBox base class with content_margin methods. **Verified in local repository.**
- `godot-cpp/gen/include/godot_cpp/classes/box_container.hpp` -- AlignmentMode enum, set_alignment(). **Verified in local repository.**
- `godot-cpp/gen/include/godot_cpp/classes/grid_container.hpp` -- set_columns(). **Verified in local repository.**
- `godot-cpp/gen/include/godot_cpp/classes/container.hpp` -- queue_sort(). **Verified in local repository.**
- [Godot control.h source](https://github.com/godotengine/godot/blob/master/scene/gui/control.h) -- LayoutPreset, SizeFlags, GrowDirection, LayoutDirection enums

### Secondary (MEDIUM confidence)
- [Godot StyleBoxFlat docs (4.3)](https://docs.godotengine.org/en/4.3/classes/class_styleboxflat.html) -- Property reference and defaults
- [Godot BoxContainer docs](https://raw.githubusercontent.com/godotengine/godot-docs/master/classes/class_boxcontainer.rst) -- alignment property, "separation" theme constant
- [Godot GridContainer docs](https://raw.githubusercontent.com/godotengine/godot-docs/master/classes/class_gridcontainer.rst) -- columns property, h_separation/v_separation constants

### Tertiary (LOW confidence)
- [Godot Forum: Box Container Separation](https://forum.godotengine.org/t/box-container-separation/61607) -- Community confirmation of `add_theme_constant_override("separation", N)` pattern

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all APIs verified in local godot-cpp headers
- Architecture: HIGH -- follows established project patterns from Phase 1-6
- Pitfalls: HIGH -- common issues identified from API inspection and Godot documentation
- Enum values: HIGH -- extracted directly from godot-cpp generated headers

**Research date:** 2026-03-18
**Valid until:** 2026-04-18 (stable -- Godot 4.x Control API is mature and unlikely to change)
