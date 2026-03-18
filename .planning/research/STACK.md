# Technology Stack: v1.1 UI & Editor Expansion

**Project:** Godot MCP Meow
**Researched:** 2026-03-18
**Scope:** NEW APIs only for v1.1 features. Existing stack (C++17, godot-cpp v10+, nlohmann/json, SCons, stdio bridge + TCP relay, GoogleTest) is validated and unchanged.

## Executive Summary

v1.1 adds five feature areas: UI system tools, Animation tools, scene file management, input injection, and viewport screenshots. **No new external libraries are needed.** All features use godot-cpp bindings to existing Godot 4.3+ engine APIs. The existing tool architecture (ToolDef registry + dispatch in mcp_server.cpp + per-module .cpp/.h files) extends naturally to all five areas.

The key architectural insight: these features operate on the **same node tree** the existing tools already manipulate. `set_node_property` can already set Control/Theme/Animation properties via the variant parser. The new tools add **domain-specific convenience** -- bulk operations, structured queries, and capabilities that require multiple Godot API calls (e.g., "add animation track + insert keyframes" as one tool call).

---

## New godot-cpp Headers Required

All headers verified present in `godot-cpp/gen/include/godot_cpp/classes/`.

### UI System Tools

| Header | Class | Purpose | Confidence |
|--------|-------|---------|------------|
| `control.hpp` | `Control` | Theme override methods, anchor/offset, size flags, focus | HIGH |
| `theme.hpp` | `Theme` | Create/modify Theme resources (set_stylebox, set_color, etc.) | HIGH |
| `style_box.hpp` | `StyleBox` | Base class for content_margin access | HIGH |
| `style_box_flat.hpp` | `StyleBoxFlat` | Create flat styleboxes (bg_color, border, corner_radius) | HIGH |
| `style_box_texture.hpp` | `StyleBoxTexture` | Texture-based styleboxes | MEDIUM |
| `container.hpp` | `Container` | Base container, fit_child_in_rect | HIGH |
| `box_container.hpp` | `BoxContainer` | alignment property, separation theme constant | HIGH |
| `grid_container.hpp` | `GridContainer` | columns property | HIGH |
| `margin_container.hpp` | `MarginContainer` | margin_left/right/top/bottom theme constants | HIGH |

### Animation Tools

| Header | Class | Purpose | Confidence |
|--------|-------|---------|------------|
| `animation.hpp` | `Animation` | Track management (add_track, track_insert_key, etc.) | HIGH |
| `animation_player.hpp` | `AnimationPlayer` | Playback control (play, stop, seek) | HIGH |
| `animation_mixer.hpp` | `AnimationMixer` | Library management (add_animation_library, get_animation) | HIGH |
| `animation_library.hpp` | `AnimationLibrary` | Animation storage (add_animation, get_animation_list) | HIGH |

### Scene File Management

| Header | Class | Purpose | Confidence |
|--------|-------|---------|------------|
| `editor_interface.hpp` | `EditorInterface` | Already used. New methods: save_scene, open_scene_from_path, etc. | HIGH |

### Input Injection

| Header | Class | Purpose | Confidence |
|--------|-------|---------|------------|
| `input.hpp` | `Input` | parse_input_event, action_press, action_release | HIGH |
| `input_event.hpp` | `InputEvent` | Base input event class | HIGH |
| `input_event_key.hpp` | `InputEventKey` | Keyboard input simulation | HIGH |
| `input_event_mouse_button.hpp` | `InputEventMouseButton` | Mouse click simulation | HIGH |
| `input_event_mouse_motion.hpp` | `InputEventMouseMotion` | Mouse movement simulation | HIGH |
| `input_event_action.hpp` | `InputEventAction` | Named action simulation | HIGH |
| `input_map.hpp` | `InputMap` | Query available input actions | HIGH |

### Viewport Screenshot

| Header | Class | Purpose | Confidence |
|--------|-------|---------|------------|
| `viewport.hpp` | `Viewport` | get_texture() to retrieve rendered content | HIGH |
| `viewport_texture.hpp` | `ViewportTexture` | Texture wrapper for viewport content | HIGH |
| `sub_viewport.hpp` | `SubViewport` | Editor viewports (2D/3D) | HIGH |
| `image.hpp` | `Image` | save_png_to_buffer, resize, get_width/height | HIGH |
| `marshalls.hpp` | `Marshalls` | raw_to_base64 for encoding PNG data to string | HIGH |

---

## Detailed API Surface Per Feature Area

### 1. UI System Tools

**What the existing tools already cover:** `create_node` can create Control subclasses (Button, Label, VBoxContainer, etc.). `set_node_property` can set any property including Control properties (custom_minimum_size, size_flags_horizontal, etc.) via the variant parser. `get_scene_tree` already reports node types.

**What NEW tools need to add:**

#### A. Control Inspector Tool (get_control_info)

Read UI-specific properties in structured form. Uses `Control` class API.

```cpp
#include <godot_cpp/classes/control.hpp>

// Key methods to call on Control* nodes:
Control* ctrl = Object::cast_to<Control>(node);
ctrl->get_anchor(SIDE_LEFT);     // float
ctrl->get_anchor(SIDE_TOP);      // float
ctrl->get_anchor(SIDE_RIGHT);    // float
ctrl->get_anchor(SIDE_BOTTOM);   // float
ctrl->get_offset(SIDE_LEFT);     // float - renamed from "margin" in Godot 4
ctrl->get_size();                 // Vector2
ctrl->get_custom_minimum_size(); // Vector2
ctrl->get_focus_mode();          // FocusMode enum
ctrl->get_h_size_flags();        // int (bitfield)
ctrl->get_v_size_flags();        // int (bitfield)
ctrl->get_tooltip_text();        // String
```

**Why needed:** `get_scene_tree` only returns basic node info (name, type, path, transform, visibility, script). UI nodes need anchor/offset/size_flags data for the AI to understand and manipulate layouts.

#### B. Theme Override Tool (set_theme_override)

Apply per-control theme overrides. Uses `Control` class theme methods.

```cpp
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>

Control* ctrl = Object::cast_to<Control>(node);

// Six override categories:
ctrl->add_theme_color_override("font_color", Color(1, 0, 0, 1));
ctrl->add_theme_constant_override("separation", 10);
ctrl->add_theme_font_size_override("font_size", 16);
ctrl->add_theme_stylebox_override("panel", stylebox_ref);
ctrl->add_theme_font_override("font", font_ref);
ctrl->add_theme_icon_override("icon", texture_ref);

// Query overrides:
ctrl->has_theme_color_override("font_color");  // bool
ctrl->get_theme_color("font_color");            // Color

// Remove overrides:
ctrl->remove_theme_color_override("font_color");
```

**Why needed:** While `set_node_property` can set simple properties, theme overrides use a different API surface (not regular properties). The AI needs dedicated tools to set override colors, fonts, and styleboxes per-control.

#### C. StyleBox Creation Tool (create_stylebox)

Create StyleBoxFlat resources. This is a **resource creation** action, not a property set.

```cpp
#include <godot_cpp/classes/style_box_flat.hpp>

Ref<StyleBoxFlat> sb;
sb.instantiate();
sb->set_bg_color(Color(0.2, 0.2, 0.2, 1.0));
sb->set_border_color(Color(1.0, 1.0, 1.0, 1.0));
sb->set_border_width_all(2);
sb->set_corner_radius_all(4);
sb->set_shadow_color(Color(0, 0, 0, 0.3));
sb->set_shadow_size(4);
sb->set_content_margin_all(8);  // Inherited from StyleBox base
sb->set_expand_margin_all(0);   // StyleBoxFlat-specific

// Apply to a control:
ctrl->add_theme_stylebox_override("panel", sb);
```

**StyleBoxFlat properties (verified):**

| Property | Type | Description |
|----------|------|-------------|
| `bg_color` | Color | Background fill color |
| `border_color` | Color | Border stroke color |
| `border_width_top/bottom/left/right` | int | Per-side border widths |
| `corner_radius_top_left/top_right/bottom_left/bottom_right` | int | Per-corner radii |
| `shadow_color` | Color | Drop shadow color |
| `shadow_size` | int | Shadow blur size in pixels |
| `shadow_offset` | Vector2 | Shadow offset |
| `content_margin_top/bottom/left/right` | float | Inherited from StyleBox base |
| `expand_margin_top/bottom/left/right` | float | StyleBoxFlat-specific expand |

**Convenience methods:** `set_border_width_all(int)`, `set_corner_radius_all(int)`, `set_expand_margin_all(float)`, `set_content_margin_all(float)`.

**Why needed:** StyleBoxFlat is the most common StyleBox type. Creating one requires instantiating a Resource, setting multiple properties, and attaching it to a Control -- a multi-step operation that warrants a dedicated tool.

#### D. Anchor/Layout Preset Tool (set_control_layout)

```cpp
#include <godot_cpp/classes/control.hpp>

// Preset-based layout (most common):
ctrl->set_anchors_preset(Control::PRESET_FULL_RECT);      // Fill parent
ctrl->set_anchors_preset(Control::PRESET_CENTER);          // Center
ctrl->set_anchors_preset(Control::PRESET_TOP_LEFT);        // Top-left corner
// ... 16 presets available

// Manual anchors:
ctrl->set_anchor(SIDE_LEFT, 0.0);
ctrl->set_anchor(SIDE_TOP, 0.0);
ctrl->set_anchor(SIDE_RIGHT, 1.0);
ctrl->set_anchor(SIDE_BOTTOM, 1.0);
ctrl->set_offset(SIDE_LEFT, 10);  // Margin from anchor

// Combined anchor + offset:
ctrl->set_anchors_and_offsets_preset(
    Control::PRESET_CENTER,         // preset
    Control::PRESET_MODE_KEEP_SIZE, // resize mode
    0                               // margin
);
```

**LayoutPreset enum values (verified in Godot 4.3):**

| Value | Name | Effect |
|-------|------|--------|
| 0 | PRESET_TOP_LEFT | Top-left corner |
| 1 | PRESET_TOP_RIGHT | Top-right corner |
| 2 | PRESET_BOTTOM_LEFT | Bottom-left corner |
| 3 | PRESET_BOTTOM_RIGHT | Bottom-right corner |
| 4 | PRESET_CENTER_LEFT | Center of left edge |
| 5 | PRESET_CENTER_TOP | Center of top edge |
| 6 | PRESET_CENTER_RIGHT | Center of right edge |
| 7 | PRESET_CENTER_BOTTOM | Center of bottom edge |
| 8 | PRESET_CENTER | Centered in parent |
| 9 | PRESET_LEFT_WIDE | Full left column |
| 10 | PRESET_TOP_WIDE | Full top row |
| 11 | PRESET_RIGHT_WIDE | Full right column |
| 12 | PRESET_BOTTOM_WIDE | Full bottom row |
| 13 | PRESET_VCENTER_WIDE | Vertically centered, full width |
| 14 | PRESET_HCENTER_WIDE | Horizontally centered, full height |
| 15 | PRESET_FULL_RECT | Fill entire parent |

**Why needed:** Layout presets are the standard way to position Controls. The AI needs to set "full rect", "center", "top wide" etc. rather than computing raw anchor values.

### 2. Animation Tools

**What existing tools cover:** `create_node` can create AnimationPlayer nodes. `set_node_property` can set simple animation properties. `get_resource_info` can read .tres animation resources.

**What NEW tools need to add:**

#### A. Animation Track Management

```cpp
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/animation_mixer.hpp>
#include <godot_cpp/classes/animation_library.hpp>

// Finding the AnimationPlayer from node tree:
AnimationPlayer* player = Object::cast_to<AnimationPlayer>(node);

// Get existing animation:
Ref<Animation> anim = player->get_animation("idle");  // inherited from AnimationMixer

// Create a new animation:
Ref<Animation> anim;
anim.instantiate();
anim->set_length(1.0);
anim->set_loop_mode(Animation::LOOP_LINEAR);

// Add a track:
int idx = anim->add_track(Animation::TYPE_VALUE);
anim->track_set_path(idx, NodePath("Sprite2D:modulate"));

// Insert keyframes:
anim->track_insert_key(idx, 0.0, Variant(Color(1,1,1,1)));
anim->track_insert_key(idx, 0.5, Variant(Color(1,0,0,1)));
anim->track_insert_key(idx, 1.0, Variant(Color(1,1,1,1)));

// Add to library:
Ref<AnimationLibrary> lib;
if (player->has_animation_library("")) {
    lib = player->get_animation_library("");
} else {
    lib.instantiate();
    player->add_animation_library("", lib);
}
lib->add_animation("flash_red", anim);

// For position/rotation/scale, use dedicated track types:
int pos_idx = anim->add_track(Animation::TYPE_POSITION_3D);
anim->track_set_path(pos_idx, NodePath("Character"));
anim->track_insert_key(pos_idx, 0.0, Variant(Vector3(0,0,0)));
anim->track_insert_key(pos_idx, 1.0, Variant(Vector3(10,0,0)));

// Method call tracks require Dictionary:
int method_idx = anim->add_track(Animation::TYPE_METHOD);
anim->track_set_path(method_idx, NodePath("."));
Dictionary method_dict;
method_dict["method"] = "emit_particles";
method_dict["args"] = Array();
anim->track_insert_key(method_idx, 0.5, method_dict);
```

**TrackType enum (verified in Godot 4.3):**

| Value | Name | Key Type |
|-------|------|----------|
| 0 | TYPE_VALUE | Variant (any interpolatable value) |
| 1 | TYPE_POSITION_3D | Vector3 |
| 2 | TYPE_ROTATION_3D | Quaternion |
| 3 | TYPE_SCALE_3D | Vector3 |
| 4 | TYPE_BLEND_SHAPE | float |
| 5 | TYPE_METHOD | Dictionary {"method": String, "args": Array} |
| 6 | TYPE_BEZIER | float (with custom curves) |
| 7 | TYPE_AUDIO | AudioStream reference |
| 8 | TYPE_ANIMATION | StringName (nested animation) |

**Key Animation class methods (verified):**

| Method | Signature | Purpose |
|--------|-----------|---------|
| `add_track` | `int add_track(TrackType type)` | Add new track, returns index |
| `remove_track` | `void remove_track(int track_idx)` | Remove track by index |
| `get_track_count` | `int get_track_count()` | Number of tracks |
| `track_get_type` | `TrackType track_get_type(int track_idx)` | Track type enum |
| `track_set_path` | `void track_set_path(int track_idx, NodePath path)` | Set target node path |
| `track_get_path` | `NodePath track_get_path(int track_idx)` | Get target node path |
| `track_insert_key` | `int track_insert_key(int track_idx, float time, Variant key, float transition=1)` | Insert keyframe |
| `track_remove_key` | `void track_remove_key(int track_idx, int key_idx)` | Remove keyframe |
| `track_get_key_count` | `int track_get_key_count(int track_idx)` | Key count on track |
| `track_get_key_time` | `float track_get_key_time(int track_idx, int key_idx)` | Key timestamp |
| `track_get_key_value` | `Variant track_get_key_value(int track_idx, int key_idx)` | Key value |
| `set_length` | `void set_length(float sec)` | Set animation duration |
| `set_loop_mode` | `void set_loop_mode(LoopMode mode)` | Set loop behavior |

**Key AnimationMixer methods (inherited by AnimationPlayer):**

| Method | Signature | Purpose |
|--------|-----------|---------|
| `add_animation_library` | `Error add_animation_library(StringName name, AnimationLibrary lib)` | Add library |
| `get_animation_library` | `AnimationLibrary get_animation_library(StringName name)` | Get library |
| `has_animation_library` | `bool has_animation_library(StringName name)` | Check library exists |
| `get_animation_library_list` | `StringName[] get_animation_library_list()` | List all library names |
| `get_animation` | `Animation get_animation(StringName name)` | Get animation by name |
| `has_animation` | `bool has_animation(StringName name)` | Check animation exists |

**Key AnimationLibrary methods:**

| Method | Signature | Purpose |
|--------|-----------|---------|
| `add_animation` | `Error add_animation(StringName name, Animation anim)` | Add to library |
| `remove_animation` | `void remove_animation(StringName name)` | Remove from library |
| `has_animation` | `bool has_animation(StringName name)` | Check exists |
| `get_animation` | `Animation get_animation(StringName name)` | Get by name |
| `get_animation_list` | `StringName[] get_animation_list()` | List all |

**Why needed:** Animation creation is inherently multi-step: create Animation resource, add tracks, set paths, insert keys, attach to library, attach to player. This cannot be done with `set_node_property` alone.

#### B. Animation Playback Control

```cpp
AnimationPlayer* player = Object::cast_to<AnimationPlayer>(node);

player->play("idle");                    // Play by name
player->play("idle", -1, 2.0);          // Play at 2x speed
player->play_backwards("idle");          // Play reversed
player->stop();                          // Stop
player->pause();                         // Pause
player->seek(0.5, true);                // Seek to 0.5s, update immediately
player->is_playing();                    // bool
player->get_current_animation();         // StringName
player->get_current_animation_position();// float
```

**Why needed:** Playback control is needed for the AI to test animations during runtime, combined with screenshot capture for visual feedback.

#### C. Animation Query

```cpp
// List all animations:
TypedArray<StringName> lib_list = player->get_animation_library_list();
for (int i = 0; i < lib_list.size(); i++) {
    StringName lib_name = lib_list[i];
    Ref<AnimationLibrary> lib = player->get_animation_library(lib_name);
    TypedArray<StringName> anim_list = lib->get_animation_list();
    // ...
}

// Query tracks:
Ref<Animation> anim = player->get_animation("idle");
int track_count = anim->get_track_count();
for (int t = 0; t < track_count; t++) {
    Animation::TrackType type = anim->track_get_type(t);
    NodePath path = anim->track_get_path(t);
    int key_count = anim->track_get_key_count(t);
    for (int k = 0; k < key_count; k++) {
        double time = anim->track_get_key_time(t, k);
        Variant value = anim->track_get_key_value(t, k);
    }
}
```

**Why needed:** The AI needs to inspect existing animations before modifying them, similar to how `get_scene_tree` is used before `create_node` / `set_node_property`.

### 3. Scene File Management

**All methods available on EditorInterface singleton, already accessed in existing code via `EditorInterface::get_singleton()`.**

```cpp
#include <godot_cpp/classes/editor_interface.hpp>

EditorInterface* ei = EditorInterface::get_singleton();

// Save current scene:
Error err = ei->save_scene();  // Returns OK or ERR_CANT_CREATE

// Save as new path:
ei->save_scene_as("res://levels/level_02.tscn", true);  // with_preview=true

// Save all open scenes:
ei->save_all_scenes();

// Open a different scene:
ei->open_scene_from_path("res://scenes/main_menu.tscn");

// Reload current scene from disk:
ei->reload_scene_from_path("res://scenes/main.tscn");

// Query open scenes:
PackedStringArray open = ei->get_open_scenes();  // All open scene paths

// Get current scene root:
Node* root = ei->get_edited_scene_root();  // Already used in existing code

// Switch editor main screen:
ei->set_main_screen_editor("2D");  // or "3D", "Script", "AssetLib"
```

**Verified EditorInterface scene methods (Godot 4.3):**

| Method | Signature | Purpose |
|--------|-----------|---------|
| `save_scene` | `Error save_scene()` | Save current scene |
| `save_scene_as` | `void save_scene_as(String path, bool with_preview=true)` | Save to new path |
| `save_all_scenes` | `void save_all_scenes()` | Save all open scenes |
| `open_scene_from_path` | `void open_scene_from_path(String scene_filepath)` | Open/switch scene |
| `reload_scene_from_path` | `void reload_scene_from_path(String scene_filepath)` | Reload from disk |
| `get_open_scenes` | `PackedStringArray get_open_scenes()` | List open scene paths |
| `get_edited_scene_root` | `Node get_edited_scene_root()` | Current scene root |
| `set_main_screen_editor` | `void set_main_screen_editor(String name)` | Switch 2D/3D/Script |
| `edit_node` | `void edit_node(Node node)` | Select node in editor |
| `select_file` | `void select_file(String file)` | Select in FileSystem dock |

**Why needed:** Current tools can read/modify the currently open scene, but cannot save, open a different scene, or query which scenes are open. Scene file management is essential for multi-scene workflows.

**Known limitation (verified):** There is NO `close_scene` method in Godot 4.3. An active proposal exists (godotengine/godot-proposals#8806) but it has not been implemented. We cannot close individual scene tabs programmatically.

### 4. Input Injection

**Requires a running game.** Input is injected via the `Input` singleton, which affects the running game process.

```cpp
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_action.hpp>

Input* input = Input::get_singleton();

// Method 1: Named action (preferred -- works with input map):
input->action_press("ui_accept", 1.0);  // Simulate press
input->action_release("ui_accept");      // Simulate release

// Method 2: Simulate a specific key:
Ref<InputEventKey> key_event;
key_event.instantiate();
key_event->set_keycode(Key::KEY_SPACE);
key_event->set_pressed(true);
input->parse_input_event(key_event);

// Method 3: Simulate mouse click:
Ref<InputEventMouseButton> mouse_event;
mouse_event.instantiate();
mouse_event->set_button_index(MouseButton::MOUSE_BUTTON_LEFT);
mouse_event->set_position(Vector2(400, 300));
mouse_event->set_pressed(true);
input->parse_input_event(mouse_event);

// Method 4: Simulate mouse movement:
Ref<InputEventMouseMotion> motion;
motion.instantiate();
motion->set_position(Vector2(500, 400));
motion->set_relative(Vector2(100, 100));
input->parse_input_event(motion);

// Warp mouse directly:
input->warp_mouse(Vector2(960, 540));

// Query available actions from InputMap:
InputMap* imap = InputMap::get_singleton();
TypedArray<StringName> actions = imap->get_actions();
```

**CRITICAL DESIGN CONSTRAINT:** Input injection from the editor plugin process (GDExtension) affects the **editor's** Input singleton, NOT the running game's. The game runs as a separate process (spawned via `EditorInterface::play_main_scene()` etc.). Two viable approaches:

1. **Editor viewport input (works now):** `Viewport::push_input(InputEvent event, bool in_local_coords=false)` can push events into the editor's 2D/3D viewport SubViewport. This lets the AI interact with editor-side preview but does NOT reach a running game.

2. **Running game input (requires IPC):** The game runs as a child process. To inject input, we need communication. The recommended approach is a **thin GDScript autoload** injected into the game project that:
   - Reads input commands from a file (e.g., `user://mcp_input_commands.json`) or listens on a UDP port
   - Calls `Input.parse_input_event()` inside the game process
   - This is simpler and more reliable than trying to use the editor-side Input singleton

**Recommendation for v1.1:** Implement BOTH:
- **Editor-side:** `Input.action_press`/`action_release` and `Input.parse_input_event` via the editor's Input singleton (useful for tools that trigger actions in the editor process itself)
- **Game-side (stretch goal):** GDScript autoload with file-based IPC for true game input injection

**Confidence:** MEDIUM for game-side injection (needs prototyping). HIGH for editor-side input API.

### 5. Viewport Screenshot

**Two distinct capture modes:**

#### A. Editor Viewport Screenshot (no game running needed)

```cpp
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/marshalls.hpp>

EditorInterface* ei = EditorInterface::get_singleton();

// Capture 2D editor viewport:
SubViewport* vp2d = ei->get_editor_viewport_2d();
Ref<ViewportTexture> tex = vp2d->get_texture();
Ref<Image> image = tex->get_image();

// Capture 3D editor viewport (index 0-3 for split views):
SubViewport* vp3d = ei->get_editor_viewport_3d(0);
Ref<ViewportTexture> tex3d = vp3d->get_texture();
Ref<Image> image3d = tex3d->get_image();

// Convert to PNG bytes:
PackedByteArray png_data = image->save_png_to_buffer();

// Encode as base64 for MCP protocol transmission:
String base64_str = Marshalls::get_singleton()->raw_to_base64(png_data);

// Or save to disk:
image->save_png("user://screenshot.png");

// Optional: resize before encoding (for large viewports):
image->resize(960, 540, Image::INTERPOLATE_BILINEAR);
```

**Verified Image save methods:**

| Method | Signature | Returns |
|--------|-----------|---------|
| `save_png` | `Error save_png(String path)` | Error code |
| `save_png_to_buffer` | `PackedByteArray save_png_to_buffer()` | PNG bytes |
| `save_jpg_to_buffer` | `PackedByteArray save_jpg_to_buffer(float quality=0.75)` | JPEG bytes |
| `save_webp_to_buffer` | `PackedByteArray save_webp_to_buffer(bool lossy=false, float quality=0.75)` | WebP bytes |
| `resize` | `void resize(int width, int height, Interpolation interp=1)` | void |
| `get_width` | `int get_width()` | Width in pixels |
| `get_height` | `int get_height()` | Height in pixels |

**Viewport access methods (verified, available since Godot 4.2):**

| Method | Signature | Notes |
|--------|-----------|-------|
| `get_editor_viewport_2d` | `SubViewport get_editor_viewport_2d()` | No camera; use global_canvas_transform |
| `get_editor_viewport_3d` | `SubViewport get_editor_viewport_3d(int idx=0)` | 4 split views (0-3) |
| `get_texture` | `ViewportTexture get_texture()` | On any Viewport |
| `get_image` | `Image get_image()` | On ViewportTexture (Ref<Image>) |

**Base64 encoding chain:**
```
Image -> save_png_to_buffer() -> PackedByteArray -> Marshalls::raw_to_base64() -> String -> std::string -> nlohmann::json
```

#### B. Running Game Viewport Screenshot (game must be running)

The game runs in a separate process. The editor cannot directly access the game's viewport. Same approach as input injection:

**Recommended:** GDScript autoload in game project captures via `get_viewport().get_texture().get_image().save_png("user://mcp_screenshot.png")`, MCP server reads the file from disk.

**Known pitfall (verified):** `get_image()` can return blank images if called before the frame is fully rendered. In GDScript, use `await RenderingServer.frame_post_draw` before capturing. In editor context (C++), the viewport is continuously rendering so this is less of an issue.

#### MCP Protocol Integration for Images

MCP supports returning images as base64-encoded content with MIME type. The tool result JSON should include:

```json
{
  "content": [
    {
      "type": "image",
      "data": "<base64 PNG data>",
      "mimeType": "image/png"
    }
  ]
}
```

This requires a small extension to the existing `mcp::create_tool_result()` function to support image content alongside text content. The protocol layer change is minimal.

---

## New Source Files

Following the existing architecture pattern (one .h + one .cpp per feature area):

| File | Purpose | New godot-cpp Includes |
|------|---------|----------------------|
| `src/ui_tools.h` / `.cpp` | Control inspection, theme overrides, StyleBox creation, layout presets | control.hpp, theme.hpp, style_box_flat.hpp, container.hpp, box_container.hpp, grid_container.hpp, margin_container.hpp |
| `src/animation_tools.h` / `.cpp` | Animation/track/keyframe CRUD, playback control, animation query | animation.hpp, animation_player.hpp, animation_mixer.hpp, animation_library.hpp |
| `src/editor_tools.h` / `.cpp` | Scene save/load/switch, editor viewport access, main screen switch | editor_interface.hpp (already available) |
| `src/input_tools.h` / `.cpp` | Input injection (action_press/release, parse_input_event) | input.hpp, input_event_key.hpp, input_event_mouse_button.hpp, input_event_mouse_motion.hpp, input_event_action.hpp, input_map.hpp |
| `src/screenshot_tools.h` / `.cpp` | Editor viewport capture, PNG encoding, base64 conversion | sub_viewport.hpp, viewport_texture.hpp, image.hpp, marshalls.hpp |

**Integration points (same pattern as existing tools):**
1. Each new module adds ToolDef entries in `mcp_tool_registry.cpp`
2. Each new module adds dispatch branches in `mcp_server.cpp::handle_request()` under `tools/call`
3. New `#include` lines added to `mcp_server.cpp`
4. No changes needed to bridge, TCP/IO thread, or MCP protocol layer (except image content type for screenshots)

---

## What NOT to Add (Already Covered by Existing v1.0 Tools)

| Capability | Existing Tool | Why New Tool Unnecessary |
|------------|---------------|--------------------------|
| Create Control nodes (Button, Label, etc.) | `create_node` | Pass `type: "Button"` -- works for all node types |
| Set Control properties (position, size, visible) | `set_node_property` | Variant parser handles all property types |
| Create AnimationPlayer nodes | `create_node` | Pass `type: "AnimationPlayer"` |
| Read .tres resource properties | `get_resource_info` | Already reads resource properties |
| Set simple properties (autoplay, text, etc.) | `set_node_property` | Works for any property |
| Run/stop game | `run_game` / `stop_game` | Already implemented |
| Read project files | `list_project_files` | Already implemented |
| Node signals | `get_node_signals` / `connect_signal` | Already implemented |
| Script management | `read_script` / `write_script` / `edit_script` | Already implemented |

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Theme application | Per-control override methods | Create full Theme resource | Overrides are more common for AI-driven adjustments; full themes are complex multi-type resources |
| StyleBox creation | StyleBoxFlat primarily | StyleBoxTexture, StyleBoxLine | StyleBoxFlat covers 95% of UI styling use cases; texture-based needs image assets |
| Input injection (game) | GDScript autoload + file IPC | TCP from editor to game process | File-based is simpler, no port conflicts, cross-platform |
| Input injection (game) | GDScript autoload + file IPC | Editor-side Input singleton | Editor Input does not reach game process (separate OS processes) |
| Screenshot format | PNG via save_png_to_buffer | JPEG via save_jpg_to_buffer | PNG is lossless, better for AI visual analysis; size acceptable for single frames |
| Screenshot encoding | base64 via Marshalls | Save to disk + return path | base64 inline is standard MCP image content; disk requires client file access |
| Animation track format | Variant-based (Godot native) | Custom JSON intermediate | Direct Godot Variant is simpler; JSON would require unnecessary conversion |
| Animation library naming | Default library ("") | Named libraries | Most projects use default; named libraries add complexity without benefit for basic use |

---

## Variant Parser Extensions

The existing `variant_parser.cpp` handles all new property types without modification:

1. **Vector2/Vector3/Color** -- Already handled via `UtilityFunctions::str_to_var()`
2. **Rect2** (for container rects) -- Also handled by `str_to_var("Rect2(0,0,100,100)")`
3. **NodePath** (for animation track paths) -- Passed as strings, converted in tool code
4. **Enums** (FocusMode, SizeFlags, TrackType, LayoutPreset) -- Passed as integers, handled by int parsing

**No variant parser changes needed.** New tool functions do domain-specific parameter parsing (e.g., converting track type string "value" to `Animation::TYPE_VALUE` integer).

---

## Build System Impact

**SCons changes:** None needed. New .cpp files in `src/` are auto-detected by the existing glob pattern in SConstruct.

**CMake changes (tests):** Add new test files for the new tool modules to CMakeLists.txt.

**Compilation impact:** ~5 new .cpp files, ~2000-3000 lines estimated. Build time increase: negligible (~5s on typical machine).

---

## Installation

No new package installations. No new dependencies. Just new source files using existing godot-cpp headers.

```bash
# No new commands needed. Existing build:
scons platform=<platform> target=template_debug

# For tests:
cd tests && cmake -B build && cmake --build build
```

---

## Sources

### Official Documentation (Godot 4.3) -- HIGH Confidence
- [EditorInterface class (4.3)](https://docs.godotengine.org/en/4.3/classes/class_editorinterface.html)
- [Animation class](https://docs.godotengine.org/en/stable/classes/class_animation.html)
- [AnimationPlayer class](https://docs.godotengine.org/en/stable/classes/class_animationplayer.html)
- [Control class](https://docs.godotengine.org/en/stable/classes/class_control.html)
- [StyleBox class](https://docs.godotengine.org/en/stable/classes/class_stylebox.html)
- [Theme class](https://docs.godotengine.org/en/stable/classes/class_theme.html)
- [Container class (4.3)](https://docs.godotengine.org/en/4.3/classes/class_container.html)
- [Using InputEvent](https://docs.godotengine.org/en/stable/tutorials/inputs/inputevent.html)
- [InputEventAction class](https://docs.godotengine.org/en/stable/classes/class_inputeventaction.html)
- [Image class](https://docs.godotengine.org/en/4.4/classes/class_image.html)
- [Marshalls class](https://docs.godotengine.org/en/stable/classes/class_marshalls.html)
- [Using Containers](https://docs.godotengine.org/en/stable/tutorials/ui/gui_containers.html)

### godot-cpp Headers (Verified Locally) -- HIGH Confidence
- All 30+ required headers confirmed present in `godot-cpp/gen/include/godot_cpp/classes/`

### Godot Engine Source (API Implementation) -- HIGH Confidence
- [control.cpp](https://github.com/godotengine/godot/blob/master/scene/gui/control.cpp) -- theme override implementation
- [animation.cpp](https://github.com/godotengine/godot/blob/master/scene/resources/animation.cpp) -- track/key implementation
- [box_container.h](https://github.com/godotengine/godot/blob/master/scene/gui/box_container.h) -- BoxContainer binding
- [box_container.cpp](https://github.com/godotengine/godot/blob/master/scene/gui/box_container.cpp) -- alignment, separation
- [grid_container.cpp](https://github.com/godotengine/godot/blob/master/scene/gui/grid_container.cpp) -- columns property

### Rokojori API Mirror (Cross-Verified) -- HIGH Confidence
- [EditorInterface](https://rokojori.com/en/labs/godot/docs/4.3/editorinterface-class)
- [Animation](https://rokojori.com/en/labs/godot/docs/4.3/animation-class)
- [AnimationMixer](https://rokojori.com/en/labs/godot/docs/4.3/animationmixer-class)
- [AnimationLibrary](https://rokojori.com/en/labs/godot/docs/4.3/animationlibrary-class)
- [Control](https://rokojori.com/en/labs/godot/docs/4.3/control-class)
- [Input](https://rokojori.com/en/labs/godot/docs/4.3/input-class)
- [Image](https://rokojori.com/en/labs/godot/docs/4.3/image-class)
- [StyleBoxFlat](https://rokojori.com/en/labs/godot/docs/4.3/styleboxflat-class)
- [StyleBox](https://rokojori.com/en/labs/godot/docs/4.3/stylebox-class)
- [Theme](https://rokojori.com/en/labs/godot/docs/4.3/theme-class)
- [Viewport](https://rokojori.com/en/labs/godot/docs/4.3/viewport-class)

### Known Issues & Limitations -- MEDIUM Confidence
- [SubViewport get_image() blank issue (#106957)](https://github.com/godotengine/godot/issues/106957)
- [No close_scene API (proposal #8806)](https://github.com/godotengine/godot-proposals/issues/8806) -- confirmed limitation
- [SubViewport get_image() stutter (#75877)](https://github.com/godotengine/godot/issues/75877) -- perf consideration
- [Expose editor viewports PR (#68696)](https://github.com/godotengine/godot/pull/68696) -- confirms API since 4.2
- [scene_changed signal bug (#97427)](https://github.com/godotengine/godot/issues/97427) -- edge case with empty scenes
- [Container fitting behavior proposal (#9616)](https://github.com/godotengine/godot-proposals/issues/9616)
