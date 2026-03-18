# Feature Landscape: v1.1 UI & Editor Expansion

**Domain:** MCP Server plugin for Godot Engine (GDExtension/C++) -- v1.1 milestone
**Researched:** 2026-03-18
**Competitive context:** GoPeak (95+ tools), Godot MCP Pro (162 tools), GDAI MCP (~30 tools)
**Focus:** New features only -- UI system, Animation, Scene file management, Input injection, Viewport screenshots

## Existing Capabilities (v1.0 baseline)

18 MCP tools already shipped: get_scene_tree, create_node, set_node_property, delete_node, read_script, write_script, edit_script, attach_script, detach_script, list_project_files, get_project_settings, get_resource_info, run_game, stop_game, get_game_output, get_node_signals, connect_signal, disconnect_signal. Plus: editor dock panel, 4 MCP prompts, MCP Resources, version-adaptive tool registry, UndoRedo integration on all mutations.

**Key architectural fact for v1.1:** The existing `create_node` and `set_node_property` tools already work with ANY node type including Control nodes and AnimationPlayer. The v1.1 features need to add **domain-aware intelligence** on top, not re-implement basic node manipulation.

---

## Feature Area 1: UI System (Control Nodes, Theme, StyleBox, Container Layout)

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Query UI tree structure with Control-specific properties | AI needs to understand UI layout to modify it. Existing get_scene_tree returns generic info; Control nodes need anchor, offset, size, min_size, size_flags, theme overrides. | Low | Extends get_scene_tree serialization |
| Create Control node hierarchies | AI needs to build menus, HUDs, dialogs. The existing create_node already supports this, but a UI-aware tool should set up anchors/size_flags automatically for common patterns. | Med | Existing create_node |
| Set theme overrides on Controls | `add_theme_color_override`, `add_theme_font_size_override`, `add_theme_stylebox_override` are the standard APIs. AI needs to style individual controls without creating full Theme resources. | Med | Existing set_node_property (partial), new dedicated tool |
| Set anchor presets | `Control.set_anchors_preset()` with values like PRESET_FULL_RECT, PRESET_CENTER, etc. Most common UI operation after node creation. | Low | Existing set_node_property |
| Configure Container properties | Set HBoxContainer/VBoxContainer separation, GridContainer columns, MarginContainer margins. All via `add_theme_constant_override("separation", N)` or property setters. | Low | Existing set_node_property + theme override tool |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| UI layout builder (compound tool) | Single tool call: "create a VBox with 3 buttons, full-rect anchored, 10px margin" instead of 5+ individual calls. Reduces AI round-trips by 80% for common UI patterns. | Med | Compound tool wrapping create_node + set_property in a single UndoRedo action |
| Get effective theme values | Query what theme colors/fonts/styleboxes a Control actually uses (resolved through the theme hierarchy, not just overrides). Uses `get_theme_color()`, `get_theme_stylebox()` etc. | Low | Read-only query, no mutation |
| Create/apply Theme resources | Create a Theme .tres resource with colors, fonts, styleboxes and apply it to a Control subtree. Enables consistent styling across a UI. | High | Needs resource creation + file I/O |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Visual UI editor / drag-and-drop simulation | Editor's built-in UI editor is far superior. Trying to replicate it via MCP tools is futile. | Provide structural manipulation (create/configure nodes), let user do visual tweaking in editor |
| Font file management / import | Font import is complex (TTF/OTF, size variations, MSDF). Better handled by user in editor. | Support setting font_size overrides and referencing existing font resources |
| RichTextLabel BBCode generation | Too domain-specific, AI can generate BBCode strings directly via set_node_property. | Let AI use write_script or set_node_property with bbcode_text property |

### User Workflows

**"Build me a main menu"**: AI creates VBoxContainer (full-rect), adds MarginContainer child with padding, adds VBoxContainer inside with Title label + 3 Buttons (Play, Settings, Quit), sets size_flags, connects button pressed signals.

**"Style this panel"**: AI queries current theme, creates StyleBoxFlat with corner_radius and bg_color, applies as override to the PanelContainer.

### Godot API Surface (verified)

- `Control.set_anchors_preset(preset, keep_offsets)` -- LayoutPreset enum (PRESET_TOP_LEFT through PRESET_FULL_RECT)
- `Control.add_theme_color_override(name, color)` / `add_theme_stylebox_override(name, stylebox)` / `add_theme_font_size_override(name, size)` / `add_theme_constant_override(name, value)`
- `Control.get_theme_color(name, theme_type)` / `get_theme_stylebox()` etc.
- `Control.set_h_size_flags(flags)` / `set_v_size_flags(flags)` -- SIZE_FILL, SIZE_EXPAND, SIZE_EXPAND_FILL, SIZE_SHRINK_BEGIN/CENTER/END
- `Control.custom_minimum_size` property
- Container children: position/size managed by parent. Never set directly.
- `StyleBoxFlat` / `StyleBoxTexture` / `StyleBoxEmpty` -- creatable via ClassDB::instantiate

**Confidence:** HIGH -- All APIs verified in Godot 4.3 official docs and source code.

---

## Feature Area 2: Animation System (AnimationPlayer, Tracks, Keyframes)

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| List animations on a node | Query AnimationPlayer/AnimationMixer for all animation libraries and animation names. Uses `get_animation_library_list()` + `get_animation_list()`. | Low | get_scene_tree (find AnimationPlayer nodes) |
| Get animation details | Return track count, track paths, track types, keyframe times/values, animation length, loop mode for a specific animation. | Med | Needs Animation resource introspection |
| Create new animation | Create Animation resource, set length/loop, add to AnimationLibrary on AnimationPlayer. Uses `AnimationLibrary.add_animation()`. | Med | Needs AnimationPlayer node to exist |
| Add track to animation | `Animation.add_track(type)` + `track_set_path()`. Track types: TYPE_VALUE, TYPE_METHOD, TYPE_POSITION_3D, TYPE_ROTATION_3D, TYPE_SCALE_3D, TYPE_BEZIER, TYPE_AUDIO, TYPE_ANIMATION. | Med | Existing animation must exist |
| Insert keyframe | `Animation.track_insert_key(track_idx, time, value, transition)`. Core animation editing operation. | Med | Track must exist |
| Remove keyframe / track | `track_remove_key()`, `remove_track()`. Needed for editing existing animations. | Low | Animation must exist |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Animation builder (compound tool) | Create full animation in one call: specify node, property, keyframes array. Eliminates the multi-step add_track/set_path/insert_key dance. | Med | Massive DX improvement for AI -- one tool call instead of 4+ |
| Play/stop animation in editor | Trigger AnimationPlayer.play() in editor preview mode. AI can verify animations look correct without running the full game. | Low | AnimationPlayer supports editor preview natively |
| Batch keyframe insertion | Insert multiple keyframes at once on multiple tracks. Typical AI workflow: "animate this node moving from A to B while fading in." | Med | Reduces round-trips significantly |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| AnimationTree state machine editing | AnimationTree blend trees and state machines are extremely complex to edit programmatically. The visual editor is far better suited. | Support AnimationPlayer directly; AnimationTree can reference the same AnimationLibrary |
| Skeletal animation / bone manipulation | Extremely specialized, requires imported 3D model context. Out of scope for MCP editor tooling. | Support basic animation track types only |
| Bezier curve handle editing | Bezier control points are finicky to set correctly programmatically. Better done in editor UI. | Support TYPE_BEZIER track creation and basic key insertion; leave curve handles to manual editing |

### User Workflows

**"Add a bounce animation to this sprite"**: AI creates Animation on AnimationPlayer, adds TYPE_VALUE track for "position:y", inserts keyframes at 0s (0), 0.3s (-50), 0.6s (0), sets loop mode, plays preview.

**"What animations does this character have?"**: AI queries AnimationPlayer, returns list of libraries/animations with their track counts and durations.

### Godot API Surface (verified)

- `AnimationMixer` (base of AnimationPlayer since 4.2): `add_animation_library(name, lib)`, `get_animation_library(name)`, `get_animation_library_list()`, `get_animation_list()`, `has_animation(name)`
- `AnimationLibrary`: `add_animation(name, anim)`, `get_animation(name)`, `get_animation_list()`, `has_animation(name)`, `remove_animation(name)`
- `Animation`: `add_track(type, at_position)`, `remove_track(idx)`, `get_track_count()`, `track_set_path(idx, path)`, `track_get_path(idx)`, `track_insert_key(idx, time, key, transition)`, `track_get_key_count(idx)`, `track_get_key_time(idx, key_idx)`, `track_get_key_value(idx, key_idx)`, `track_remove_key(idx, key_idx)`, `set_length(s)`, `set_loop_mode(mode)`
- Animation naming: `"library_name/animation_name"` for non-default libraries; just `"animation_name"` for default (empty-name) library
- `track_set_path` MUST be called before inserting keys -- otherwise errors occur

**Confidence:** HIGH -- Animation API is stable since Godot 4.0; AnimationMixer refactor landed in 4.2, well-documented.

---

## Feature Area 3: Scene File Management (Save/Load/Switch Scenes)

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Save current scene | `EditorInterface::save_scene()` returns Error (OK or ERR_CANT_CREATE). Essential: AI edits a scene, then saves. Without this, AI changes are lost. | Low | None -- direct EditorInterface API |
| Open/switch scene | `EditorInterface::open_scene_from_path(path)`. AI needs to navigate between scenes to make cross-scene edits. | Low | None -- direct EditorInterface API |
| Get open scenes list | `EditorInterface::get_open_scenes()` returns PackedStringArray. AI needs to know what scenes are open. | Low | None |
| Create new scene | Create a root node (any Node type), set it as scene root, save to .tscn path. Uses create_node for root + save_scene_as or ResourceSaver. | Med | Existing create_node |
| Reload scene from disk | `EditorInterface::reload_scene_from_path(path)`. Discard unsaved changes and reload. Useful for AI reverting bad changes. | Low | None |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Scene instantiation (PackedScene) | Instantiate a .tscn as a child node in current scene. Essential for prefab-style workflows ("add a player.tscn instance here"). | Med | Uses ResourceLoader::load() + PackedScene::instantiate() |
| Get current scene path/info | Return the path of the currently edited scene, whether it has unsaved changes, its root node type. | Low | `EditorInterface::get_edited_scene_root()->get_scene_file_path()` |
| Mark scene as unsaved | `EditorInterface::mark_scene_as_unsaved()`. Useful when AI makes changes outside UndoRedo (file edits). | Low | Direct API call |
| Save scene as (new path) | Save current scene to a different file path. Enables "duplicate this scene" workflows. | Med | Needs EditorInterface or ResourceSaver API |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Close scene / close tab | No public API exists (`open_scene_from_path` exists but `close_scene` does not -- active Godot proposal #8806). Attempting private API access is fragile. | Let user close scenes manually. AI can open_scene to switch context. |
| Scene diffing / merge | Extremely complex. Scene files are Godot's custom .tscn format. Not worth implementing. | AI can query scene tree and compare programmatically if needed |
| Batch scene operations | Opening/editing/saving multiple scenes in sequence should be multiple tool calls, not one atomic operation. Simpler, more debuggable. | Provide granular tools; AI orchestrates the sequence |

### User Workflows

**"Create a new level scene"**: AI creates Node2D as root, adds TileMapLayer + Camera2D + Player instance, saves as res://levels/level_02.tscn.

**"Switch to the main menu scene and add a button"**: AI calls open_scene(res://scenes/main_menu.tscn), waits, then creates Button node.

**"Save my work"**: AI calls save_scene() after making edits.

### Godot API Surface (verified)

- `EditorInterface::save_scene()` -> Error
- `EditorInterface::open_scene_from_path(path, set_inherited=false)` -> void (NOTE: set_inherited param added in recent Godot, may not exist in 4.3. Need version check.)
- `EditorInterface::reload_scene_from_path(path)` -> void
- `EditorInterface::get_open_scenes()` -> PackedStringArray (returns empty string for unsaved scenes)
- `EditorInterface::mark_scene_as_unsaved()` -> void
- `EditorInterface::get_edited_scene_root()` -> Node* (already used by v1.0 tools)
- Known bug: `scene_changed` signal not fired when opening scene from empty scene state (Godot issue #97427)
- No `close_scene` API exists (proposal #8806 open)

**Confidence:** HIGH -- All verified against Godot 4.3 docs and source headers. One caveat: `open_scene_from_path` signature changed between 4.3 and 4.4 (added `set_inherited` param). Need compatibility handling.

---

## Feature Area 4: Input Injection (Keyboard/Mouse to Running Game)

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Inject keyboard key press/release | Create InputEventKey, set keycode + pressed state, inject via communication channel to running game. Core requirement for AI playtesting. | High | Running game communication bridge (see Architecture) |
| Inject mouse click at position | Create InputEventMouseButton with button_index + position + pressed state. AI needs to click UI buttons, game objects. | High | Running game communication bridge |
| Inject mouse movement | Create InputEventMouseMotion with position delta. AI needs to move cursor for hover effects, aiming. | High | Running game communication bridge |
| Inject input action | Create InputEventAction with action name + pressed state. Higher-level than raw keys: "press jump" instead of "press Space". | High | Running game communication bridge |

### Architecture Challenge: Editor vs. Running Game Processes

**CRITICAL**: In Godot 4, the running game is a separate OS process from the editor. `Input.parse_input_event()` called from the editor plugin injects events into the EDITOR, not the running game. There are three viable approaches for injecting input into the running game:

**Approach A -- Autoload Script Injection (used by Godot MCP Pro)**
- Inject a GDScript autoload into the running game that opens a TCP/WebSocket connection back to the editor plugin
- Editor sends input commands over this channel
- Autoload script creates InputEvent objects and calls `Input.parse_input_event()` from within the game process
- Pros: Full control, works reliably, can also capture screenshots and game state
- Cons: Modifies project (adds autoload), requires coordination between editor and game processes

**Approach B -- EngineDebugger IPC (used by GodotIQ)**
- Use Godot's built-in EditorDebuggerPlugin + EngineDebugger message system
- Editor sends custom messages like `"mcp:inject_key"` via `EditorDebuggerSession.send_message()`
- Game-side script registers capture with `EngineDebugger.register_message_capture("mcp")` and handles injection
- Pros: Uses official Godot IPC, no extra networking
- Cons: Still requires game-side autoload for message capture; EngineDebugger is less documented; GDExtension support for EditorDebuggerPlugin has known limitations (proposal #8777)

**Approach C -- Companion GDScript Autoload (recommended for this project)**
- Ship a companion GDScript autoload (e.g., `mcp_runtime_helper.gd`) that users enable in Project Settings
- This script connects back to the GDExtension's TCP server (or a secondary port) when the game runs
- Handles: input injection, viewport screenshot capture, runtime scene tree query
- Pros: Clean separation, no hack-injection, user explicitly opts in, works across all Godot 4.3+ versions
- Cons: Requires user to enable autoload (one-time setup step)

**Recommendation:** Approach C. It keeps the GDExtension plugin clean, gives users explicit control, and avoids fragile auto-injection. The autoload can be auto-configured by the editor dock panel.

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Input sequence / macro | Execute a sequence of inputs with timing: "press W for 2 seconds, then click at (400,300)". AI playtesting requires timed sequences. | High | Built on top of basic injection |
| Action-level injection | "Press the 'jump' action" instead of "press Space key". More robust against input remapping. | Med | Uses InputEventAction |
| Wait for game state | Block until a condition is met after input (e.g., "click Play button, wait for scene transition"). Enables reliable multi-step interactions. | High | Requires game-side state reporting |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| OS-level input injection (SendInput/xdotool) | Goes outside Godot's input system, breaks cross-platform, unreliable, security concerns. | Use Godot's Input.parse_input_event() inside the game process |
| Touch/gesture simulation | Too specialized, most Godot MCP users develop desktop/console games. | Support mouse events which Godot auto-converts to touch on mobile |
| Gamepad input simulation | Low priority; keyboard/mouse covers 95% of AI testing needs. Add based on demand. | Defer to v1.2+ |

### User Workflows

**"Playtest this: click the Start button, then press WASD to move"**: AI runs game, waits for load, injects mouse click at Start button position, then injects key presses for movement, captures screenshots to verify.

**"Test the jump mechanic"**: AI runs game, injects "jump" action input, captures game output for errors, takes screenshot to verify animation.

### Godot API Surface (verified for game-side injection)

- `Input::get_singleton()->parse_input_event(event)` -- feeds InputEvent through the game's input pipeline
- `InputEventKey`: `set_keycode(Key)`, `set_physical_keycode(Key)`, `set_pressed(bool)`, `set_echo(bool)`
- `InputEventMouseButton`: `set_button_index(MouseButton)`, `set_pressed(bool)`, `set_position(Vector2)`, `set_global_position(Vector2)`, `set_double_click(bool)`
- `InputEventMouseMotion`: `set_position(Vector2)`, `set_global_position(Vector2)`, `set_relative(Vector2)`
- `InputEventAction`: `set_action(StringName)`, `set_pressed(bool)`, `set_strength(float)`
- All InputEvent classes constructable via `Ref<T>; ref.instantiate()`
- Known issue: `parse_input_event` for InputEventMouseButton may be ignored when `use_accumulated_input` is false (Godot forum report, 4.4.1)

**Confidence:** MEDIUM -- The injection APIs themselves are well-documented and stable. The challenge is the editor-to-game communication bridge, which is an architectural design decision rather than an API uncertainty. All three approaches (autoload, debugger IPC, companion script) are proven by competitors.

---

## Feature Area 5: Viewport Screenshot Capture

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Capture running game screenshot | Get viewport image from running game, encode as PNG, return as base64 or save to file. This is THE feature that closes the AI build-test-fix loop. | High | Running game communication bridge (same as input injection) |
| Capture editor 2D viewport | `EditorInterface::get_editor_viewport_2d()->get_texture()->get_image()`. AI can "see" the 2D editor view. | Med | None -- editor-side only |
| Capture editor 3D viewport | `EditorInterface::get_editor_viewport_3d(0)->get_texture()->get_image()`. AI can "see" the 3D editor view. Idx 0-3 for split views. | Med | None -- editor-side only |

### Architecture: Running Game Screenshots

Same challenge as input injection -- the running game viewport is in a separate process. The companion autoload script (same one used for input injection) handles this:

1. Game-side: `get_viewport().get_texture().get_image()` -> Image
2. Image encoded as PNG bytes
3. Sent back to editor plugin via TCP/debugger channel
4. Editor encodes as base64 and returns via MCP tool response

**Important timing note:** Must call `await RenderingServer.frame_post_draw` before capturing to ensure the frame is fully rendered. This requires the game-side script to be async-aware.

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Screenshot with resize/compression | Return smaller images to reduce MCP message size. AI models work well with 512x512 or 1024x1024 screenshots. | Low | Image.resize() before encoding |
| Screenshot region capture | Capture specific area of viewport (e.g., just the UI panel, just the game world). | Med | Image.get_region(Rect2i) |
| Screenshot comparison | Return pixel-level diff between two screenshots. AI can detect visual changes. | High | Probably overkill for v1.1; defer |
| Editor screenshot (full window) | Capture entire editor window including dock panels. Uses Editor > Take Screenshot (Ctrl+F12) functionality. | Low | May not be programmatically accessible |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Video recording / frame sequence | Too much data for MCP transport, too complex. | Single-frame screenshots at key moments |
| Real-time viewport streaming | Continuous screenshot stream would overwhelm MCP protocol. | On-demand capture via tool call |
| Screenshot without editor overlays (3D gizmos) | No Godot API exists to capture editor viewport without gizmos. Open proposal #11793. | Document that editor viewport screenshots include gizmos |

### User Workflows

**"Take a screenshot of the running game"**: AI calls capture tool, gets base64 PNG, analyzes visually for layout issues or errors.

**"Show me what the 2D editor looks like"**: AI captures editor 2D viewport, returns image for visual context.

**"The UI is overlapping -- can you see?"**: AI takes game screenshot, identifies overlapping elements, adjusts Control anchors/sizes.

### Godot API Surface (verified)

Editor-side:
- `EditorInterface::get_editor_viewport_2d()` -> SubViewport
- `EditorInterface::get_editor_viewport_3d(idx)` -> SubViewport (idx 0-3)
- `SubViewport::get_texture()` -> ViewportTexture
- `ViewportTexture::get_image()` -> Image
- `Image::save_png_to_buffer()` -> PackedByteArray (for base64 encoding)
- `Image::resize(width, height, interpolation)` for downscaling

Game-side (in companion autoload):
- `get_viewport().get_texture().get_image()` -> Image
- Must await `RenderingServer.frame_post_draw` for clean capture
- Images saved at Viewport resolution, not window resolution (important for pixel art games)

**Confidence:** HIGH for editor viewport capture (direct API). MEDIUM for running game capture (depends on communication bridge design, same as input injection).

---

## Feature Dependencies (v1.1)

```
EXISTING v1.0 TOOLS
  |
  +-- UI System tools
  |     create_node (already works with Control types)
  |     set_node_property (already works with anchors, size_flags)
  |     NEW: get_ui_properties (Control-specific query)
  |     NEW: set_theme_override (dedicated theme override tool)
  |     NEW: build_ui_layout (compound tool, optional differentiator)
  |
  +-- Animation tools
  |     create_node (AnimationPlayer type)
  |     NEW: list_animations
  |     NEW: get_animation_info
  |     NEW: create_animation
  |     NEW: add_animation_track + insert_keyframe
  |     NEW: remove_animation_track / remove_keyframe
  |
  +-- Scene file management tools
  |     NEW: save_scene
  |     NEW: open_scene
  |     NEW: get_open_scenes
  |     NEW: get_current_scene_info
  |     NEW: create_new_scene
  |     NEW: instantiate_scene (PackedScene)
  |
  +-- Companion Autoload (shared infrastructure for input + screenshots)
  |     |
  |     +-- Input injection tools
  |     |     NEW: inject_key
  |     |     NEW: inject_mouse_click
  |     |     NEW: inject_mouse_move
  |     |     NEW: inject_action
  |     |
  |     +-- Screenshot capture tools
  |           NEW: capture_game_screenshot
  |           NEW: capture_editor_viewport
```

**Critical dependency:** Input injection and game screenshot capture share the same architectural requirement -- a communication bridge to the running game process. These MUST be designed and built together. The companion autoload script is the shared infrastructure.

---

## MVP Recommendation (v1.1)

### Phase 1 -- Scene File Management + UI Fundamentals (lowest risk, immediate value)

These features use only editor-side APIs (no running game communication needed):

1. **save_scene** -- EditorInterface::save_scene()
2. **open_scene** -- EditorInterface::open_scene_from_path()
3. **get_open_scenes** -- EditorInterface::get_open_scenes()
4. **get_current_scene_info** -- scene path, root type, unsaved status
5. **set_theme_override** -- add_theme_*_override for any Control
6. **get_ui_properties** -- Control-specific properties in scene tree queries

Rationale: Pure editor API, low risk, high value. Scene management alone fills a critical gap (AI currently cannot save its work).

### Phase 2 -- Animation System

7. **list_animations** -- query AnimationPlayer
8. **get_animation_info** -- detailed track/keyframe info
9. **create_animation** -- create Animation + add to library
10. **edit_animation_track** -- add/remove tracks, insert/remove keyframes

Rationale: Animation API is well-documented and stable. No running game dependency. Builds on Phase 1's ability to navigate scenes.

### Phase 3 -- Running Game Bridge (input + screenshots)

11. **Companion autoload infrastructure** -- mcp_runtime_helper.gd
12. **capture_game_screenshot** -- viewport capture via bridge
13. **capture_editor_viewport** -- editor 2D/3D viewport capture (no bridge needed)
14. **inject_key / inject_mouse_click / inject_action** -- input injection via bridge

Rationale: Highest complexity, highest reward. The companion autoload is shared infrastructure. Build screenshot capture first (easier to test, immediate visual feedback for AI), then layer input injection on top.

### Defer to v1.2+

- **UI layout builder compound tool** -- Nice to have, not essential when individual tools work
- **Input sequence/macro** -- Build after basic injection is proven
- **Screenshot comparison/diffing** -- Overkill for initial release
- **Scene instantiation (PackedScene)** -- Useful but not critical for MVP
- **Gamepad input simulation** -- Low demand signal
- **AnimationTree state machine editing** -- Too complex, too niche

---

## Competitive Gap Analysis

| Feature | Our v1.0 | GoPeak (95+) | MCP Pro (162) | GDAI (~30) | v1.1 Target |
|---------|----------|-------------|---------------|------------|-------------|
| Scene tree CRUD | YES | YES | YES | YES | YES |
| Script management | YES | YES | YES | YES | YES |
| UndoRedo integration | YES | Unknown | YES | NO | YES |
| Signal management | YES | YES | YES | NO | YES |
| Save/load scenes | NO | YES | YES | NO | **YES** |
| UI/Control tools | NO | Unknown | YES | NO | **YES** |
| Animation editing | NO | Unknown | YES | NO | **YES** |
| Input simulation | NO | YES | YES | NO | **YES** |
| Game screenshots | NO | YES | YES | YES | **YES** |
| Editor screenshots | NO | Unknown | Unknown | Unknown | **YES** |
| Zero dependencies | YES | NO (Node.js) | NO (Node.js) | NO (Python) | YES |

v1.1 closes all major feature gaps with competitors while maintaining the zero-dependency GDExtension advantage.

---

## Tool Count Projection

v1.0: 18 tools
v1.1 additions: ~14-16 new tools
v1.1 total: ~32-34 tools

This puts us solidly in the mid-tier (above GDAI's ~30, below GoPeak's 95+) but with the zero-dependency advantage that no competitor has.

---

## Sources

- [EditorInterface (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_editorinterface.html) -- scene management APIs
- [EditorInterface (Godot 4.4)](https://docs.godotengine.org/en/4.4/classes/class_editorinterface.html) -- updated API signatures
- [Animation class (Godot stable)](https://docs.godotengine.org/en/stable/classes/class_animation.html) -- track/keyframe APIs
- [AnimationLibrary (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_animationlibrary.html) -- library management
- [AnimationMixer (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_animationmixer.html) -- base class for AnimationPlayer
- [Control class (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_control.html) -- UI layout and theme APIs
- [Using Containers (Godot)](https://docs.godotengine.org/en/stable/tutorials/ui/gui_containers.html) -- container layout guide
- [Using InputEvent (Godot)](https://docs.godotengine.org/en/stable/tutorials/inputs/inputevent.html) -- input injection via parse_input_event
- [EditorDebuggerPlugin (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_editordebuggerplugin.html) -- editor-game communication
- [EngineDebugger (Godot)](https://docs.godotengine.org/en/stable/classes/class_enginedebugger.html) -- game-side debugger API
- [Easy screenshots (ShaggyDev)](https://shaggydev.com/2025/02/05/godot-screenshots/) -- viewport capture patterns
- [Expose editor viewports PR #68696](https://github.com/godotengine/godot/pull/68696) -- get_editor_viewport_2d/3d
- [close_scene proposal #8806](https://github.com/godotengine/godot-proposals/issues/8806) -- no close_scene API exists
- [GDExtension Debugger Tooling proposal #8777](https://github.com/godotengine/godot-proposals/issues/8777) -- GDExtension debugger limitations
- [Godot MCP Pro](https://godot-mcp.abyo.net/) -- 162 tools, autoload injection approach
- [GoPeak](https://had0yun.github.io/godot-mcp/) -- 95+ tools, input injection + screenshots
- [Godot MCP Pro dev blog](https://dev.to/y1uda/i-built-a-godot-mcp-server-because-existing-ones-couldnt-let-ai-test-my-game-47dl) -- build-test-fix loop motivation
- [EditorInterface source (editor_interface.h)](https://github.com/godotengine/godot/blob/master/editor/editor_interface.h) -- C++ method signatures
- [Animation source (animation.cpp)](https://github.com/godotengine/godot/blob/master/scene/resources/animation.cpp) -- track/key implementation
- [Godot Input source (input.cpp)](https://github.com/godotengine/godot/blob/master/core/input/input.cpp) -- InputEvent creation patterns
