# Project Research Summary

**Project:** Godot MCP Meow — v1.1 UI & Editor Expansion
**Domain:** Godot GDExtension MCP Server Plugin (C++)
**Researched:** 2026-03-18
**Confidence:** HIGH (stack and architecture), MEDIUM (input injection and game-side screenshot bridge)

## Executive Summary

The v1.1 milestone adds five feature areas to an established 18-tool MCP server: UI system tools, animation editing tools, scene file management, input injection, and viewport screenshots. The existing C++ GDExtension architecture — two-process bridge + TCP relay, IO thread + main thread queue, per-module tool functions, `nlohmann::json` returns, `EditorUndoRedoManager` for mutations — extends cleanly to all five areas without any new external dependencies. No new libraries are required; all features use godot-cpp bindings to APIs already present in Godot 4.3+. The tool registry and dispatch chain require only additive modifications. Five new source file pairs and three modified existing files cover the entire v1.1 scope.

The recommended build sequence is: scene file management first (pure `EditorInterface` API, low risk, immediately fills the critical gap where the AI cannot save its own work), then UI tools, then animation tools, and finally the running-game bridge (input injection + game screenshots) which is the most architecturally complex feature. The running-game bridge is the sole feature that requires cross-process communication: the game runs as a separate OS process and neither its viewport nor its `Input` singleton is accessible from the editor plugin. A companion GDScript autoload is the recommended bridge approach, matching what Godot MCP Pro and GoPeak use. The companion autoload serves as shared infrastructure for both game screenshots and input injection, so these two features must be designed together.

The most important pitfalls to resolve before implementation begins are: (1) `set_anchors_preset()` is silently broken — always use `set_anchors_and_offsets_preset()` instead; (2) animation track NodePaths are relative to the AnimationPlayer's root node, not the player itself — paths must be computed dynamically; (3) running-game screenshots and input injection both cross an OS process boundary, making them a fundamental architectural decision rather than a minor implementation detail; and (4) the synchronous `poll()` loop must be extended to support deferred responses for screenshot capture that requires waiting for `RenderingServer::frame_post_draw`. Addressing all four before writing code prevents major rework.

---

## Key Findings

### Recommended Stack

No changes to the existing stack. v1.1 is a pure extension of the validated C++17 + godot-cpp v10+ + nlohmann/json + SCons foundation. All 30+ new godot-cpp headers required by v1.1 features have been verified present in `godot-cpp/gen/include/godot_cpp/classes/`. Five new `.cpp` source files are auto-detected by SCons' existing glob pattern. No build system changes are needed.

**Core technologies (unchanged from v1.0):**
- **C++17 + godot-cpp v10+**: GDExtension plugin — sole implementation language
- **nlohmann/json 3.12.0**: MCP protocol serialization — all tool return values
- **SCons**: Build system — auto-detects new `.cpp` files, no changes needed
- **stdio bridge + TCP relay**: AI client transport — unchanged for v1.1
- **GoogleTest**: Unit testing — new modules need coverage behind `MEOW_GODOT_MCP_GODOT_ENABLED` guards

**New godot-cpp header groups verified present:**
- UI: `control.hpp`, `style_box_flat.hpp`, `theme.hpp`, `box_container.hpp`, `grid_container.hpp`, `margin_container.hpp`
- Animation: `animation.hpp`, `animation_player.hpp`, `animation_mixer.hpp`, `animation_library.hpp`
- Scene management: `editor_interface.hpp` (already used — new methods only)
- Input: `input.hpp`, `input_event_key.hpp`, `input_event_mouse_button.hpp`, `input_event_mouse_motion.hpp`
- Screenshots: `sub_viewport.hpp`, `viewport_texture.hpp`, `image.hpp`, `marshalls.hpp`

### Expected Features

The competitive landscape (GoPeak: 95+ tools, Godot MCP Pro: 162 tools, GDAI: ~30 tools) shows every major gap clearly. v1.1 closes all meaningful gaps while maintaining the zero-dependency GDExtension advantage no competitor has. Projected tool count: v1.0 is 18 tools; v1.1 adds ~14-16 for a total of ~32-34.

**Must have (table stakes):**
- `save_scene` / `open_scene` / `get_open_scenes` — AI currently cannot save its own work; most critical missing capability
- `get_control_properties` — AI needs Control-specific data (anchors, size flags, theme overrides) to understand UI layouts
- `set_theme_override` — per-control theme customization without creating full Theme resources
- `set_layout_preset` (via `set_anchors_and_offsets_preset`) — standard way to position Controls
- `get_animations` / `get_animation_info` — read-only query of AnimationPlayer state
- `create_animation` + `add_track` + `insert_keyframe` — programmatic animation creation
- `capture_editor_viewport` — editor 2D/3D viewport screenshots (no game running needed)
- `inject_key` / `inject_mouse_click` / `inject_action` — game input for AI playtesting

**Should have (competitive differentiators):**
- `create_new_scene` — AI-driven new scene creation
- `capture_game_screenshot` — running game screenshots that close the build-test-fix loop
- `instantiate_scene` (PackedScene) — prefab-style workflows
- Animation playback control (`play`, `stop`, `seek`) for in-editor preview
- UI layout builder compound tool — reduces AI round-trips for common UI patterns

**Defer to v1.2+:**
- Input sequence / macro (timed sequences of inputs)
- Screenshot comparison / diffing
- AnimationTree state machine editing (too complex, poor API surface)
- Gamepad input simulation (low demand signal)
- Close scene (no public API — proposal #8806 still open)

### Architecture Approach

All new tools follow the identical pattern as v1.0 tools: free functions in `.h`/`.cpp` module pairs, `MEOW_GODOT_MCP_GODOT_ENABLED` guards for testability, additive entries in `mcp_tool_registry.cpp`, additive `if (tool_name == "...")` branches in `mcp_server.cpp::handle_request()`, and all Godot API calls on the main thread via `poll()`. Three existing files require substantive changes: `mcp_protocol.h/.cpp` needs a `create_image_tool_result()` function for MCP `ImageContent` responses (screenshots require this — text content cannot carry image data), `mcp_server.cpp` needs new dispatch branches, and `mcp_tool_registry.cpp` needs ~12-15 new ToolDef entries.

**New source file pairs:**
1. **`ui_tools.h/.cpp`** — Control property inspection, `set_anchors_and_offsets_preset()` layout, per-control theme overrides via `add_theme_*_override()`, StyleBoxFlat creation
2. **`animation_tools.h/.cpp`** — AnimationPlayer/Library/Animation CRUD, track management (add/remove), keyframe operations (insert/remove), read-only queries
3. **`editor_tools.h/.cpp`** — Scene file management (save/open/list), editor viewport screenshot capture, editor-side input injection; may split into separate files if it grows beyond ~400 lines

**Modified existing files:**
4. **`mcp_protocol.cpp`** — New `create_image_tool_result()` function wrapping base64 PNG data as MCP `ImageContent`
5. **`mcp_server.cpp`** — New dispatch branches, special-case `capture_viewport` to call `create_image_tool_result()`
6. **`mcp_tool_registry.cpp`** — ~12-15 new ToolDef entries

**New companion component (game-side):**
7. **Companion GDScript autoload** — Cross-process bridge for running game screenshots and input injection; lives in the game project, not the editor plugin

The screenshot data flow is the only new pattern: `capture_viewport` must call `create_image_tool_result()` instead of `create_tool_result()`. All other tools retain the existing text content path.

### Critical Pitfalls

1. **Running game is a separate OS process** (Pitfalls 1 and 4) — both game viewport screenshots and input injection require cross-process communication. `get_viewport()` and `Input::parse_input_event()` in the editor process have zero effect on the running game. Verified by all existing Godot MCP implementations that handle screenshots. Architecture decision required before implementing either feature: use a companion GDScript autoload (recommended), EditorDebuggerPlugin IPC, or limit to editor-only scope.

2. **`set_anchors_preset()` is silently broken** (Pitfall 2) — confirmed broken across Godot 4.0-4.6 in GitHub issues #66651, #67161, #85185, #92487. Always use `set_anchors_and_offsets_preset()`. Anchor presets on Container children have no effect at all — detect parent type and use `size_flags_*` instead.

3. **Animation track paths are relative to the AnimationPlayer's root node** (Pitfall 3) — tracks silently animate nothing if the NodePath is computed relative to the AnimationPlayer itself. Use `root_node.get_path_to(target_node)` to compute correct paths. Also: call `track_set_path()` before `track_insert_key()`, and always add animations through `AnimationLibrary` (Godot 4 requirement, Pitfall 9).

4. **No `await` in C++ GDExtension** (Pitfall 13) — screenshot capture must wait for `RenderingServer::frame_post_draw` before the image is valid. The synchronous `poll()` model must be extended. Options: flag + next-frame fulfillment in the following `poll()` call, or accept one-frame-stale data (sufficient for most use cases and far simpler to implement).

5. **Image-to-base64 pipeline has multiple failure modes** (Pitfall 10) — `save_png_to_buffer()` crashes on compressed textures; large images exceed MCP buffer limits. Always call `img->decompress()` first, resize to a maximum width, prefer JPEG encoding for smaller payload, and use MCP `ImageContent` type rather than text-embedded base64.

---

## Implications for Roadmap

All five feature areas are independent of each other (no inter-feature dependencies), but the running-game bridge (input injection + game screenshots) requires its own companion autoload infrastructure that the other three areas do not need. The natural grouping is: editor-only features first (scene management, UI, animation), then the cross-process bridge features last.

### Phase 1: Scene File Management

**Rationale:** Smallest scope, lowest risk, pure `EditorInterface` API, zero dependencies on new infrastructure. Fills the most critical gap (AI cannot save its work) with 3-4 tools. Provides foundation for multi-scene workflows needed by all later phases.

**Delivers:** `save_scene`, `save_scene_as`, `open_scene`, `get_open_scenes`, `get_current_scene_info`

**Addresses:** Scene file management table stakes; enables AI to persist all edits from v1.0 and future v1.1 work

**Avoids:** Pitfall 6 — validate `get_edited_scene_root()` before saving, check Error return value. Pitfall 14 — do not rely on `scene_changed` signal alone after `open_scene_from_path`; add a frame delay and poll `get_edited_scene_root()`.

**Research flag:** No deeper research needed. API surface is small and well-documented.

### Phase 2: UI System Tools

**Rationale:** Control node APIs are stable and well-documented. No running-game dependency. The critical `set_anchors_preset` bug is fully understood and the fix is one function name change. Theme overrides follow a typed-method pattern that is distinct from `set_node_property`.

**Delivers:** `get_control_properties`, `set_layout_preset`, `set_theme_override`, `remove_theme_override`; optional enhancement to `scene_tools::serialize_node()` to include Control-specific summary fields

**Addresses:** UI system table stakes; enables AI to build and style Control hierarchies

**Avoids:** Pitfall 2 — use `set_anchors_and_offsets_preset()`, never `set_anchors_preset()`. Pitfall 11 — detect Container parents and use `size_flags_*`. Pitfall 5 — use `add_theme_*_override()` methods, not Theme resource assignment from GDExtension code. Pitfall 16 — prefer composite tools over individual setter tools to prevent tool count explosion.

**Research flag:** No deeper research needed. Composite tool design requires API judgment during planning, not external research.

### Phase 3: Animation Tools

**Rationale:** Most complex editor-side feature. The animation hierarchy (Player → Library → Animation → Track → Key) is deep with ordering constraints (path before keys, library before animation). Build read-only tools first (`get_animations`, `get_animation_info`), then mutation tools. No running-game dependency.

**Delivers:** `get_animations`, `get_animation_info`, `create_animation`, `add_track`, `insert_keyframe`, `remove_keyframe`; optional `play_animation` / `stop_animation` for in-editor preview

**Addresses:** Animation system table stakes; enables AI to create and edit animations programmatically

**Avoids:** Pitfall 3 — compute NodePaths from AnimationPlayer's `root_node` property, call `track_set_path` before `track_insert_key`, call `clear_caches()` after modification. Pitfall 9 — always use AnimationLibrary layer, check `has_animation_library("")` before creating. Pitfall 12 — wrap multi-step create operations in a single UndoRedo action.

**Research flag:** A short design spike on UndoRedo feasibility for animation operations is warranted before implementation. ARCHITECTURE.md Decision 4 recommends skipping UndoRedo for animation mutations (too complex), but this tradeoff should be explicit in the plan.

### Phase 4: Editor Viewport Screenshots

**Rationale:** Introduces the only new data flow pattern (ImageContent response). Requires the `mcp_protocol.cpp` extension first. Build after all text-response tools are stable. Editor viewport capture (2D and 3D) requires no game running and no cross-process work — it is the simpler half of the screenshot feature.

**Delivers:** `capture_editor_viewport` (2D and 3D modes), `mcp_protocol::create_image_tool_result()`, optional image resize and compression parameters

**Addresses:** Visual feedback for AI; editor layout inspection; closes the gap with all three competitors on screenshot capability

**Avoids:** Pitfall 7 — connect `frame_post_draw` one-shot callback or accept one-frame stale; always validate `img->is_empty()` and `img->get_width() > 0`. Pitfall 10 — decompress before PNG save, resize to max width, prefer JPEG for smaller payload. Pitfall 13 — decide on deferred-response approach for `poll()` before writing the tool.

**Research flag:** Requires one internal architecture decision (deferred-response mechanism) before implementation. No external research needed — the decision is between two known options.

### Phase 5: Running-Game Bridge (Input Injection + Game Screenshots)

**Rationale:** Highest complexity, highest reward. The companion GDScript autoload is shared infrastructure for both input injection and game-side screenshots — build them together. Build game screenshot first (easier to test visually), then layer input injection on top. Requires a running game for all testing, which slows iteration compared to earlier phases.

**Delivers:** Companion autoload (`mcp_runtime_helper.gd`), `capture_game_screenshot`, `inject_key`, `inject_mouse_click`, `inject_mouse_motion`, `inject_action`

**Addresses:** Build-test-fix loop closure; AI playtesting capability; full feature parity with GoPeak and Godot MCP Pro on runtime tools

**Avoids:** Pitfall 1 — companion autoload with file-based or TCP bridge; never attempt direct viewport access from editor. Pitfall 4 — game-side `Input.parse_input_event()` only, not editor-side. Pitfall 8 — always inject press+release pairs, account for viewport scaling in mouse coordinates, use `Input.action_press/release` for action-based input. Pitfall 13 — game-side autoload uses GDScript `await RenderingServer.frame_post_draw`.

**Research flag:** Needs a research spike on the IPC mechanism before implementation. File-based polling vs. TCP connection from autoload vs. EditorDebuggerPlugin each have distinct tradeoffs for latency, reliability, and implementation complexity. Godot MCP Pro and GoPeak (both use autoload + WebSocket/TCP) are the primary architecture references.

### Phase 6: Prompt Templates

**Rationale:** Pure text, no API risk. Depends on all tool definitions being finalized. Low effort, meaningful DX improvement for AI users. No competitor ships curated prompts alongside their tools.

**Delivers:** New MCP prompt templates for UI building (`build_ui_layout`), animation workflows (`setup_animation`), and playtesting (`debug_game`)

**Research flag:** No research needed.

### Phase Ordering Rationale

- Phases 1-3 are editor-only and share no dependencies. Scene Management comes first because it gives the AI the ability to persist all subsequent work immediately.
- Phase 4 (editor screenshots) must come after Phases 1-3 are stable because it introduces the first new response type in the protocol layer; a bug in `create_image_tool_result()` would affect all subsequent testing.
- Phase 5 must come last because it requires a running game for all testing (slows iteration), and its IPC design decision must be made carefully with full understanding of the completed editor-side architecture.
- Phase 6 can only be written once all tool names are final.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All required godot-cpp headers verified present locally. No new dependencies introduced. Build system unchanged. |
| Features | HIGH | Competitive landscape well-researched across 3 named competitors. Table stakes and deferral decisions are grounded in market evidence and API feasibility. |
| Architecture | HIGH | New modules follow established patterns exactly. Three file modifications are additive and low-risk. ImageContent response is specified by MCP 2025-06-18 spec. |
| Pitfalls | HIGH (editor-side) / MEDIUM (game bridge) | Editor-side pitfalls verified via Godot GitHub issues with specific issue numbers. Game-side bridge pitfalls are architectural in nature; the specific IPC mechanism has not been prototyped in this codebase. |

**Overall confidence:** HIGH for Phases 1-4. MEDIUM for Phase 5 (game bridge IPC not yet prototyped).

### Gaps to Address

- **Phase 5 IPC mechanism**: File-based polling, TCP connection from autoload to editor, or EditorDebuggerPlugin? A prototype sprint is needed before committing to the Phase 5 plan. Godot MCP Pro and GoPeak are the primary references.
- **Animation UndoRedo feasibility**: Can animation track/keyframe mutations be wrapped in `EditorUndoRedoManager` actions cleanly? ARCHITECTURE.md recommends skipping it (too complex), but this tradeoff should be made explicit in the Phase 3 plan so users understand which operations are not undoable.
- **Viewport screenshot timing**: One-frame-stale (accept previous frame's render) vs. deferred-response (wait for `frame_post_draw`). For editor viewports that render continuously, one-frame-stale is almost always acceptable and is far simpler. This decision should be recorded in the Phase 4 plan.
- **`open_scene_from_path` signature change** between Godot 4.3 and 4.4 (added `set_inherited` parameter): apply the version-adaptive tool registry pattern from v1.0.
- **`parse_input_event` + `use_accumulated_input = false` regression in Godot 4.4.1**: reported on forum but no GitHub issue number confirmed — needs validation during Phase 5 planning.

---

## Sources

### Primary (HIGH confidence)

- Godot 4.3 official docs: EditorInterface, Animation, AnimationPlayer, AnimationMixer, AnimationLibrary, Control, StyleBox, Theme, Container, InputEvent, Image, Marshalls, Viewport — all API signatures verified
- godot-cpp headers: all 30+ required headers confirmed present locally in `godot-cpp/gen/include/godot_cpp/classes/`
- Godot engine source (godotengine/godot): `control.cpp`, `animation.cpp`, `box_container.cpp`, `grid_container.cpp`, `editor_interface.h` — implementation details verified
- MCP spec 2025-06-18: `ImageContent`, `TextContent` response formats
- Rokojori API Mirror: cross-verification of Godot 4.3 API signatures for EditorInterface, Animation, AnimationMixer, AnimationLibrary, Control, Input, Image, StyleBoxFlat, Theme, Viewport

### Secondary (MEDIUM confidence)

- Godot GitHub issues: #66651, #67161, #85185, #92487 (anchor preset bug), #17313 (animation track paths), #87692, #85931, #96808, #73557 (input injection bugs), #50787, #108535 (image pipeline bugs), #97427 (scene_changed signal bug), #8806 (no close_scene API), #84550 (theme overrides missing for GDExtension)
- godot-cpp issues: #1332 (ThemeDB slowdown from GDExtension constructor)
- Competing implementations: GoPeak (95+ tools, input + screenshots), Godot MCP Pro (162 tools, autoload approach), GDAI MCP (~30 tools) — architecture reference for game bridge approach
- Community sources: ShaggyDev viewport screenshot tutorial (Feb 2025), Godot forum threads on GDExtension async patterns and input injection
- Godot proposals: #8806 (close_scene), #8777 (GDExtension debugger tooling), #10994 (editor-game communication)

### Tertiary (LOW confidence)

- `parse_input_event` + `use_accumulated_input = false` regression in Godot 4.4.1: forum-reported, no GitHub issue confirmed — needs validation at Phase 5
- EditorDebuggerPlugin GDExtension support limitations (proposal #8777): proposal exists but current state of GDExtension support should be re-verified at Phase 5 planning

---

*Research completed: 2026-03-18*
*Ready for roadmap: yes*
