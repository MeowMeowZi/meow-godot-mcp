# Phase 24: Composite Tools - Context

**Gathered:** 2026-03-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Add 5 composite tools that combine multiple primitive operations into single calls with atomic UndoRedo: find_nodes, batch_set_property, create_character, create_ui_panel, duplicate_node.

</domain>

<decisions>
## Implementation Decisions

### Architecture
- All composite tools wrap sub-operations in a single UndoRedo action (create_action + commit_action). Ctrl+Z undoes entire composite
- New `composite_tools.h/cpp` module — same level as physics_tools, tilemap_tools
- Composite tools call existing internal C++ functions, NOT JSON tool dispatch

### find_nodes
- Returns `{"nodes": [{"path": "...", "type": "..."}, ...], "count": N}`
- Search parameters: `type` (class name), `name_pattern` (glob or substring), `property_filter` (property_name + value)
- Searches entire scene tree from root (or specified root_path)

### batch_set_property
- Two modes: `node_paths: [...]` (explicit list) OR `type_filter: "Label"` (all nodes of type)
- Applies `property_name` + `value` to all matched nodes in single UndoRedo action
- Returns count of modified nodes and any errors per node

### create_character
- Parameters: `name`, `type` (2d/3d), `shape_type`, `parent_path`
- Optional: `sprite_texture` (res:// path), `script_template` (none/basic_movement)
- Creates: CharacterBody2D/3D → CollisionShape2D/3D (with shape) → Sprite2D/3D (if texture)
- basic_movement template: `extends CharacterBody2D` + SPEED/JUMP_VELOCITY + move_and_slide()
- Single UndoRedo action for entire tree

### create_ui_panel
- Simplified declarative JSON: `{root_type, children: [{type, text/name, ...}], style: {bg_color, ...}}`
- Maximum two levels of nesting (root + children, no grandchildren)
- Creates container + children + applies StyleBox/theme overrides from style
- Single UndoRedo action

### duplicate_node
- Deep-copies node subtree to new parent with optional rename
- Script references are shared (not file-copied)
- Preserves all children, properties, and signal connections within the subtree
- Single UndoRedo action

### Claude's Discretion
- Internal helper function organization
- Error handling for individual sub-operations within a composite
- Exact basic_movement GDScript template content

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `scene_mutation.cpp` — create_node, delete_node, set_property with UndoRedo
- `physics_tools.cpp` — create_collision_shape already creates CollisionShape + Shape in one go
- `variant_parser.cpp` — string-to-Godot-type conversion for property values
- `ui_tools.cpp` — set_layout_preset, set_theme_override, create_stylebox

### Established Patterns
- UndoRedo via `EditorUndoRedoManager::get_singleton()`
- `create_action()` / `add_do_method()` / `add_undo_method()` / `commit_action()`
- Tool dispatch in mcp_server.cpp: `if (tool_name == "xxx") { ... return make_tool_response(...) }`

### Integration Points
- `mcp_server.cpp` dispatch — add 5 new tool dispatch blocks
- `mcp_tool_registry.cpp` — register 5 new ToolDef entries
- `composite_tools.h/cpp` — new module

</code_context>

<specifics>
## Specific Ideas

No specific requirements — standard composite tool implementation based on research findings.

</specifics>

<deferred>
## Deferred Ideas

- create_scene_from_template — deferred to v2 (high complexity)

</deferred>
