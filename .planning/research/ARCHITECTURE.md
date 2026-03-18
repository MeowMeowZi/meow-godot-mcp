# Architecture Patterns: v1.1 Integration

**Domain:** Godot GDExtension MCP Server Plugin -- v1.1 UI & Editor Expansion
**Researched:** 2026-03-18
**Scope:** How new features integrate with the existing v1.0 architecture

## Existing Architecture Summary

The v1.0 codebase follows a clear pattern:

```
AI Client <--stdio--> Bridge (~50KB) <--TCP--> GDExtension (MCPServer)
                                                    |
                                              MCPPlugin (EditorPlugin)
                                                    |
                                              MCPServer (plain C++)
                                               /       \
                                    IO thread    Main thread (poll)
                                   (TCP r/w)     (Godot API calls)
                                                    |
                                            Tool modules (free functions)
                                            - scene_tools.h/.cpp
                                            - scene_mutation.h/.cpp
                                            - script_tools.h/.cpp
                                            - project_tools.h/.cpp
                                            - runtime_tools.h/.cpp
                                            - signal_tools.h/.cpp
```

**Key patterns in existing code:**

1. **Tool modules are free functions** -- not classes. Each module is a `.h`/`.cpp` pair exporting functions like `nlohmann::json get_scene_tree(...)`.
2. **MEOW_GODOT_MCP_GODOT_ENABLED ifdef** guards Godot-dependent code, allowing headers to be included in unit tests.
3. **Tool registration** is a static vector in `mcp_tool_registry.cpp` -- ToolDef structs with name, description, input_schema, min_version.
4. **Tool dispatch** is an if-else chain in `MCPServer::handle_request()` inside `mcp_server.cpp`.
5. **All tool calls execute on the main thread** via `MCPServer::poll()` called from `MCPPlugin::_process()`.
6. **Tool results** are always `nlohmann::json` objects returned as `type: "text"` MCP content blocks via `mcp::create_tool_result()`.
7. **UndoRedo** is passed as a raw pointer from MCPServer to tool functions that need mutation support.

## New Modules Required

### 1. `ui_tools.h` / `ui_tools.cpp` -- NEW MODULE

**Responsibility:** Control node hierarchy queries, theme/stylebox management, container layout operations.

**Why separate module:** UI (Control) nodes have a fundamentally different property surface than Node2D/Node3D. They have anchors, offsets, size flags, theme overrides, layout presets -- concepts that do not exist for spatial nodes. Grouping these together is the clean boundary.

**Functions:**

```cpp
#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// Query Control-specific properties for a node (anchors, offsets, size_flags, theme overrides)
nlohmann::json get_control_properties(const std::string& node_path);

// Set layout preset on a Control node (PRESET_TOP_LEFT, PRESET_FULL_RECT, etc.)
nlohmann::json set_layout_preset(const std::string& node_path, const std::string& preset,
                                  godot::EditorUndoRedoManager* undo_redo);

// Set/remove theme override on a Control node
// override_type: "color" | "constant" | "font" | "font_size" | "stylebox" | "icon"
nlohmann::json set_theme_override(const std::string& node_path, const std::string& override_type,
                                   const std::string& name, const std::string& value,
                                   godot::EditorUndoRedoManager* undo_redo);

nlohmann::json remove_theme_override(const std::string& node_path, const std::string& override_type,
                                      const std::string& name,
                                      godot::EditorUndoRedoManager* undo_redo);

#endif
```

**Godot APIs used:**
- `Control::set_anchors_preset()`, `Control::set_offsets_preset()`, `Control::set_anchors_and_offsets_preset()`
- `Control::set_anchor()`, `Control::get_anchor()`, `Control::set_offset()`, `Control::get_offset()`
- `Control::set_h_size_flags()`, `Control::set_v_size_flags()`
- `Control::add_theme_color_override()`, `Control::add_theme_stylebox_override()`, etc.
- `Control::get_theme_stylebox()`, `Control::has_theme_stylebox_override()`
- `Control::set_custom_minimum_size()`

**Confidence:** HIGH -- all APIs confirmed present in godot-cpp headers.

### 2. `animation_tools.h` / `animation_tools.cpp` -- NEW MODULE

**Responsibility:** AnimationPlayer/AnimationLibrary management, track editing, keyframe operations.

**Why separate module:** Animation is a complex subsystem with its own resource hierarchy (AnimationPlayer -> AnimationLibrary -> Animation -> Track -> Key). It warrants isolation to contain the complexity.

**Functions:**

```cpp
#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// List animations on an AnimationPlayer node
nlohmann::json get_animations(const std::string& node_path);

// Get detailed animation info including tracks and keyframes
nlohmann::json get_animation_info(const std::string& node_path, const std::string& animation_name);

// Create a new animation on an AnimationPlayer
nlohmann::json create_animation(const std::string& node_path, const std::string& animation_name,
                                 float length, const std::string& loop_mode);

// Add a track to an animation
nlohmann::json add_track(const std::string& node_path, const std::string& animation_name,
                          const std::string& track_type, const std::string& track_path);

// Insert a keyframe into a track
nlohmann::json insert_keyframe(const std::string& node_path, const std::string& animation_name,
                                int track_index, float time, const std::string& value);

// Remove a keyframe from a track
nlohmann::json remove_keyframe(const std::string& node_path, const std::string& animation_name,
                                int track_index, int key_index);

#endif
```

**Godot APIs used:**
- `AnimationMixer::get_animation_library_list()`, `AnimationMixer::get_animation()`, `AnimationMixer::has_animation()`
- `AnimationLibrary::add_animation()`, `AnimationLibrary::get_animation()`, `AnimationLibrary::get_animation_list()`
- `Animation::add_track()`, `Animation::get_track_count()`, `Animation::track_get_type()`, `Animation::track_get_path()`
- `Animation::track_insert_key()`, `Animation::track_get_key_count()`, `Animation::track_get_key_value()`, `Animation::track_get_key_time()`
- `Animation::track_remove_key()`, `Animation::set_length()`, `Animation::set_loop_mode()`
- `AnimationPlayer::get_current_animation()`, `AnimationPlayer::is_playing()`

**Confidence:** HIGH -- all APIs confirmed present in godot-cpp headers. The Animation class has a rich API surface for programmatic track/keyframe manipulation.

### 3. `editor_tools.h` / `editor_tools.cpp` -- NEW MODULE

**Responsibility:** Scene file management (save/load/switch), viewport screenshots, and input injection to running game.

**Why one module (not three):** These are all "editor-level operations" that use `EditorInterface` and `Input` singletons rather than node-level APIs. They share the same dependency surface and are conceptually "editor control" features. However, screenshot capture has binary data handling that is unique. If the module grows beyond ~400 lines, splitting `screenshot_tools` out is warranted.

**Functions:**

```cpp
#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// --- Scene File Management ---

// Save the currently edited scene
nlohmann::json save_scene();

// Save the currently edited scene to a new path
nlohmann::json save_scene_as(const std::string& path);

// Open a scene file in the editor
nlohmann::json open_scene(const std::string& scene_filepath);

// List currently open scenes in the editor
nlohmann::json get_open_scenes();

// --- Viewport Screenshot ---

// Capture the editor viewport (2D or 3D) and return base64-encoded PNG
// Returns MCP ImageContent-compatible JSON, NOT text content
nlohmann::json capture_viewport(const std::string& viewport_type, int max_width = 0);

// --- Input Injection ---

// Inject a keyboard input event into the running game
nlohmann::json inject_key_input(const std::string& key, bool pressed, bool shift, bool ctrl, bool alt);

// Inject a mouse button event
nlohmann::json inject_mouse_button(const std::string& button, bool pressed, float x, float y);

// Inject a mouse motion event
nlohmann::json inject_mouse_motion(float x, float y, float rel_x, float rel_y);

#endif
```

**Godot APIs used:**
- Scene: `EditorInterface::save_scene()`, `EditorInterface::save_scene_as()`, `EditorInterface::open_scene_from_path()`, `EditorInterface::get_open_scenes()`
- Screenshot: `EditorInterface::get_editor_viewport_2d()`, `EditorInterface::get_editor_viewport_3d()`, `SubViewport` -> `Viewport::get_texture()` -> `Texture2D::get_image()` -> `Image::save_png_to_buffer()`, `Marshalls::raw_to_base64()`
- Input: `Input::parse_input_event()`, `InputEventKey`, `InputEventMouseButton`, `InputEventMouseMotion` constructors and setters

**Confidence:** HIGH for scene management and input injection (well-documented, stable APIs). MEDIUM for viewport screenshots -- `get_editor_viewport_2d()`/`get_editor_viewport_3d()` return SubViewport which has `get_texture()`, but the screenshot capture from the *running game* (as opposed to the editor viewport) requires the game's own viewport, which the editor plugin cannot directly access. The editor can only capture the *editor's* 2D/3D viewports. For the running game, the screenshot must be taken within the game process itself or via an alternative approach (detailed in Pitfalls section below).

## Existing Modules That Need Modification

### 1. `mcp_tool_registry.cpp` -- MODIFICATION (additive only)

**What changes:** Add new ToolDef entries to the static `tools` vector for all new tools.

**Estimated additions:** ~12-15 new ToolDef entries (3-4 UI tools, 4-6 animation tools, 3-4 editor tools, 1-2 screenshot/input tools).

**Pattern:** Identical to existing entries. No structural changes needed.

```cpp
// Example new entry
{
    "get_control_properties",
    "Get Control-node-specific properties including anchors, offsets, size flags, and theme overrides",
    {
        {"type", "object"},
        {"properties", {
            {"node_path", {{"type", "string"}, {"description", "Path to Control node relative to scene root"}}}
        }},
        {"required", {"node_path"}}
    },
    {4, 3, 0}  // Available in all supported versions
},
```

### 2. `mcp_server.cpp` -- MODIFICATION (additive only)

**What changes:** Add new `#include` directives for new tool headers, and add new `if (tool_name == "...")` blocks inside `handle_request()`.

**Current state:** The dispatch chain has 18 tool entries. Adding 12-15 more is manageable but makes the file longer (~700+ lines). The if-else chain pattern is straightforward and grep-friendly. No refactoring to a map-based dispatch is needed at this scale (30-35 tools), but it should be considered if tools exceed 50.

**Pattern:** Identical to existing dispatch. Each new tool gets its own `if` block with parameter extraction and delegation to the tool module function.

### 3. `mcp_protocol.h` / `mcp_protocol.cpp` -- MODIFICATION (new function)

**What changes:** Add a new `create_image_tool_result()` function to handle screenshot responses, because the current `create_tool_result()` always wraps results as `type: "text"` content blocks.

**New function:**

```cpp
// In mcp_protocol.h:
nlohmann::json create_image_tool_result(const nlohmann::json& id,
                                         const std::string& base64_data,
                                         const std::string& mime_type);

// Alternatively, a multi-content result builder:
nlohmann::json create_multi_content_tool_result(const nlohmann::json& id,
                                                  const nlohmann::json& content_array);
```

**Implementation:**

```cpp
nlohmann::json create_image_tool_result(const nlohmann::json& id,
                                         const std::string& base64_data,
                                         const std::string& mime_type) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", {
                {
                    {"type", "image"},
                    {"data", base64_data},
                    {"mimeType", mime_type}
                }
            }},
            {"isError", false}
        }}
    };
}
```

This is required because MCP spec 2025-06-18 defines `ImageContent` as `{"type": "image", "data": "base64...", "mimeType": "image/png"}` -- fundamentally different from the `TextContent` that `create_tool_result` currently produces.

**Alternative approach:** The screenshot tool could return text JSON containing a base64 string, but this defeats the purpose -- MCP-aware clients (Claude, Cursor) render `ImageContent` blocks as actual images. Using the proper content type enables visual feedback in the AI chat.

### 4. `mcp_prompts.h` / `mcp_prompts.cpp` -- MODIFICATION (additive)

**What changes:** Add new prompt templates for UI building and animation workflows. The existing pattern (static prompt definitions with argument substitution) requires no structural changes.

### 5. `scene_tools.h` / `scene_tools.cpp` -- POSSIBLE MODIFICATION

**What might change:** The `serialize_node()` function currently inspects Node2D and Node3D for transform/visibility. For UI tools to work well, the scene tree serialization should also report Control-specific properties when a node is a Control subclass. This could mean adding a `Control* control = Object::cast_to<Control>(node)` branch that includes anchors, size, and size_flags in the serialized output.

**Decision:** This is optional. The dedicated `get_control_properties` tool provides the full detail. Adding a lightweight summary (size, anchors_preset) to `serialize_node` output improves AI context quality without bloating the tree response. Recommend adding it as a small enhancement.

## Component Boundary Diagram (v1.1)

```
src/
+-- mcp_server.h/.cpp          [MODIFY: add includes + dispatch for new tools]
+-- mcp_protocol.h/.cpp        [MODIFY: add create_image_tool_result()]
+-- mcp_tool_registry.h/.cpp   [MODIFY: add ~12-15 ToolDef entries]
+-- mcp_plugin.h/.cpp          [NO CHANGE]
+-- mcp_dock.h/.cpp             [NO CHANGE -- or update tool count display]
+-- mcp_prompts.h/.cpp          [MODIFY: add UI/animation prompt templates]
+-- register_types.h/.cpp       [NO CHANGE]
|
+-- scene_tools.h/.cpp          [MINOR MODIFY: Control info in serialize_node]
+-- scene_mutation.h/.cpp       [NO CHANGE]
+-- script_tools.h/.cpp         [NO CHANGE]
+-- project_tools.h/.cpp        [NO CHANGE]
+-- runtime_tools.h/.cpp        [NO CHANGE]
+-- signal_tools.h/.cpp         [NO CHANGE]
+-- variant_parser.h/.cpp       [NO CHANGE]
|
+-- ui_tools.h/.cpp             [NEW: Control properties, theme, layout]
+-- animation_tools.h/.cpp      [NEW: AnimationPlayer, tracks, keyframes]
+-- editor_tools.h/.cpp         [NEW: scene file mgmt, screenshots, input]
```

## Data Flow Changes

### Standard Tools (UI, Animation, Scene Management, Input)

No change to the data flow. These tools follow the exact same pattern as existing tools:

```
IO thread receives JSON-RPC
  -> process_message_io queues PendingRequest
  -> Main thread poll() dequeues
  -> handle_request() dispatches to tool function
  -> Tool function calls Godot API, returns nlohmann::json
  -> create_tool_result() wraps as TextContent
  -> PendingResponse queued
  -> IO thread sends response
```

### Screenshot Tool (New Data Flow)

The screenshot tool introduces a new content type in the response:

```
IO thread receives JSON-RPC (tools/call capture_viewport)
  -> Main thread poll() dispatches
  -> editor_tools::capture_viewport()
     -> EditorInterface::get_editor_viewport_2d() or get_editor_viewport_3d()
     -> SubViewport::get_texture() -> Texture2D::get_image()
     -> Image::save_png_to_buffer() -> PackedByteArray
     -> Marshalls::raw_to_base64() -> String
     -> Convert to std::string
     -> Return as {base64_data, mime_type} pair
  -> mcp_server.cpp uses create_image_tool_result() instead of create_tool_result()
  -> PendingResponse with ImageContent
  -> IO thread sends response
```

**Key difference:** The dispatch logic in `mcp_server.cpp` must know that `capture_viewport` returns image content and call `create_image_tool_result()` instead of `create_tool_result()`. Two approaches:

**Approach A (Recommended):** Tool function returns a struct/tagged-union indicating content type:

```cpp
// In mcp_server.cpp handle_request():
if (tool_name == "capture_viewport") {
    // ... extract args ...
    auto result = capture_viewport(viewport_type, max_width);
    if (result.contains("error")) {
        return mcp::create_tool_result(id, result);  // error as text
    }
    return mcp::create_image_tool_result(id,
        result["data"].get<std::string>(),
        result["mimeType"].get<std::string>());
}
```

**Approach B:** Tool function returns a pre-built content array that mcp_server wraps directly. More flexible but breaks the pattern where tool functions return "just data."

Approach A is simpler, maintains the existing pattern, and only adds special handling for the one tool that needs it.

### Input Injection Data Flow

Input injection targets the **running game**, not the editor scene tree. This is important because:

1. `Input::parse_input_event()` injects into the **global Input singleton** which is shared between editor and game.
2. When `EditorInterface::is_playing_scene()` is true, input events injected via `Input::parse_input_event()` will be processed by the running game's viewport.
3. The tool should verify the game is running before injecting.

```
IO thread receives JSON-RPC (tools/call inject_key_input)
  -> Main thread poll() dispatches
  -> editor_tools::inject_key_input()
     -> Check: EditorInterface::is_playing_scene()? If not, return error
     -> Construct InputEventKey with keycode, pressed state, modifiers
     -> Input::get_singleton()->parse_input_event(event)
     -> Return success JSON
  -> Normal text content response
```

## Suggested Build Order (Phase Dependencies)

The v1.1 features have the following dependency graph:

```
Scene File Management -----> (no deps, uses EditorInterface)
UI System Tools -----------> (no deps, uses Control API)
Animation Tools -----------> (no deps, uses Animation API)
Viewport Screenshots ------> (needs mcp_protocol.cpp change for ImageContent)
Input Injection -----------> (no deps, uses Input API, but needs game running)
Prompt Templates ----------> (depends on all tools being defined)
```

**Recommended build order:**

### Phase 1: Scene File Management (editor_tools -- partial)
**Rationale:** Simplest new feature. Only 3-4 tools. Uses well-known EditorInterface methods that already exist in the codebase. Low risk. Immediately useful -- AI can now save work.

Tools: `save_scene`, `save_scene_as`, `open_scene`, `get_open_scenes`

**Dependencies:** None. **Estimated effort:** Small.

### Phase 2: UI System Tools (ui_tools)
**Rationale:** Next in complexity. Control node APIs are well-documented and stable. Theme overrides follow the same set/get pattern as existing property tools. The variant_parser may need minor extensions for theme types.

Tools: `get_control_properties`, `set_layout_preset`, `set_theme_override`, `remove_theme_override`

**Dependencies:** None, but benefits from Phase 1's `serialize_node` enhancement. **Estimated effort:** Medium.

### Phase 3: Animation Tools (animation_tools)
**Rationale:** Most complex new module. The Animation API has a deep hierarchy (Player -> Library -> Animation -> Track -> Key) with many track types and value formats. Build incrementally: read-only tools first, then mutation.

Tools: `get_animations`, `get_animation_info`, `create_animation`, `add_track`, `insert_keyframe`, `remove_keyframe`

**Dependencies:** None. **Estimated effort:** Large.

### Phase 4: Viewport Screenshots (editor_tools -- partial)
**Rationale:** Requires the `create_image_tool_result()` addition to mcp_protocol.cpp. Binary data handling is a new pattern. Build after text-only tools are solid.

Tools: `capture_viewport`

**Dependencies:** mcp_protocol.cpp change. **Estimated effort:** Medium (mostly the protocol change and base64 encoding pipeline).

### Phase 5: Input Injection (editor_tools -- partial)
**Rationale:** Requires a running game to test, which makes development/testing slower. Input event construction is straightforward but the interaction with the running game process has edge cases. Build last.

Tools: `inject_key_input`, `inject_mouse_button`, `inject_mouse_motion`

**Dependencies:** `runtime_tools` (game must be running). **Estimated effort:** Medium.

### Phase 6: Prompt Templates
**Rationale:** Depends on all tools being defined. Pure text, no API risk.

New prompts: `build_ui_layout`, `setup_animation`, `debug_game` (or similar)

**Dependencies:** All tool definitions finalized. **Estimated effort:** Small.

## Patterns to Follow

### Pattern 1: Tool Module Convention (existing pattern, apply to new modules)

Every new tool module follows the same structure:

```cpp
// ui_tools.h
#ifndef MEOW_GODOT_MCP_UI_TOOLS_H
#define MEOW_GODOT_MCP_UI_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

namespace godot { class EditorUndoRedoManager; }

nlohmann::json get_control_properties(const std::string& node_path);
// ... more functions ...

#endif
#endif
```

```cpp
// ui_tools.cpp
#include "ui_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/control.hpp>
// ... Godot headers ...

// Helper: resolve a node path from scene root and cast to Control
static godot::Control* resolve_control(const std::string& node_path) {
    auto* ei = godot::EditorInterface::get_singleton();
    if (!ei) return nullptr;
    godot::Node* root = ei->get_edited_scene_root();
    if (!root) return nullptr;
    godot::Node* node = root->get_node_or_null(
        godot::NodePath(godot::String(node_path.c_str())));
    return godot::Object::cast_to<godot::Control>(node);
}

nlohmann::json get_control_properties(const std::string& node_path) {
    godot::Control* ctrl = resolve_control(node_path);
    if (!ctrl) {
        return {{"success", false}, {"error", "Control node not found: " + node_path}};
    }
    // ... extract properties ...
}

#endif
```

### Pattern 2: Resolve-and-Cast Helper (new for typed node access)

The existing `signal_tools.cpp` has `resolve_node()`. New modules need type-specific variants:

```cpp
// Resolve and cast to specific type. Returns nullptr if not found or wrong type.
static godot::Control* resolve_control(const std::string& node_path);
static godot::AnimationPlayer* resolve_animation_player(const std::string& node_path);
```

This avoids repeating the EditorInterface -> scene root -> get_node_or_null -> cast_to chain in every function.

### Pattern 3: Error Response Convention (existing, follow exactly)

All tool functions return `{"success": false, "error": "message"}` on failure and `{"success": true, ...data...}` on success. This is consistent across all v1.0 modules and must continue in v1.1.

### Pattern 4: ImageContent Response (NEW pattern for screenshots)

For the screenshot tool only, the response format differs from standard text content:

```cpp
// In mcp_server.cpp, special-case the screenshot tool:
if (tool_name == "capture_viewport") {
    auto result = capture_viewport(viewport_type, max_width);
    if (!result["success"].get<bool>()) {
        return mcp::create_tool_result(id, result);  // Error as text
    }
    // Success: return as MCP ImageContent
    return mcp::create_image_tool_result(
        id,
        result["data"].get<std::string>(),
        "image/png"
    );
}
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Over-Abstracting the Dispatch

**What:** Creating a generic tool dispatch system (e.g., `std::map<std::string, std::function<json(json)>>`) to replace the if-else chain.

**Why avoid:** The if-else chain works. Each tool has different parameter extraction logic. A generic dispatch would need parameter validation/extraction to be either generic (complex) or still per-tool (just moved elsewhere). At 30-35 tools, the if-else chain is readable and grep-friendly. Refactor only if tools exceed 50.

### Anti-Pattern 2: Capturing Running Game Viewport Directly from Editor

**What:** Trying to call `get_viewport().get_texture()` on the running game's viewport from the editor process.

**Why it fails:** The running game is a separate process launched by `EditorInterface::play_main_scene()`. The editor and game do not share a viewport. The editor's viewports (`get_editor_viewport_2d()`, `get_editor_viewport_3d()`) show the *editor* view, not the game's rendered output.

**Instead:** Capture the editor's viewport (which shows the game preview when the game is running in embedded mode on Godot 4.4+), or document that screenshot capture is for editor viewports only. If game-viewport screenshots are needed, that requires injecting a screenshot command into the running game via a debug protocol -- far more complex and out of scope for v1.1.

### Anti-Pattern 3: Blocking Main Thread with Image Encoding

**What:** Encoding a large viewport image to PNG and then to base64 on the main thread.

**Why risky:** `Image::save_png_to_buffer()` and `Marshalls::raw_to_base64()` are CPU-bound. For a 1920x1080 viewport, the PNG encoding could take 50-200ms, which blocks the editor. For typical editor viewports (often smaller), this is acceptable.

**Mitigation:** Add an optional `max_width` parameter to `capture_viewport` that downscales the image before encoding. Default to a reasonable size (e.g., 800px width). Document that large screenshots may cause a brief editor pause.

### Anti-Pattern 4: Assuming Theme Overrides are Simple String Values

**What:** Treating theme override values the same as `set_node_property` string values.

**Why it fails:** Theme overrides have typed APIs:
- `add_theme_color_override(name, Color)` -- needs Color parsing
- `add_theme_constant_override(name, int)` -- needs int parsing
- `add_theme_font_size_override(name, int)` -- needs int parsing
- `add_theme_stylebox_override(name, Ref<StyleBox>)` -- needs StyleBox construction

**Instead:** The `set_theme_override` tool should accept the override type as a parameter and parse the value accordingly. For simple types (color, constant, font_size), reuse `variant_parser`. For StyleBox, support creating a `StyleBoxFlat` with common properties (bg_color, border_width, corner_radius) via a JSON object parameter.

### Anti-Pattern 5: Animation Keyframe Values Without Type Context

**What:** Accepting keyframe values as plain strings without knowing the track type.

**Why it fails:** Different track types have fundamentally different key value formats:
- TYPE_VALUE: Variant (could be anything -- Vector2, float, Color, bool)
- TYPE_POSITION_3D: Vector3
- TYPE_ROTATION_3D: Quaternion
- TYPE_SCALE_3D: Vector3
- TYPE_BEZIER: float + in/out handles
- TYPE_METHOD: Dictionary with method name + args

**Instead:** The `insert_keyframe` tool should either infer the expected type from the track type (recommended), or require the caller to specify. For TYPE_VALUE tracks, the existing `variant_parser` can handle string-to-Variant conversion. For specialized tracks (position, rotation, scale), parse the value as the expected vector/quaternion type.

## Scalability Considerations

| Concern | v1.0 (18 tools) | v1.1 (~30 tools) | Future (50+ tools) |
|---------|-----------------|-------------------|---------------------|
| Tool dispatch | if-else chain | if-else chain (still manageable) | Consider map-based dispatch |
| Tool registry | Static vector, ~250 lines | ~400 lines | Consider per-module registration |
| Binary responses | Not supported | 1 tool (screenshot) | May need generic binary content support |
| mcp_server.cpp size | ~570 lines | ~800-900 lines | Consider splitting dispatch into handler files |
| Test surface | 132 unit tests | Add ~50 for new modules | Per-module test files |

## Key Technical Decisions

### Decision 1: Editor Viewport Screenshots, NOT Game Viewport

**What:** The `capture_viewport` tool captures the editor's 2D or 3D viewport using `EditorInterface::get_editor_viewport_2d()`/`get_editor_viewport_3d()`.

**Why:** The running game is a separate OS process. The editor cannot directly read its framebuffer. The editor viewports are accessible via SubViewport objects. When the game runs in embedded mode (Godot 4.4+), the editor's game preview viewport may show the running game, but this is not guaranteed across versions.

**Implication:** The tool description should clearly state it captures the "editor viewport" not the "game screen."

### Decision 2: MCP ImageContent for Screenshots

**What:** Return screenshots as MCP `ImageContent` blocks (`type: "image"`, base64 data, mime type) rather than as text containing a base64 string.

**Why:** MCP-aware clients (Claude Desktop, Cursor) render ImageContent as actual images in the chat. Returning base64 in a text block loses this capability. The MCP spec (2025-06-18) explicitly defines ImageContent for this purpose.

**Implication:** Requires adding `create_image_tool_result()` to `mcp_protocol.cpp`.

### Decision 3: Input Injection via `Input::parse_input_event()`

**What:** Use Godot's `Input::parse_input_event()` to inject synthetic input events.

**Why:** This is the standard Godot API for programmatic input. It feeds events into the same input processing pipeline as real hardware input. Available in godot-cpp.

**Limitation:** This injects into the *editor process*, not the game process. When the game runs as a separate process (which is the default), the injected input does not reach the game. Input injection only works when the game runs in the editor's embedded game preview. This is a significant limitation that should be clearly documented.

**Alternative consideration:** For out-of-process games, input injection would require OS-level input simulation (SendInput on Windows, xdotool on Linux) or a separate mechanism. This is out of scope for v1.1.

### Decision 4: No UndoRedo for Animation Track/Key Operations

**What:** Animation track and keyframe operations will NOT use EditorUndoRedoManager initially.

**Why:** Animation resources are modified through the Animation class API directly, not through the scene tree node property system. Implementing UndoRedo for animation operations requires recording the old state (all keyframes on a track), setting the new state, and providing a reverse operation -- significantly more complex than node property changes. The editor's built-in animation editor does handle undo/redo, but through internal mechanisms not exposed via EditorUndoRedoManager in a clean way for external callers.

**Mitigation:** Mark animation tools as "direct mutation, no undo support" in their tool descriptions. Revisit in v1.2 if users request it.

### Decision 5: Theme Override Types as Explicit Parameter

**What:** The `set_theme_override` tool takes an explicit `override_type` parameter rather than auto-detecting the type.

**Why:** Theme override APIs are type-specific (`add_theme_color_override`, `add_theme_constant_override`, etc.). Auto-detection would require querying the theme for the expected type of a given name, which is fragile -- the same name could be a color in one control type and a constant in another. Explicit typing is unambiguous.

## Sources

- [EditorInterface -- Godot Engine (stable)](https://docs.godotengine.org/en/stable/classes/class_editorinterface.html) -- Scene management, viewport access
- [EditorInterface -- Godot Engine (4.4)](https://docs.godotengine.org/en/4.4/classes/class_editorinterface.html) -- Version-specific API
- [Animation -- Godot Engine (stable)](https://docs.godotengine.org/en/stable/classes/class_animation.html) -- Track and keyframe API
- [AnimationPlayer -- Godot Engine (4.4)](https://docs.godotengine.org/en/4.4/classes/class_animationplayer.html) -- Playback and library management
- [Animation Track Types -- Godot Engine (stable)](https://docs.godotengine.org/en/stable/tutorials/animation/animation_track_types.html) -- Track type reference
- [Animation System DeepWiki](https://deepwiki.com/godotengine/godot/4.7-animation-system) -- Internal architecture
- [Control -- Godot Engine (stable)](https://docs.godotengine.org/en/stable/classes/class_control.html) -- UI node properties, theme overrides
- [StyleBox -- Godot Engine (stable)](https://docs.godotengine.org/en/stable/classes/class_stylebox.html) -- StyleBox resource API
- [Using the Theme Editor -- Godot Engine (4.4)](https://docs.godotengine.org/en/4.4/tutorials/ui/gui_using_theme_editor.html) -- Theme system overview
- [MCP Tools Specification (2025-06-18)](https://modelcontextprotocol.io/specification/2025-06-18/server/tools) -- ImageContent, TextContent response types
- [InputEventKey -- Godot Engine (4.4)](https://docs.godotengine.org/en/4.4/classes/class_inputeventkey.html) -- Key input construction
- [Using InputEvent -- Godot Engine (4.4)](https://docs.godotengine.org/en/4.4/tutorials/inputs/inputevent.html) -- Input injection via parse_input_event
- [In-Engine Screenshots (2025)](https://shaggydev.com/2025/02/05/godot-screenshots/) -- Viewport screenshot pattern
- [Expose Editor Viewports PR #68696](https://github.com/godotengine/godot/pull/68696) -- get_editor_viewport_2d/3d introduction
- [close_scene proposal #8806](https://github.com/godotengine/godot-proposals/issues/8806) -- Missing close_scene API
- [GDExtension Input Handling](https://godotforums.org/d/32909-input-handling-in-godot-40-using-gdextension) -- C++ input event handling in GDExtension
- godot-cpp headers verified: `animation.hpp`, `animation_player.hpp`, `animation_mixer.hpp`, `animation_library.hpp`, `control.hpp`, `viewport.hpp`, `sub_viewport.hpp`, `image.hpp`, `marshalls.hpp`, `input.hpp`, `input_event_key.hpp`, `input_event_mouse_button.hpp`, `input_event_mouse_motion.hpp`, `editor_interface.hpp`
