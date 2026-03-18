# Domain Pitfalls

**Domain:** v1.1 Expansion -- UI System, Animation, Scene File I/O, Input Injection, Viewport Screenshots
**Project:** Godot MCP Meow (C++ GDExtension, godot-cpp v10+)
**Researched:** 2026-03-18
**Context:** Adding new tool categories to an existing 18-tool MCP server plugin. Known codebase patterns: IO thread + main thread queue, `MEOW_GODOT_MCP_GODOT_ENABLED` guards, `EditorInterface::get_singleton()` for editor access, `EditorUndoRedoManager` for mutations, `nlohmann::json` returns from standalone tool functions.

---

## Critical Pitfalls

Mistakes that cause crashes, data loss, or require fundamental rearchitecting.

---

### Pitfall 1: Viewport Screenshot Requires Cross-Process Communication (Game Runs in Separate Process)

**What goes wrong:** The most natural approach to "capture a game screenshot" is `get_viewport()->get_texture()->get_image()`. But in Godot 4, when you press Play, the game runs as a **separate OS process** from the editor. The GDExtension plugin runs inside the editor process. It has zero direct access to the running game's viewport, memory, or scene tree. Calling `get_viewport()` from the editor plugin returns the **editor's** viewport, not the game's.

**Why it happens:** Developers assume editor and game share the same process (they did in Godot 3's built-in editor testing). Godot 4 runs the game as a child process via `EditorInterface::play_main_scene()`. The editor and game communicate only through Godot's debugger protocol.

**Consequences:** Implementing the "obvious" approach gives you a screenshot of the Godot editor UI, not the running game. Or, if no game viewport is found, you get a null reference crash.

**Prevention:** There are three viable approaches, in order of recommendation:

1. **File-based capture (simplest, recommended for v1.1):** Inject a lightweight GDScript autoload into the running game that captures `get_viewport().get_texture().get_image().save_png("user://mcp_screenshot.png")` on command. The editor plugin reads the saved file. Communication trigger: write a command file that the game autoload watches, or use a fixed hotkey. This requires no debugger plugin infrastructure.

2. **EditorDebuggerPlugin (proper but complex):** Implement a custom `EditorDebuggerPlugin` in the editor that sends messages to the running game via `EngineDebugger`. The game-side autoload captures the viewport and sends the image data back via `EngineDebugger.send_message()`. This is architecturally clean but requires GDScript on both sides (editor debugger plugin + game autoload), which is a departure from the pure-C++ approach.

3. **Editor viewport only (limited but no game dependency):** Capture the editor's 2D/3D viewport instead. Use `EditorInterface::get_editor_viewport_3d()` or the editor's main viewport. This shows the scene as it appears in the editor, not the running game. Useful for "show me the current scene layout" but not "show me what the game looks like running."

**Detection:** If your screenshot tool returns an image of the Godot editor dock panels or a blank/incorrect image while the game is running, this pitfall is active.

**Confidence:** HIGH -- verified via Godot 4 architecture (game is separate process), confirmed by all existing Godot MCP implementations that handle screenshots (GoPeak, Godot MCP Pro, GDAI MCP all use game-side scripts).

**Phase relevance:** Viewport screenshot phase. Must be designed before implementation begins.

---

### Pitfall 2: `set_anchors_preset()` Is Silently Broken When Called from Code

**What goes wrong:** When programmatically creating or configuring UI `Control` nodes, calling `set_anchors_preset()` **does nothing** in most situations. The node appears at its default position regardless of the preset you specify. This affects all code paths -- GDScript, C#, and C++ GDExtension equally. The issue has been reported across Godot 4.0 through 4.6.

**Why it happens:** `set_anchors_preset()` only updates anchor values but does NOT update offset values. Since most presets depend on correct offsets to position the control, the visual result is unchanged. Additionally, if the control is inside a Container, anchor presets are completely ignored (the container controls positioning via size flags). Furthermore, calling presets in `_ready()` can fail because the layout system hasn't completed its first pass yet.

**Consequences:** AI creates UI layouts via MCP tools, but Controls appear at wrong positions. Users see broken layouts and lose trust in the tool. Debugging is difficult because the anchor property values look correct in the inspector -- the offsets are just wrong.

**Prevention:**
- **Always use `set_anchors_and_offsets_preset()` instead of `set_anchors_preset()`.** The combined method updates both anchors AND offsets together, which is what you actually need.
- **Set `custom_minimum_size` BEFORE applying presets.** Some presets (like Center) compute offsets from the node's minimum size.
- **Use `call_deferred()` for presets applied during node creation.** The layout system needs at least one frame to initialize.
- **Detect container parents and use size flags instead.** If the Control's parent is a Container subclass, anchor presets are non-operational. Set `size_flags_horizontal` and `size_flags_vertical` instead.
- **Document this in the MCP tool description** so the AI knows to use the correct method.

**Detection:** Controls are created successfully (no errors) but appear at position (0,0) or with incorrect sizing despite anchor preset being set.

**Confidence:** HIGH -- verified via Godot GitHub issues (#66651, #67161, #85185, #92487) and multiple forum reports through 2025-2026.

**Phase relevance:** UI system phase. Affects every UI layout tool.

---

### Pitfall 3: Animation Track NodePaths Are Relative to AnimationPlayer's Root -- Not the AnimationPlayer Itself

**What goes wrong:** When adding animation tracks programmatically via `Animation::add_track()` + `Animation::track_set_path()`, developers set the NodePath relative to the AnimationPlayer node. This is wrong. Tracks use paths relative to the AnimationPlayer's `root_node` property (which defaults to its parent node, typically the scene root). A path like `"."` points to the root node, not the AnimationPlayer.

**Why it happens:** The NodePath system in Animation is confusingly documented. Developers expect `track_set_path("Sprite2D:position")` to mean "find Sprite2D relative to the AnimationPlayer." But if the AnimationPlayer is at `/root/Main/AnimationPlayer` and `root_node` is `..` (default), then `"Sprite2D:position"` actually searches for `/root/Main/Sprite2D:position`.

**Consequences:** Tracks appear to be set correctly but don't animate anything at runtime. No error is printed -- the AnimationPlayer silently skips tracks with invalid paths. The AI creates what looks like a correct animation but it does nothing when played.

**Prevention:**
- **Always compute the relative path from the AnimationPlayer's root node.** Use `root_node.get_path_to(target_node)` to build the correct path dynamically.
- **Call `track_set_path()` BEFORE `track_insert_key()`.** Inserting keys without a path set causes `Condition '!data' is true` errors.
- **For method tracks, the key value must be a Dictionary** with `"method"` (String) and `"args"` (Array) keys. Passing anything else returns -1.
- **Clear animation caches after modifying tracks.** Call `AnimationPlayer::clear_caches()` to force the player to re-resolve node references.
- **Handle the AnimationLibrary indirection.** In Godot 4, animations must be added through an `AnimationLibrary`. The default library (empty name `""`) addresses animations as just `"anim_name"`, but named libraries use `"lib_name/anim_name"`.

**Detection:** Animations play but nothing moves. Or `track_insert_key` returns -1. Or AnimationPlayer prints warnings about unresolved paths in the Output panel.

**Confidence:** HIGH -- verified via Godot docs (Animation class), GitHub issue #17313, and multiple forum posts about programmatic track creation.

**Phase relevance:** Animation tools phase. Fundamental to all animation editing tools.

---

### Pitfall 4: Input Injection Only Works on the Running Game Process (Separate from Editor)

**What goes wrong:** `Input::get_singleton()->parse_input_event()` called from the editor GDExtension injects input into the **editor's** input pipeline, not the running game. This means injected keyboard events affect the editor UI (opening menus, triggering editor shortcuts) and injected mouse events click on editor panels. The running game, being a separate process, receives none of these events.

**Why it happens:** Same root cause as the viewport screenshot pitfall -- the game runs as a separate OS process. `Input` is a per-process singleton. The editor's `Input` singleton and the game's `Input` singleton are different objects in different processes.

**Consequences:** "Press spacebar" ends up pressing spacebar in the Godot editor, potentially triggering scene playback, toggling inspector checkboxes, or other editor actions. The running game is unaffected.

**Prevention:**
- **OS-level input injection:** Use platform-native APIs (`SendInput` on Windows, `XTest` / `xdg` on Linux, `CGEvent` on macOS) to inject input at the OS level, targeting the game's window. This is the most reliable approach but requires platform-specific code.
- **Game-side autoload script:** Similar to the screenshot approach, inject a GDScript autoload into the game that listens for commands (via file, UDP, or a simple TCP socket) and calls `Input.parse_input_event()` from within the game process.
- **Viewport.push_input() for editor-only testing:** If the goal is to test input handling in the editor viewport (tool scripts), use `Viewport::push_input()` on the specific viewport. But this doesn't help with running game input.

**Detection:** Editor UI responds to injected input instead of the game. Or game window doesn't respond at all to injected events.

**Confidence:** HIGH -- verified via Godot 4 process architecture and `Input` singleton scope.

**Phase relevance:** Input injection phase. Architecture must be decided before implementation.

---

## Moderate Pitfalls

---

### Pitfall 5: Theme Overrides Disappear in Inspector for GDExtension-Defined Control Subclasses

**What goes wrong:** If you create a custom Control subclass via GDExtension (e.g., a specialized Panel or VBoxContainer), the "Theme Overrides" section disappears from the Godot editor inspector. Users cannot visually customize the node's appearance through the inspector. The `add_theme_*_override()` methods still work in code, but the inspector-driven workflow is broken.

**Why it happens:** Godot's editor generates the "Theme Overrides" inspector section by querying the class's expected theme items via internal ClassDB metadata. GDExtension classes don't register this metadata, so the editor shows nothing. This is a known Godot engine limitation (issue #84550), not a bug in godot-cpp.

**Consequences:** For a tool that creates Control nodes, this doesn't directly matter -- the MCP tool creates nodes of built-in Godot types (Button, Label, Panel), not custom GDExtension types. However, if the MCP tool tries to apply theme overrides, it needs to know that the API path is `add_theme_*_override()` methods, not Theme resource assignment, for per-node customization.

**Prevention:**
- **Use `add_theme_color_override()`, `add_theme_font_override()`, etc.** for per-node styling. These methods work correctly from C++ regardless of the inspector issue.
- **Do NOT modify `ThemeDB::get_singleton()->get_default_theme()` from GDExtension constructor code.** This causes severe editor loading slowdowns (issue #1332 on godot-cpp).
- **Apply theme overrides in `_enter_tree()` or after the node is in the tree**, not in the constructor.
- **For the MCP tool API, expose theme overrides as explicit parameters** rather than relying on Theme resource creation. E.g., `set_theme_override("font_color", "#ff0000")` rather than creating a full Theme resource.

**Detection:** Editor inspector missing "Theme Overrides" section on GDExtension nodes. Or editor loading takes 10+ seconds after modifying the default theme.

**Confidence:** HIGH -- verified via Godot issue #84550 and godot-cpp issue #1332.

**Phase relevance:** UI system tools phase. Affects theme/style management tools.

---

### Pitfall 6: `EditorInterface::save_scene()` Silently Fails Without Proper Scene State

**What goes wrong:** `EditorInterface::get_singleton()->save_scene()` saves the currently edited scene. But it can silently produce no result (or save an empty file) if:
1. No scene is currently open (`get_edited_scene_root()` returns null).
2. The scene has never been saved before (no path assigned).
3. The scene was modified by code that didn't go through UndoRedo, so the editor doesn't know it's "dirty."

Additionally, there is no `close_scene()` method in the `EditorInterface` API. You cannot programmatically close a scene tab. There's only `open_scene_from_path()` for switching scenes.

**Why it happens:** The editor's save logic checks internal "modified" flags that are only set when changes go through the UndoRedo system or inspector. Direct property modifications via code may not trigger the "scene needs saving" state. The missing `close_scene()` API is a recognized gap (proposal #8806).

**Consequences:** AI says "scene saved" but the .tscn file on disk doesn't reflect the changes. Or `save_scene_as()` fails because the path doesn't end with `.tscn`/`.scn`. Scene file management tools may appear to work but data is lost on editor restart.

**Prevention:**
- **Always validate `get_edited_scene_root()` is not null** before save operations.
- **Use `save_scene()` (no args) for the current scene** and verify the return value (Error enum).
- **For "save as" functionality, use `EditorInterface::save_scene_as(path)`** and ensure the path includes the `.tscn` extension.
- **For opening scenes, use `EditorInterface::open_scene_from_path()`** and handle the `scene_changed` signal. Note: this signal has a bug where it doesn't fire when replacing an empty scene (Godot issue #97427).
- **Mark the scene as modified after code changes** by calling a trivial UndoRedo action if needed, to ensure the editor knows there are unsaved changes.
- **`reload_scene_from_path(path)` exists** for force-reloading a scene that was modified on disk externally.

**Detection:** Scene "saved" but changes missing after editor restart. `scene_changed` signal not firing after `open_scene_from_path()`.

**Confidence:** MEDIUM -- verified via Godot proposals (#8806) and forum reports. The exact behavior depends on how changes were made.

**Phase relevance:** Scene file management phase.

---

### Pitfall 7: Viewport Screenshot Timing -- Black/Empty Image If Captured Before Rendering Completes

**What goes wrong:** Calling `get_viewport()->get_texture()->get_image()` returns a completely black or empty image if called before the rendering pipeline has finished drawing the current frame. In GDScript, this is solved with `await RenderingServer.frame_post_draw`. In C++ GDExtension, there is no `await` equivalent.

**Why it happens:** The viewport texture represents the last **completed** frame. If you capture during the frame's processing (before rendering), you get the previous frame's data or a blank buffer. The RenderingServer processes draws asynchronously, so the texture data lags behind scene changes.

**Consequences:** Screenshot tools return black images intermittently, especially on the first capture or immediately after scene changes. Users report "screenshot doesn't work" for what is actually a timing issue.

**Prevention:**
- **Connect a one-shot callback to `RenderingServer::frame_post_draw`:**
  ```cpp
  RenderingServer::get_singleton()->connect(
      "frame_post_draw",
      Callable(this, "_on_frame_drawn"),
      CONNECT_ONE_SHOT
  );
  ```
  Perform the actual capture in `_on_frame_drawn`.
- **For editor viewport captures, wait at least 2 frames** after any scene modification before capturing. Use a frame counter in `_process()`.
- **For game viewport captures (file-based approach), the game-side script should use `await RenderingServer.frame_post_draw`** before calling `get_texture().get_image()`.
- **Always validate the captured Image:** Check `img->is_empty()` and `img->get_width() > 0` before processing.

**Detection:** Screenshots are occasionally all-black or all-transparent. The issue is intermittent and timing-dependent.

**Confidence:** HIGH -- well-documented issue across Godot 3 and 4, with the official workaround being `await RenderingServer.frame_post_draw`.

**Phase relevance:** Viewport screenshot phase.

---

### Pitfall 8: `Input.parse_input_event()` Stale Action States and Viewport Scaling Bugs

**What goes wrong:** Even when injecting input into the correct process (the game), `parse_input_event()` has several known bugs:

1. **Stale action states:** `is_action_just_pressed()` only returns true if the previous state was "not pressed." If you inject a press event without a corresponding release event, subsequent press events for the same action are silently ignored. The action appears "stuck."
2. **Viewport scaling breaks mouse coordinates:** When the game uses viewport scaling modes other than "Keep Height" or "Expand," the mouse position in injected events is applied at the **window** coordinate space, not the **viewport** coordinate space. UI elements at scaled positions don't receive the click.
3. **Headless mode:** `parse_input_event()` does nothing in `--headless` mode, breaking CI-based automated testing.
4. **`use_accumulated_input = false` regression:** In Godot 4.4+, setting `Input.use_accumulated_input = false` causes `InputEventMouseButton` from `parse_input_event()` to be completely ignored.

**Prevention:**
- **Always inject both press AND release events.** For keyboard: send `InputEventKey` with `pressed=true`, then after a frame delay, send another with `pressed=false`. For mouse buttons: same pattern.
- **Account for viewport scaling in mouse coordinates.** Transform coordinates using the viewport's `get_final_transform()` or `get_screen_transform()` inverse.
- **Use `Input.action_press()` / `Input.action_release()` for action-based input** instead of constructing `InputEventAction` objects. These bypass some of the stale-state issues.
- **Call `Input.flush_buffered_events()` after `parse_input_event()`** for immediate processing.
- **Document that input injection requires a running game** and does not work headless.

**Detection:** Input appears to work once but subsequent identical inputs are ignored. Mouse clicks land at wrong positions in scaled viewports.

**Confidence:** HIGH -- verified via Godot issues #87692, #85931, #96808, #73557, and forum reports.

**Phase relevance:** Input injection phase.

---

### Pitfall 9: Animation System Requires AnimationLibrary Layer in Godot 4

**What goes wrong:** Developers try to add animations directly to an AnimationPlayer using the Godot 3 pattern (`AnimationPlayer::add_animation("name", anim)`). In Godot 4, this method no longer exists on AnimationPlayer. Animations must be added through an `AnimationLibrary` first, and the library is then registered with the AnimationPlayer.

**Why it happens:** Godot 4 refactored the animation system to support AnimationLibraries for better organization. Many tutorials and Stack Overflow answers still reference the Godot 3 API. The AnimationPlayer class itself inherits from AnimationMixer in Godot 4, which changed the API surface.

**Consequences:** Compilation errors if using old API. Or, if using `call()` reflection to bypass type checking, runtime errors about missing methods.

**Prevention:**
- **Create animations through AnimationLibrary:**
  ```cpp
  Ref<Animation> anim;
  anim.instantiate();
  // ... add tracks and keys to anim ...

  Ref<AnimationLibrary> lib;
  lib.instantiate();
  lib->add_animation("walk", anim);

  animation_player->add_animation_library("", lib); // empty name = default library
  ```
- **The default library (empty string name `""`) is special:** Animations in it are referenced by just their name (e.g., `"walk"`). Named libraries use `"lib_name/walk"`.
- **Check if a library already exists before adding:** `animation_player->has_animation_library("")` to avoid overwriting.
- **For the MCP tool, provide a `library_name` parameter** defaulting to `""` to give AI control over organization.

**Detection:** Method not found errors for `add_animation()`. Or animations added but not appearing in the AnimationPlayer panel.

**Confidence:** HIGH -- verified via Godot 4 API docs and AnimationPlayer/AnimationMixer class hierarchy.

**Phase relevance:** Animation tools phase.

---

### Pitfall 10: Image-to-Base64 Pipeline for MCP Response Payload

**What goes wrong:** MCP tool results are JSON text. Returning a screenshot requires encoding the image as base64 within the JSON response. The pipeline (`Viewport -> Texture2D -> Image -> PackedByteArray -> base64 String`) has several failure points:

1. **`save_png_to_buffer()` can crash on certain image formats.** If the image has an unsupported format (like compressed PVRTC from imported textures), calling `save_png_to_buffer()` causes a buffer overflow crash (Godot issue #50787). Decompress first.
2. **Alpha channel can be destroyed during PNG save.** Godot's `detect_alpha()` method has bugs that cause it to strip the alpha channel in certain cases (issue #108535).
3. **`Marshalls::get_singleton()->raw_to_base64()` expects a `PackedByteArray`**, not a raw `uint8_t*`. Must construct the PackedByteArray properly.
4. **Large images produce large JSON responses.** A 1920x1080 PNG is ~2-4MB, which becomes ~3-5MB in base64. This can exceed MCP client buffer limits or cause timeouts.

**Prevention:**
- **Always decompress images before saving to buffer:** `img->decompress()` if `img->is_compressed()`.
- **Use JPEG for screenshots (smaller payload):** `img->save_jpg_to_buffer(0.8)` produces files ~10x smaller than PNG.
- **Resize before encoding:** `img->resize(960, 540, Image::INTERPOLATE_BILINEAR)` to halve dimensions and quarter file size.
- **Construct PackedByteArray correctly:**
  ```cpp
  PackedByteArray png_buf = img->save_png_to_buffer();
  String base64 = Marshalls::get_singleton()->raw_to_base64(png_buf);
  ```
- **Return as MCP image content type** if the client supports it, rather than embedding base64 in a text response:
  ```json
  {"type": "image", "data": "<base64>", "mimeType": "image/png"}
  ```
- **Set a maximum resolution** and downscale if the viewport is larger.

**Detection:** Crashes when saving screenshots of imported/compressed textures. JSON responses that are megabytes in size. Base64 string is empty or garbled.

**Confidence:** MEDIUM -- verified PNG crash via Godot issue #50787, alpha issue via #108535. Base64 pipeline verified via Marshalls docs. Payload size is an architectural concern.

**Phase relevance:** Viewport screenshot phase.

---

## Minor Pitfalls

---

### Pitfall 11: Container Nodes Ignore Direct Position/Size Setting

**What goes wrong:** Setting `position`, `size`, `anchor_*`, or `offset_*` properties on a Control node that is a child of a Container (VBoxContainer, HBoxContainer, GridContainer, etc.) has no effect. The Container overrides all positioning every frame.

**Prevention:**
- **Detect parent type before setting layout properties.** If parent is a Container subclass, use `size_flags_*` and `custom_minimum_size` instead.
- **Document this in the MCP tool schema description** so the AI knows to use different properties for container children.
- **Consider a "get_layout_context" helper** that returns whether the node is in a container or free-positioned.

**Phase relevance:** UI system phase.

---

### Pitfall 12: UndoRedo for Complex Multi-Step Operations (Animation + UI)

**What goes wrong:** Animation editing and UI construction involve multiple interdependent steps (create node, add library, add animation, add tracks, insert keys, set properties). If each step is a separate UndoRedo action, pressing Ctrl+Z undoes only the last step, leaving the scene in an inconsistent state (e.g., an AnimationPlayer with a library but no tracks, or a Container with some children but broken layout).

**Prevention:**
- **Wrap multi-step operations in a single `create_action()` / `commit_action()` block.** This is already the pattern used for `create_node` with properties in v1.0.
- **Order operations correctly within the action:** `add_child` before `set_owner` before property changes.
- **For animation tools, consider grouping "create animation with tracks" as one action** rather than separate "create animation" + "add track" + "add key" tools.

**Phase relevance:** All new tool phases.

---

### Pitfall 13: No `await` in C++ GDExtension -- Callback-Based Async Patterns Required

**What goes wrong:** Many Godot operations require waiting for a signal (e.g., `RenderingServer.frame_post_draw` for screenshots, `SceneTree.process_frame` for deferred operations). GDScript uses `await signal` for linear code flow. C++ GDExtension has no `await` equivalent -- you must split logic into a request function and a callback.

**Prevention:**
- **Use `connect()` with `CONNECT_ONE_SHOT` for single-wait operations:**
  ```cpp
  RenderingServer::get_singleton()->connect(
      "frame_post_draw", Callable(this, "_capture_frame"), CONNECT_ONE_SHOT
  );
  ```
- **This interacts with the existing IO thread / main thread architecture:** The MCP request arrives on the IO thread, is queued to main thread, but the main thread callback connects a signal and returns. The response can't be sent until the signal fires. **This breaks the synchronous request-response model** of the current `poll()` implementation.
- **Option 1: Block for one frame.** Since `poll()` is called every `_process()`, you can set a flag and respond on the next `poll()` call. Simple but adds one-frame latency.
- **Option 2: Extend the pending response mechanism** to support deferred responses that are fulfilled by signal callbacks rather than immediate returns.

**Detection:** If a tool function needs to return data that depends on a signal, and it tries to return immediately, the data will be stale or null.

**Confidence:** HIGH -- verified via Godot forum discussions on GDExtension async patterns and the project's own synchronous poll() architecture.

**Phase relevance:** Viewport screenshot phase primarily. May affect other tools that need post-frame data.

---

### Pitfall 14: `scene_changed` Signal Not Fired When Replacing Empty Scene

**What goes wrong:** `EditorPlugin::scene_changed` is not emitted when `EditorInterface::open_scene_from_path()` opens a scene that replaces an empty (unsaved, untitled) scene. The plugin never learns that the scene changed.

**Prevention:**
- **Don't rely solely on `scene_changed` for tracking the current scene.** Also check `get_edited_scene_root()` after `open_scene_from_path()` returns.
- **Add a frame delay after `open_scene_from_path()`** before querying the new scene root, as the scene switch may not be immediate.

**Detection:** Scene management tools report "no scene open" after opening a scene from empty state.

**Confidence:** MEDIUM -- reported in Godot issue #97427 for Godot 4.3/4.4.

**Phase relevance:** Scene file management phase.

---

### Pitfall 15: PackedByteArray vs Array Return Type Confusion in godot-cpp

**What goes wrong:** Some godot-cpp methods return `Array` where you expect `PackedByteArray`. The v1.0 retrospective already documented that `StreamPeer::get_data()` returns `Array` (not `PackedByteArray`). Similar surprises exist in:
- `Image::get_data()` -- returns `PackedByteArray` (correct)
- `StreamPeerTCP::get_partial_data()` -- returns `Array` with error code and data
- `FileAccess::get_buffer()` -- returns `PackedByteArray` (correct)

**Prevention:**
- **Always check the godot-cpp header files** for actual return types rather than assuming they match the GDScript docs.
- **Use explicit type conversions** and validate array sizes before accessing elements.
- **The v1.0 pattern in `mcp_server.cpp` (lines 142-147) is correct:** `Array get_result = client_peer->get_partial_data(available)` then extracting `PackedByteArray data = get_result[1]`.

**Phase relevance:** All phases that handle binary data (screenshots, file I/O).

---

### Pitfall 16: UI Tool Proliferation -- Too Many Tools Degrades AI Performance

**What goes wrong:** Adding separate MCP tools for every UI operation (set_anchor, set_margin, set_size_flags, set_theme_color_override, set_theme_font_override, set_theme_stylebox_override, etc.) creates a tool explosion. AI models perform worse when they have 50+ tools to choose from -- they confuse similar tools, misremember parameter names, and waste tokens on tool selection.

**Prevention:**
- **Group related operations into composite tools.** For example, a single `configure_control_layout` tool that accepts anchor preset, margins, size flags, and minimum size as optional parameters.
- **Use the existing `set_node_property` for simple cases.** Most Control properties (anchors, offsets, size flags) are regular node properties that can be set via the existing v1.0 `set_node_property` tool.
- **Reserve new tools for operations that truly need custom logic** (e.g., `set_anchors_and_offsets_preset` which is a method call, not a property set).
- **Consider a `batch_set_properties` tool** for setting multiple properties at once, reducing round-trips.

**Phase relevance:** All v1.1 phases. Tool API design decision.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| UI System (Control nodes) | Pitfall 2 (`set_anchors_preset` broken), Pitfall 11 (containers override layout), Pitfall 5 (theme overrides missing) | Use `set_anchors_and_offsets_preset()`, detect container parents, use `add_theme_*_override()` methods |
| Animation editing | Pitfall 3 (NodePath relative to root), Pitfall 9 (AnimationLibrary layer), Pitfall 12 (UndoRedo grouping) | Compute paths from root node, always create through AnimationLibrary, wrap multi-step operations in single action |
| Scene file management | Pitfall 6 (`save_scene` silent failures), Pitfall 14 (`scene_changed` signal bug) | Validate scene root before save, don't rely solely on `scene_changed`, use frame delays after open |
| Input injection | Pitfall 4 (separate process), Pitfall 8 (stale states, viewport scaling) | OS-level injection or game-side autoload, always send press+release pairs, account for viewport transforms |
| Viewport screenshots | Pitfall 1 (separate process), Pitfall 7 (timing/black image), Pitfall 10 (image pipeline), Pitfall 13 (no await in C++) | File-based capture via game autoload, wait for `frame_post_draw`, decompress+resize+JPEG, extend poll() for deferred responses |
| General (all tools) | Pitfall 16 (tool proliferation), Pitfall 15 (PackedByteArray confusion) | Composite tools, check godot-cpp headers for actual types |

---

## Integration with Existing Architecture

The v1.0 architecture has specific implications for v1.1 pitfalls:

### Current Architecture Constraint: Synchronous poll()

The existing `MCPServer::poll()` processes requests synchronously on the main thread and immediately enqueues responses. Tools that need async operations (viewport capture after `frame_post_draw`, file writes that need a frame delay) cannot return immediately. **This is the single biggest architectural challenge for v1.1.**

**Options:**
1. **One-frame-delayed responses:** Add a "pending async response" queue. `poll()` checks for completed async operations alongside new requests. Screenshot tool sets a flag, connects `frame_post_draw`, captures on next frame, and enqueues response in the following `poll()`.
2. **Blocking with frame wait:** Use the existing `std::promise`/`std::future` pattern but have the main thread fulfill the promise from a signal callback rather than immediately.
3. **Keep it simple:** For screenshot, just capture the current frame's texture (which is the previous frame's render). Accept the one-frame-stale data. For most use cases, this is sufficient.

### Existing Pattern: All Godot APIs on Main Thread

All v1.0 tools follow the pattern: IO thread queues request -> main thread executes -> response queued back. V1.1 tools MUST follow the same pattern. UI manipulation, animation editing, scene save/load -- all must execute on the main thread via `handle_request()`.

### Existing Pattern: MEOW_GODOT_MCP_GODOT_ENABLED Guard

New tool files (ui_tools.cpp, animation_tools.cpp, etc.) must use the same `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED` pattern for Godot-dependent code, keeping the files unit-testable.

---

## Sources

### Godot Engine Issues
- [Theme Overrides missing for GDExtension Control nodes -- Godot #84550](https://github.com/godotengine/godot/issues/84550)
- [Control.set_anchors_preset not working as expected -- Godot #66651](https://github.com/godotengine/godot/issues/66651)
- [set_anchors_preset ignored on instanced nodes -- Godot #92487](https://github.com/godotengine/godot/issues/92487)
- [Setting AnchorPreset in _Ready has no effect -- Godot #67161](https://github.com/godotengine/godot/issues/67161)
- [Layout Mode not updating programmatically -- Godot #85185](https://github.com/godotengine/godot/issues/85185)
- [Setting anchors via code incorrect with containers -- Godot #88788](https://github.com/godotengine/godot/issues/88788)
- [Adding tracks/keys to Animation via code causes errors -- Godot #17313](https://github.com/godotengine/godot/issues/17313)
- [AnimationPlayer track order bug -- Godot #108461](https://github.com/godotengine/godot/issues/108461)
- [Input.parse_input_event regression 4.2.1 -- Godot #87692](https://github.com/godotengine/godot/issues/87692)
- [Input.parse_input_event mouse coordinates broken with viewport scaling -- Godot #85931](https://github.com/godotengine/godot/issues/85931)
- [Mouse enter/exit signals trigger falsely with scaled viewport -- Godot #96808](https://github.com/godotengine/godot/issues/96808)
- [Input.parse_input_event does not work headless -- Godot #73557](https://github.com/godotengine/godot/issues/73557)
- [No close_scene API -- Godot proposals #8806](https://github.com/godotengine/godot-proposals/issues/8806)
- [scene_changed signal not fired for empty scene -- Godot #97427](https://github.com/godotengine/godot/issues/97427)
- [Image save_png_to_buffer crash on PVRTC -- Godot #50787](https://github.com/godotengine/godot/issues/50787)
- [Alpha channel destroyed on PNG save -- Godot #108535](https://github.com/godotengine/godot/issues/108535)
- [Sub-Viewport capture timing issue -- Godot #28916](https://github.com/godotengine/godot/issues/28916)

### godot-cpp Issues
- [Adding to default Theme slow editor load -- godot-cpp #1332](https://github.com/godotengine/godot-cpp/issues/1332)
- [Theme Overrides missing for GDExtension -- godot-rust/gdext #213](https://github.com/godot-rust/gdext/issues/213)

### Godot Documentation
- [Animation class API](https://docs.godotengine.org/en/stable/classes/class_animation.html)
- [Animation track types](https://docs.godotengine.org/en/stable/tutorials/animation/animation_track_types.html)
- [AnimationPlayer class API](https://docs.godotengine.org/en/stable/classes/class_animationplayer.html)
- [Size and anchors](https://docs.godotengine.org/en/stable/tutorials/ui/size_and_anchors.html)
- [Using InputEvent](https://docs.godotengine.org/en/stable/tutorials/inputs/inputevent.html)
- [RenderingServer class API](https://docs.godotengine.org/en/stable/classes/class_renderingserver.html)
- [EditorDebuggerPlugin class API](https://docs.godotengine.org/en/4.4/classes/class_editordebuggerplugin.html)
- [Marshalls class API (base64)](https://docs.godotengine.org/en/stable/classes/class_marshalls.html)
- [Image class API](https://docs.godotengine.org/en/stable/classes/class_image.html)

### Community / Forum
- [GDExtension C++ await/async patterns](https://forum.godotengine.org/t/how-would-i-wait-a-frame-gdextension-cpp/78891)
- [How to await signal in GDExtension](https://forum.godotengine.org/t/how-to-await-signal-in-gdextension/80126)
- [set_anchors_preset via GDScript issue](https://forum.godotengine.org/t/setting-control-node-anchor-presets-via-gdscript-issue/112097)
- [parse_input_event and is_action_just_pressed](https://forum.godotengine.org/t/how-to-ensure-parse-input-event-triggers-is-action-just-pressed/98363)
- [Programmatic Animation creation](https://forum.godotengine.org/t/programmatically-create-animationplayer-animation-with-a-property-track-and-a-frame-property/18040)
- [Editor-game communication discussion -- Godot proposals #10994](https://github.com/godotengine/godot-proposals/discussions/10994)

### Existing Godot MCP Implementations (Architecture Reference)
- [GoPeak (HaD0Yun/godot-mcp)](https://github.com/HaD0Yun/godot-mcp) -- 95+ tools with screenshot/input support
- [Godot MCP Pro](https://godot-mcp.abyo.net/) -- 162 tools, WebSocket, runtime analysis suite
- [GDAI MCP Plugin](https://github.com/3ddelano/gdai-mcp-plugin-godot) -- Auto screenshot feature
- [Calinou/godot-editor-screenshooter](https://github.com/Calinou/godot-editor-screenshooter) -- Editor viewport screenshot addon

### Screenshots / Capture
- [Easy in-engine screenshots (The Shaggy Dev, Feb 2025)](https://shaggydev.com/2025/02/05/godot-screenshots/)
- [Godot demo: screen capture](https://github.com/godotengine/godot-demo-projects/blob/master/viewport/screen_capture/README.md)
