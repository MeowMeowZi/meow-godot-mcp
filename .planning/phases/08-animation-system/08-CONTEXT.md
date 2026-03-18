# Phase 8: Animation System - Context

**Gathered:** 2026-03-18
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can create animations with tracks and keyframes for any animatable property. Covers AnimationPlayer creation with AnimationLibrary, typed track management, keyframe CRUD, animation property queries, and animation settings (duration, loop, step). Does NOT cover AnimationTree, blend spaces, state machines, or animation playback preview.

Requirements: ANIM-01, ANIM-02, ANIM-03, ANIM-04, ANIM-05

</domain>

<decisions>
## Implementation Decisions

### Tool Architecture — 5 Dedicated Animation Tools
- **create_animation** — Creates AnimationPlayer node + AnimationLibrary (default "") + named Animation resource. One-step complete setup. UndoRedo supported. Maps to SC1.
- **add_animation_track** — Adds a typed track (value, position_3d, rotation_3d, scale_3d) to an existing Animation with correct node path. No UndoRedo (too complex). Maps to SC2.
- **set_keyframe** — Insert, update, or remove keyframes on any track via `action` parameter ("insert"/"update"/"remove"). Values parsed via variant_parser. No UndoRedo. Maps to SC3.
- **get_animation_info** — Query AnimationPlayer: animation list, per-animation track structure, per-track keyframe data (time + value + transition). Full depth. Maps to SC4.
- **set_animation_properties** — Set animation duration, loop mode, step. UndoRedo supported. Maps to SC5.

### Track & Keyframe Design
- 4 supported track types: value, position_3d, rotation_3d, scale_3d — covers most common animation scenarios
- UndoRedo only for create_animation and set_animation_properties; track and keyframe operations skip UndoRedo (research noted "too complex" for Animation mutations)
- Keyframe values parsed as strings via existing variant_parser — "Vector3(1,0,0)", "0.5", "Color(1,0,0,1)"
- set_keyframe uses `action` parameter: "insert" (add new), "update" (modify existing at time), "remove" (delete at time)
- get_animation_info returns complete data: animation list → per-animation tracks → per-track keyframes (time + value + transition type)

### AnimationLibrary Management
- Automatic library management: create_animation auto-creates default library (empty string name "")
- User doesn't need to understand AnimationLibrary concept — it's an implementation detail
- All tools identify animations by `player_path` + `animation_name` — uniform parameter pattern
- animation_name maps to library lookup internally

### Animation Properties
- Loop mode as string enum: "none", "linear", "pingpong" — maps to Godot Animation::LoopMode
- No playback preview tool — out of scope, SC5 only requires correct property values
- Duration, loop_mode, step are the three settable properties

### Code Organization
- New `animation_tools.h` / `animation_tools.cpp` — single module for all 5 functions
- Free functions returning nlohmann::json
- Mutation tools with UndoRedo take EditorUndoRedoManager* parameter

### Claude's Discretion
- Track type string parsing and validation
- Keyframe transition type support (linear, cubic, etc.)
- Animation query response JSON structure details
- Error messages and edge case handling
- Whether to support multiple AnimationLibraries (beyond default "")
- How to handle track index vs track path identification

</decisions>

<canonical_refs>
## Canonical References

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` — Phase 8 maps to ANIM-01..05
- `.planning/ROADMAP.md` — Phase 8 success criteria

### Prior Phase Context
- `.planning/phases/06-scene-file-management/06-CONTEXT.md` — Latest tool design patterns
- `.planning/phases/07-ui-system/07-CONTEXT.md` — 6-tool module pattern, batch parameter design

### Existing Implementation
- `src/mcp_server.cpp` — MCPServer: handle_request dispatch, 29 current tools
- `src/mcp_tool_registry.cpp` — 29 ToolDef entries
- `src/scene_mutation.cpp` — UndoRedo pattern, create_node as reference
- `src/variant_parser.cpp` — String→Variant parsing for keyframe values
- `src/ui_tools.cpp` — Latest tool module pattern (Phase 7)

</canonical_refs>

<code_context>
## Existing Code Insights

### Key Godot APIs
- `AnimationPlayer` — manages animations, has `add_animation_library()`, `get_animation_list()`
- `AnimationLibrary` — container for named Animation resources, `add_animation()`, `get_animation()`
- `Animation` — tracks and keyframes, `add_track()`, `track_insert_key()`, `track_set_key_value()`, `track_remove_key()`
- `Animation::TrackType` — TYPE_VALUE, TYPE_POSITION_3D, TYPE_ROTATION_3D, TYPE_SCALE_3D
- `Animation::LoopMode` — LOOP_NONE, LOOP_LINEAR, LOOP_PINGPONG

### Integration Points
- `handle_request()`: add 5 new tool handlers
- `mcp_tool_registry.cpp`: register 5 new ToolDef entries
- New `animation_tools.h/.cpp` added to build (auto via Glob)

</code_context>

<deferred>
## Deferred Ideas

- AnimationTree / blend space support — complex state machine
- Animation playback preview tool — beyond current scope
- Bezier curve track type — specialized
- Audio track type — requires audio stream resources
- Method call track type — security considerations
- Multiple AnimationLibrary management — keep simple with default

</deferred>

---

*Phase: 08-animation-system*
*Context gathered: 2026-03-18*
