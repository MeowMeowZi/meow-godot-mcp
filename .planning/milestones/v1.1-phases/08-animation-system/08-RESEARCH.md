# Phase 8: Animation System - Research

**Researched:** 2026-03-18
**Domain:** Godot 4.3+ Animation API via GDExtension C++ (godot-cpp)
**Confidence:** HIGH

## Summary

Phase 8 adds 5 MCP tools for animation creation and management: `create_animation`, `add_animation_track`, `set_keyframe`, `get_animation_info`, and `set_animation_properties`. The Godot animation API is well-documented and the godot-cpp headers confirm all needed methods exist. The architecture follows the established tool module pattern from Phase 7 (ui_tools.h/cpp).

The Godot animation system has a three-level hierarchy: AnimationPlayer (node) -> AnimationLibrary (resource, named container) -> Animation (resource, tracks+keyframes). In Godot 4.x, AnimationPlayer inherits from AnimationMixer, which provides the `add_animation_library()`, `get_animation_library()`, `get_animation_list()`, and `has_animation()` methods. The Animation class itself provides all track and keyframe manipulation: `add_track()`, `track_insert_key()`, `track_get_key_value()`, etc.

**Primary recommendation:** Follow the ui_tools.h/cpp module pattern exactly. Create `animation_tools.h` + `animation_tools.cpp` with 5 free functions returning `nlohmann::json`. Use the existing `variant_parser.h` for keyframe value parsing. UndoRedo only for create_animation and set_animation_properties as decided in CONTEXT.md.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **5 Dedicated Animation Tools**: create_animation, add_animation_track, set_keyframe, get_animation_info, set_animation_properties
- **create_animation**: Creates AnimationPlayer node + AnimationLibrary (default "") + named Animation resource. One-step complete setup. UndoRedo supported. Maps to SC1.
- **add_animation_track**: Adds a typed track (value, position_3d, rotation_3d, scale_3d) to an existing Animation with correct node path. No UndoRedo (too complex). Maps to SC2.
- **set_keyframe**: Insert, update, or remove keyframes on any track via `action` parameter ("insert"/"update"/"remove"). Values parsed via variant_parser. No UndoRedo. Maps to SC3.
- **get_animation_info**: Query AnimationPlayer: animation list, per-animation track structure, per-track keyframe data (time + value + transition). Full depth. Maps to SC4.
- **set_animation_properties**: Set animation duration, loop mode, step. UndoRedo supported. Maps to SC5.
- **4 supported track types**: value, position_3d, rotation_3d, scale_3d
- **UndoRedo only for**: create_animation and set_animation_properties; track and keyframe operations skip UndoRedo
- **Keyframe values parsed as strings** via existing variant_parser: "Vector3(1,0,0)", "0.5", "Color(1,0,0,1)"
- **set_keyframe action parameter**: "insert" (add new), "update" (modify existing at time), "remove" (delete at time)
- **Automatic library management**: create_animation auto-creates default library (empty string name "")
- **Uniform parameter pattern**: All tools identify animations by `player_path` + `animation_name`
- **Loop mode as string enum**: "none", "linear", "pingpong" maps to Animation::LOOP_NONE, LOOP_LINEAR, LOOP_PINGPONG
- **Duration, loop_mode, step** are the three settable properties
- **Code organization**: New `animation_tools.h` / `animation_tools.cpp`, single module for all 5 functions, free functions returning nlohmann::json

### Claude's Discretion
- Track type string parsing and validation
- Keyframe transition type support (linear, cubic, etc.)
- Animation query response JSON structure details
- Error messages and edge case handling
- Whether to support multiple AnimationLibraries (beyond default "")
- How to handle track index vs track path identification

### Deferred Ideas (OUT OF SCOPE)
- AnimationTree / blend space support -- complex state machine
- Animation playback preview tool -- beyond current scope
- Bezier curve track type -- specialized
- Audio track type -- requires audio stream resources
- Method call track type -- security considerations
- Multiple AnimationLibrary management -- keep simple with default
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ANIM-01 | AI can create AnimationPlayer with AnimationLibrary and Animation resources | `create_animation` tool: AnimationPlayer created via ClassDB::instantiate + add_child + set_owner, AnimationLibrary via Ref::instantiate(), Animation via Ref::instantiate(). UndoRedo for node creation. |
| ANIM-02 | AI can add/delete tracks (Value, Position3D, Rotation3D, etc.) to Animation | `add_animation_track` tool: Animation::add_track(TrackType) + track_set_path(). 4 types: TYPE_VALUE, TYPE_POSITION_3D, TYPE_ROTATION_3D, TYPE_SCALE_3D. Remove via Animation::remove_track(). |
| ANIM-03 | AI can insert/delete/modify keyframes on tracks | `set_keyframe` tool: track_insert_key() for insert, track_find_key()+track_set_key_value() for update, track_find_key()+track_remove_key() for remove. Values via parse_variant(). |
| ANIM-04 | AI can query AnimationPlayer animation list, track structure, keyframe data | `get_animation_info` tool: get_animation_list() for names, get_animation() for Ref<Animation>, then iterate tracks with track_get_type/track_get_path/track_get_key_count/track_get_key_value/track_get_key_time/track_get_key_transition. |
| ANIM-05 | AI can set Animation duration, loop mode, step and it plays correctly | `set_animation_properties` tool: Animation::set_length(), set_loop_mode(), set_step(). UndoRedo supported with old value capture. |
</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ | C++ GDExtension bindings | Project foundation |
| nlohmann/json | 3.12.0 | JSON construction/parsing | Existing across all tools |
| variant_parser.h | in-project | String-to-Variant parsing | Reuse for keyframe values |

### Godot Classes Used
| Class | Header | Purpose |
|-------|--------|---------|
| AnimationPlayer | `<godot_cpp/classes/animation_player.hpp>` | Node that plays animations |
| AnimationMixer | `<godot_cpp/classes/animation_mixer.hpp>` | Base class with library management methods |
| AnimationLibrary | `<godot_cpp/classes/animation_library.hpp>` | Container for named Animation resources |
| Animation | `<godot_cpp/classes/animation.hpp>` | Tracks, keyframes, properties |
| EditorInterface | `<godot_cpp/classes/editor_interface.hpp>` | get_edited_scene_root() |
| EditorUndoRedoManager | `<godot_cpp/classes/editor_undo_redo_manager.hpp>` | UndoRedo for create/properties |
| ClassDB | `<godot_cpp/core/class_db.hpp>` | instantiate AnimationPlayer |
| Node | `<godot_cpp/classes/node.hpp>` | Scene tree node operations |

No new dependencies required. All Godot headers are already available in godot-cpp.

## Architecture Patterns

### Recommended Project Structure
```
src/
  animation_tools.h      # 5 function declarations, #ifdef guarded
  animation_tools.cpp    # 5 function implementations + helpers
  mcp_tool_registry.cpp  # +5 ToolDef entries (29 -> 34 tools)
  mcp_server.cpp         # +5 tool dispatch handlers, +1 #include
```

### Pattern 1: AnimationPlayer Lookup (node + cast)
**What:** Find an AnimationPlayer node by path and validate it
**When to use:** All 5 tools need to locate the AnimationPlayer
**Example:**
```cpp
// Source: Existing pattern from ui_tools.cpp lookup_control()
struct AnimPlayerLookupResult {
    bool success;
    nlohmann::json error;
    AnimationPlayer* player;
};

static AnimPlayerLookupResult lookup_animation_player(const std::string& player_path) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {false, {{"error", "No scene open"}}, nullptr};
    }
    Node* node = scene_root->get_node_or_null(NodePath(player_path.c_str()));
    if (!node) {
        return {false, {{"error", "Node not found: " + player_path}}, nullptr};
    }
    AnimationPlayer* player = Object::cast_to<AnimationPlayer>(node);
    if (!player) {
        return {false, {{"error", "Node is not an AnimationPlayer: " + player_path}}, nullptr};
    }
    return {true, {}, player};
}
```

### Pattern 2: Animation Lookup (player_path + animation_name)
**What:** Find a specific Animation resource within the default library
**When to use:** add_animation_track, set_keyframe, get_animation_info (per-animation), set_animation_properties
**Example:**
```cpp
// Source: godot-cpp animation_mixer.hpp API
static Ref<Animation> find_animation(AnimationPlayer* player, const std::string& anim_name) {
    // AnimationMixer::has_animation() and get_animation() use the naming convention:
    // default library (empty name ""): just "anim_name"
    // named library: "lib_name/anim_name"
    StringName sn(anim_name.c_str());
    if (!player->has_animation(sn)) {
        return Ref<Animation>();
    }
    return player->get_animation(sn);
}
```

### Pattern 3: create_animation with UndoRedo
**What:** Create AnimationPlayer node + AnimationLibrary + Animation in one step
**When to use:** ANIM-01
**Critical details:**
```cpp
// 1. Create AnimationPlayer node via ClassDB (same as create_node in scene_mutation.cpp)
// 2. Create AnimationLibrary via Ref::instantiate()
// 3. Create Animation via Ref::instantiate()
// 4. Add animation to library: library->add_animation(anim_name, animation)
// 5. Add library to player: player->add_animation_library(StringName(""), library)
// 6. Use UndoRedo for the node creation (add_child + set_owner pattern)
//
// IMPORTANT: AnimationLibrary and Animation are Ref-counted Resources, not Nodes.
// They are added to the AnimationPlayer, not to the scene tree.
// UndoRedo covers the node add/remove; the resources follow the node.
```

### Pattern 4: Track Type String Mapping
**What:** Map string track types to Animation::TrackType enum
**When to use:** add_animation_track
**Example:**
```cpp
static const std::unordered_map<std::string, Animation::TrackType> track_type_map = {
    {"value",       Animation::TYPE_VALUE},
    {"position_3d", Animation::TYPE_POSITION_3D},
    {"rotation_3d", Animation::TYPE_ROTATION_3D},
    {"scale_3d",    Animation::TYPE_SCALE_3D},
};
```

### Pattern 5: Keyframe Insert with Typed Tracks
**What:** Position/Rotation/Scale 3D tracks have dedicated insert methods
**When to use:** set_keyframe with insert action
**Critical detail:**
```cpp
// For TYPE_VALUE tracks: use track_insert_key(track_idx, time, variant_value, transition)
// For TYPE_POSITION_3D: use position_track_insert_key(track_idx, time, Vector3)
// For TYPE_ROTATION_3D: use rotation_track_insert_key(track_idx, time, Quaternion)
// For TYPE_SCALE_3D: use scale_track_insert_key(track_idx, time, Vector3)
//
// The generic track_insert_key() also works for 3D tracks but the typed versions
// are more explicit and avoid potential issues with Variant boxing.
```

### Pattern 6: Keyframe Value Serialization for Query
**What:** Convert Godot Variant keyframe values to JSON-friendly strings
**When to use:** get_animation_info
**Example:**
```cpp
// track_get_key_value() returns Variant
// For JSON output, convert to string representation:
// Vector3 -> "Vector3(x, y, z)"
// Quaternion -> "Quaternion(x, y, z, w)"
// float/int -> number
// Use Variant::stringify() or UtilityFunctions::var_to_str()
```

### Anti-Patterns to Avoid
- **Direct track_insert_key on 3D tracks with wrong types:** Position3D tracks expect Vector3, Rotation3D expects Quaternion. Passing wrong type via generic track_insert_key will silently fail or corrupt.
- **Forgetting to set track path:** After add_track(), the track has no node path. Must call track_set_path() or the animation won't affect any node.
- **Using get_animation() without checking has_animation():** Returns null Ref if animation doesn't exist; will crash on dereference.
- **Modifying Animation resources while playing:** Not a concern for editor-time MCP tools, but note it for documentation.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Variant to string | Custom to_string per type | `UtilityFunctions::var_to_str()` | Handles all Variant types, round-trips with str_to_var |
| String to Variant | Custom parsers per type | `parse_variant()` from variant_parser.h | Already handles Vector3, Quaternion, Color, numbers |
| Node creation + scene tree | Manual add_child | `create_node` pattern from scene_mutation.cpp | UndoRedo, set_owner, error handling already solved |
| Track type validation | Inline if/else chains | `std::unordered_map<string, TrackType>` | Clean, same pattern as preset_map in ui_tools.cpp |

**Key insight:** The existing `parse_variant()` function already handles all the value types needed for keyframes (Vector3, Quaternion, Color, float, int, bool). No new parsing code needed.

## Common Pitfalls

### Pitfall 1: AnimationLibrary Naming Convention
**What goes wrong:** Confusion between animation names at AnimationPlayer level vs AnimationLibrary level
**Why it happens:** AnimationMixer::get_animation_list() returns `PackedStringArray` with names in format `"library_name/animation_name"`. Default library (empty string "") returns just `"animation_name"` (no prefix).
**How to avoid:** When creating animations, always use the default library (empty name ""). When looking up animations, use `player->has_animation(StringName(anim_name.c_str()))` which handles the naming convention internally.
**Warning signs:** Animation not found errors when the user provides bare animation names.

### Pitfall 2: Track Index vs Find Key
**What goes wrong:** Using wrong key index when updating/removing keyframes
**Why it happens:** `track_find_key()` returns -1 if no key is found at the specified time. Passing -1 to track_set_key_value or track_remove_key causes out-of-bounds access.
**How to avoid:** Always check return value of `track_find_key()` before using it. Use `Animation::FIND_MODE_APPROX` for floating-point time matching.
**Warning signs:** Crash or Godot error when updating/removing non-existent keyframe.

### Pitfall 3: Quaternion Values for Rotation Tracks
**What goes wrong:** Users provide Euler angles, but rotation_3d tracks store Quaternion
**Why it happens:** Most users think in degrees, but Godot's rotation 3D tracks use Quaternion internally
**How to avoid:** Accept "Quaternion(x,y,z,w)" for rotation tracks via parse_variant. Document clearly that rotation tracks expect Quaternion values. Consider accepting Vector3 Euler angles and converting, but this adds complexity -- recommend staying with Quaternion for v1.
**Warning signs:** Rotation animations don't look right, gimbal lock effects.

### Pitfall 4: AnimationPlayer Must Be In Scene Tree
**What goes wrong:** Adding AnimationLibrary to an AnimationPlayer that isn't in the scene tree yet
**Why it happens:** In the create_animation flow, if library/animation setup happens before add_child
**How to avoid:** Follow the create_node pattern: first add_child + set_owner via UndoRedo, then set up the library/animation. Or: set up resources first, then add to tree (resources don't require tree). Both work, but resources-first is simpler since Ref<> objects are independent of the tree.
**Warning signs:** Resources not persisting when scene is saved.

### Pitfall 5: track_remove_key Takes Key INDEX, Not Time
**What goes wrong:** Passing a time value to track_remove_key()
**Why it happens:** API has both `track_remove_key(track_idx, key_idx)` and `track_remove_key_at_time(track_idx, time)`. Easy to confuse.
**How to avoid:** For the "remove" action in set_keyframe: use `track_find_key()` to get the key index first, then `track_remove_key()`. Or use `track_remove_key_at_time()` directly.

### Pitfall 6: Ref<> Resource Lifecycle
**What goes wrong:** AnimationLibrary or Animation gets freed prematurely
**Why it happens:** Ref<> is reference-counted. If not held by anything (not added to a player), it gets freed.
**How to avoid:** Create the Ref<>, add it to the parent immediately: `library->add_animation(name, anim)` then `player->add_animation_library("", library)`. The player keeps the reference alive.
**Warning signs:** Null reference crashes when trying to access animation later.

## Code Examples

### Example 1: create_animation (complete flow)
```cpp
// Source: godot-cpp animation.hpp, animation_library.hpp, animation_mixer.hpp
nlohmann::json create_animation(const std::string& player_path,
                                 const std::string& animation_name,
                                 const std::string& parent_path,
                                 const std::string& node_name,
                                 EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) return {{"error", "No scene open"}};
    if (!undo_redo) return {{"error", "UndoRedo not available"}};

    // If player_path is provided, look up existing player
    // Otherwise create new AnimationPlayer node at parent_path
    AnimationPlayer* player = nullptr;

    if (!player_path.empty()) {
        // Use existing AnimationPlayer
        Node* node = scene_root->get_node_or_null(NodePath(player_path.c_str()));
        player = node ? Object::cast_to<AnimationPlayer>(node) : nullptr;
        if (!player) return {{"error", "AnimationPlayer not found: " + player_path}};
    } else {
        // Create new AnimationPlayer (follows scene_mutation.cpp create_node pattern)
        Variant instance = ClassDB::instantiate(StringName("AnimationPlayer"));
        player = Object::cast_to<AnimationPlayer>(instance.operator Object*());
        if (!player) return {{"error", "Failed to instantiate AnimationPlayer"}};

        // Set name
        player->set_name(String(node_name.empty() ? "AnimationPlayer" : node_name.c_str()));

        // Find parent
        Node* parent = scene_root;
        if (!parent_path.empty()) {
            parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
            if (!parent) { memdelete(player); return {{"error", "Parent not found: " + parent_path}}; }
        }

        // UndoRedo: add node to scene tree
        undo_redo->create_action(String("MCP: Create AnimationPlayer"));
        undo_redo->add_do_method(parent, "add_child", player, true);
        undo_redo->add_do_method(player, "set_owner", scene_root);
        undo_redo->add_do_reference(player);
        undo_redo->add_undo_method(parent, "remove_child", player);
        undo_redo->commit_action();
    }

    // Ensure default library exists
    StringName lib_name("");
    if (!player->has_animation_library(lib_name)) {
        Ref<AnimationLibrary> library;
        library.instantiate();
        player->add_animation_library(lib_name, library);
    }

    // Create animation
    Ref<AnimationLibrary> lib = player->get_animation_library(lib_name);
    StringName anim_sn(animation_name.c_str());
    if (lib->has_animation(anim_sn)) {
        return {{"error", "Animation already exists: " + animation_name}};
    }

    Ref<Animation> anim;
    anim.instantiate();
    anim->set_length(1.0);  // 1 second default
    lib->add_animation(anim_sn, anim);

    return {{"success", true},
            {"player_path", /* computed path */},
            {"animation_name", animation_name}};
}
```

### Example 2: add_animation_track
```cpp
// Source: godot-cpp animation.hpp
nlohmann::json add_animation_track(const std::string& player_path,
                                    const std::string& animation_name,
                                    const std::string& track_type_str,
                                    const std::string& track_path) {
    auto lookup = lookup_animation_player(player_path);
    if (!lookup.success) return lookup.error;

    Ref<Animation> anim = find_animation(lookup.player, animation_name);
    if (!anim.is_valid()) {
        return {{"error", "Animation not found: " + animation_name}};
    }

    auto it = track_type_map.find(track_type_str);
    if (it == track_type_map.end()) {
        return {{"error", "Unknown track type: " + track_type_str +
                          ". Valid: value, position_3d, rotation_3d, scale_3d"}};
    }

    int track_idx = anim->add_track(it->second);
    anim->track_set_path(track_idx, NodePath(track_path.c_str()));

    return {{"success", true},
            {"track_index", track_idx},
            {"track_type", track_type_str},
            {"track_path", track_path}};
}
```

### Example 3: set_keyframe (insert action)
```cpp
// Source: godot-cpp animation.hpp
// For TYPE_VALUE: use generic track_insert_key()
// For TYPE_POSITION_3D: use position_track_insert_key() with Vector3
// For TYPE_ROTATION_3D: use rotation_track_insert_key() with Quaternion
// For TYPE_SCALE_3D: use scale_track_insert_key() with Vector3

Animation::TrackType type = anim->track_get_type(track_index);
Variant value = parse_variant(value_str, nullptr, "");

switch (type) {
    case Animation::TYPE_VALUE:
        anim->track_insert_key(track_index, time, value, transition);
        break;
    case Animation::TYPE_POSITION_3D:
        anim->position_track_insert_key(track_index, time, Vector3(value));
        break;
    case Animation::TYPE_ROTATION_3D:
        anim->rotation_track_insert_key(track_index, time, Quaternion(value));
        break;
    case Animation::TYPE_SCALE_3D:
        anim->scale_track_insert_key(track_index, time, Vector3(value));
        break;
}
```

### Example 4: get_animation_info (query structure)
```cpp
// Source: godot-cpp animation_mixer.hpp, animation.hpp
// Query result structure:
// {
//   "success": true,
//   "player_path": "AnimationPlayer",
//   "animations": [
//     {
//       "name": "idle",
//       "length": 1.0,
//       "loop_mode": "linear",
//       "step": 0.1,
//       "track_count": 2,
//       "tracks": [
//         {
//           "index": 0,
//           "type": "position_3d",
//           "path": "Sprite:position",
//           "key_count": 3,
//           "keys": [
//             {"time": 0.0, "value": "Vector3(0, 0, 0)", "transition": 1.0},
//             {"time": 0.5, "value": "Vector3(100, 0, 0)", "transition": 1.0},
//             {"time": 1.0, "value": "Vector3(0, 0, 0)", "transition": 1.0}
//           ]
//         }
//       ]
//     }
//   ]
// }
```

### Example 5: Value Serialization with var_to_str
```cpp
// Source: godot-cpp utility_functions.hpp
#include <godot_cpp/variant/utility_functions.hpp>

// Convert any Variant to string representation for JSON
static std::string variant_to_string(const Variant& val) {
    String godot_str = UtilityFunctions::var_to_str(val);
    return std::string(godot_str.utf8().get_data());
}
// Round-trips: var_to_str(Vector3(1,2,3)) -> "Vector3(1, 2, 3)"
//              str_to_var("Vector3(1, 2, 3)") -> Vector3(1, 2, 3)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Single animation dict on AnimationPlayer | AnimationLibrary system (AnimationPlayer -> AnimationLibrary -> Animation) | Godot 4.0 | Must use library->add_animation(), not direct player methods |
| AnimationPlayer as standalone | AnimationPlayer extends AnimationMixer | Godot 4.0 | Library management methods are on AnimationMixer base class |
| No position/rotation/scale specific methods | Dedicated typed insert methods (position_track_insert_key etc.) | Godot 4.0+ | Prefer typed methods for 3D tracks |

**Deprecated/outdated:**
- In Godot 3.x, animations were directly on AnimationPlayer. In 4.x, they go through AnimationLibrary.
- AnimationPlayer.add_animation() no longer exists; use AnimationLibrary.add_animation() + AnimationMixer.add_animation_library().

## Open Questions

1. **Track identification: index vs path in set_keyframe**
   - What we know: Tracks can be identified by index (integer) or by path. Index is simpler but fragile if tracks are reordered.
   - What's unclear: Whether the AI will more naturally use track_index or node_path to identify tracks
   - Recommendation: Use `track_index` as the primary identifier (returned by add_animation_track). It's unambiguous and stable within a session. Accept optional `track_path` as alternative lookup via `find_track()`.

2. **Transition type for keyframes**
   - What we know: `track_insert_key()` takes a `float transition` parameter (default 1.0). The transition value controls easing between keys. Value of 1.0 = linear, <1 = ease-in, >1 = ease-out.
   - What's unclear: Whether to expose as numeric float or as named string ("linear", "ease_in", "ease_out")
   - Recommendation: Accept optional `transition` as a float parameter (default 1.0). This is the most direct mapping to the Godot API. Named transitions would need a mapping layer.

3. **create_animation: new player vs existing player**
   - What we know: CONTEXT.md says "Creates AnimationPlayer node + AnimationLibrary + named Animation resource. One-step complete setup."
   - What's unclear: Should create_animation also support adding an animation to an EXISTING AnimationPlayer?
   - Recommendation: Support both via optional `player_path`. If provided, use existing player. If omitted, create new AnimationPlayer.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest (unit) + Python UAT (integration) |
| Config file | tests/CMakeLists.txt (unit), tests/uat_phase8.py (integration) |
| Quick run command | `cd tests/build && cmake --build . && ctest --output-on-failure` |
| Full suite command | `cd tests/build && cmake --build . && ctest --output-on-failure` + `python tests/uat_phase8.py` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ANIM-01 | Create AnimationPlayer + library + animation | UAT | `python tests/uat_phase8.py` | No - Wave 0 |
| ANIM-02 | Add typed tracks to animation | UAT | `python tests/uat_phase8.py` | No - Wave 0 |
| ANIM-03 | Insert/update/remove keyframes | UAT | `python tests/uat_phase8.py` | No - Wave 0 |
| ANIM-04 | Query animation info (list, tracks, keyframes) | UAT | `python tests/uat_phase8.py` | No - Wave 0 |
| ANIM-05 | Set animation properties (duration, loop, step) | UAT | `python tests/uat_phase8.py` | No - Wave 0 |
| ALL | Tool registry has 34 tools with correct schemas | Unit | `cd tests/build && ctest -R ToolRegistry --output-on-failure` | No - Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && cmake --build . && ctest --output-on-failure`
- **Per wave merge:** Full unit suite + UAT
- **Phase gate:** Full suite green before verify-work

### Wave 0 Gaps
- [ ] `tests/test_tool_registry.cpp` -- update from 29 to 34 tools, add schema validation for 5 new tools
- [ ] `tests/uat_phase8.py` -- new file following uat_phase7.py pattern, covers ANIM-01 through ANIM-05
- [ ] No new framework install needed -- GoogleTest and Python UAT infrastructure already exist

## Sources

### Primary (HIGH confidence)
- `godot-cpp/gen/include/godot_cpp/classes/animation.hpp` -- verified all Animation methods, enums (TrackType, LoopMode, InterpolationType, FindMode)
- `godot-cpp/gen/include/godot_cpp/classes/animation_player.hpp` -- verified AnimationPlayer inherits AnimationMixer, play/stop/seek methods
- `godot-cpp/gen/include/godot_cpp/classes/animation_mixer.hpp` -- verified add_animation_library, get_animation_library, get_animation_list, has_animation methods
- `godot-cpp/gen/include/godot_cpp/classes/animation_library.hpp` -- verified add_animation, get_animation, has_animation, get_animation_list methods
- `src/ui_tools.h` + `src/ui_tools.cpp` -- module pattern reference (Phase 7)
- `src/scene_mutation.cpp` -- UndoRedo create_node pattern
- `src/variant_parser.cpp` -- parse_variant() for value parsing
- `src/mcp_tool_registry.cpp` -- 29 existing tool definitions
- `src/mcp_server.cpp` -- handle_request dispatch pattern

### Secondary (MEDIUM confidence)
- [Godot 4.3 AnimationLibrary docs](https://docs.godotengine.org/en/4.3/classes/class_animationlibrary.html) -- Library naming convention verified
- [Godot 4.3 AnimationMixer docs](https://docs.godotengine.org/en/4.3/classes/class_animationmixer.html) -- get_animation_list naming convention: default library returns "anim_name", named returns "lib_name/anim_name"
- [Godot Animation stable docs](https://docs.godotengine.org/en/stable/classes/class_animation.html) -- Track types and methods reference

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all needed classes verified in godot-cpp headers, pattern follows established Phase 7 module approach
- Architecture: HIGH -- 5 tools map cleanly to 5 functions, Godot API confirmed in headers, UndoRedo pattern established
- Pitfalls: HIGH -- verified through actual API review (track_remove_key index vs time, Quaternion for rotation, library naming)

**Research date:** 2026-03-18
**Valid until:** 2026-04-18 (stable - godot-cpp API unlikely to change)
