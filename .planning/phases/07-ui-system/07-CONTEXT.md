# Phase 7: UI System - Context

**Gathered:** 2026-03-18
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can construct, layout, and style Control node UIs including theme customization. Covers layout presets, theme overrides (colors, fonts, font sizes, styleboxes), StyleBoxFlat creation, UI property queries, and Container layout configuration. Does NOT cover theme resource files (.tres), global theme editing, or custom theme types.

Requirements: UISYS-01, UISYS-02, UISYS-03, UISYS-04, UISYS-05, UISYS-06

</domain>

<decisions>
## Implementation Decisions

### Tool Architecture — 6 Dedicated UI Tools
- **set_layout_preset** — Set Control layout preset (full_rect, center, top_wide, etc.). Calls `set_anchors_and_offsets_preset()`. Maps to SC1.
- **set_theme_override** — Batch set theme overrides (colors, fonts, font sizes). Single call sets multiple overrides. Maps to SC2.
- **create_stylebox** — Create StyleBoxFlat with properties and auto-apply to node as theme override. Maps to SC3.
- **get_ui_properties** — Query Control's UI-specific properties (anchors, offsets, size_flags, minimum_size, focus_neighbors, etc.). Maps to SC4.
- **set_container_layout** — Configure Container parameters + optional batch child size_flags. Maps to SC5.
- **get_theme_overrides** — Query all current theme overrides on a node, categorized by type. Supports SC2.

### Theme Override Design
- set_theme_override uses batch model: `{"font_color": "#ff0000", "font_size": 16}` — reduces round trips
- Override types auto-detected from key names (Godot convention: color names end in _color, etc.)
- create_stylebox auto-applies to target node — `node_path` + `override_name` parameters, one-step operation
- Only StyleBoxFlat supported (success criteria only requires Flat, most common type)
- get_theme_overrides returns categorized: `{"colors": {...}, "fonts": {...}, "font_sizes": {...}, "styles": {...}}`

### Layout & Container Design
- set_layout_preset supports ALL Godot LayoutPreset enum values (16 presets): PRESET_TOP_LEFT through PRESET_FULL_RECT, passed as string names like "full_rect", "center"
- set_container_layout includes optional `child_size_flags` parameter to batch-set all direct children's size_flags_horizontal/vertical
- get_ui_properties returns comprehensive data: anchors (4), offsets (4), size_flags (h/v), minimum_size, pivot_offset, grow_direction (h/v), focus_neighbors (4), layout_direction

### Code Organization
- New `ui_tools.h` / `ui_tools.cpp` file pair — single module for all 6 functions
- Follows established pattern: free functions returning nlohmann::json
- All mutation tools take EditorUndoRedoManager* for undo/redo support

### Claude's Discretion
- Exact preset string names and parsing strategy
- Theme override key detection heuristics
- StyleBoxFlat property names and ranges
- get_ui_properties inclusion of optional vs always-present fields
- Container type detection (VBoxContainer, HBoxContainer, GridContainer, etc.)
- Error messages and validation details

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` -- Full v1.1 requirements, Phase 7 maps to UISYS-01..06
- `.planning/ROADMAP.md` -- Phase 7 success criteria, dependency chain

### Prior Phase Context
- `.planning/phases/1/1-CONTEXT.md` -- Phase 1: connection model, error patterns
- `.planning/phases/03-script-project-management/03-CONTEXT.md` -- Phase 3: threading model, dispatch patterns
- `.planning/phases/06-scene-file-management/06-CONTEXT.md` -- Phase 6: latest tool design patterns

### Existing Implementation
- `src/mcp_server.h` / `src/mcp_server.cpp` -- MCPServer: handle_request dispatch, 23 current tools
- `src/scene_mutation.h` / `src/scene_mutation.cpp` -- set_node_property, UndoRedo pattern, variant_parser
- `src/mcp_tool_registry.h` / `src/mcp_tool_registry.cpp` -- ToolDef struct, 23 tool definitions
- `src/variant_parser.h` / `src/variant_parser.cpp` -- String→Variant parsing for Color, Vector2, etc.
- `src/scene_file_tools.h` / `src/scene_file_tools.cpp` -- Latest tool module pattern (Phase 6)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `variant_parser.h`: parse_variant() for Color, Vector2, etc. — reuse for theme override values
- `MCPServer::handle_request()`: tool dispatch — add 6 new handlers
- `ToolDef` registry: version-gated registration — register 6 new tools
- UndoRedo pattern from scene_mutation.cpp: all UI mutation tools use same pattern
- EditorInterface::get_singleton(): editor access for all operations

### Key Godot APIs
- `Control::set_anchors_and_offsets_preset()` — layout presets
- `Control::add_theme_color_override()`, `add_theme_font_override()`, `add_theme_font_size_override()`, `add_theme_stylebox_override()` — theme overrides
- `Control::get_theme_color()`, `get_theme_font()`, etc. — query current values
- `Control::has_theme_color_override()`, etc. — check if override exists
- `StyleBoxFlat` properties: bg_color, corner_radius_*, border_width_*, border_color, content_margin_*, anti_aliasing
- `Container::queue_sort()` — refresh layout after property changes

### Integration Points
- `handle_request()` in mcp_server.cpp: add 6 new tool handlers
- `mcp_tool_registry.cpp`: register 6 new ToolDef entries
- New `ui_tools.h/.cpp` files added to SConstruct build (auto via Glob)

</code_context>

<deferred>
## Deferred Ideas

- Theme resource (.tres) creation/editing — complex, separate feature
- Global theme management — beyond per-control overrides
- Custom theme types — advanced, not in v1.1
- StyleBoxTexture / StyleBoxLine support — add when needed
- Theme inheritance visualization — advanced query feature

</deferred>

---

*Phase: 07-ui-system*
*Context gathered: 2026-03-18*
