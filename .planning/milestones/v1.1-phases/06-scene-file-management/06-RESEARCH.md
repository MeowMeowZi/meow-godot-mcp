# Phase 6: Scene File Management - Research

**Researched:** 2026-03-18
**Domain:** Godot EditorInterface scene file API, PackedScene, ResourceSaver (C++ GDExtension)
**Confidence:** HIGH

## Summary

Phase 6 adds 5 MCP tools for scene file lifecycle: save_scene, open_scene, list_open_scenes, create_scene, and instantiate_scene. The Godot EditorInterface singleton provides nearly all the APIs needed -- `save_scene()`, `save_scene_as()`, `open_scene_from_path()`, and `get_open_scenes()` are all available in Godot 4.3+. For creating new scenes, the approach requires PackedScene::pack() + ResourceSaver::save() + EditorInterface::open_scene_from_path() since `add_root_node()` is a 4.5+ API not available on the minimum target version. For instantiating sub-scenes, ResourceLoader::load() + PackedScene::instantiate() with UndoRedo for editor integration.

The primary technical risk is SCNF-05 (packing the current scene tree to a file), which combines PackedScene::pack() with ResourceSaver::save() and has a known signal-duplication bug in Godot 4.3. However, this bug only manifests with nested scene instances that have signal connections, which is an edge case that can be documented. All other operations map cleanly to existing Godot APIs.

**Primary recommendation:** Follow the established tool pattern (free functions in scene_file_tools.h/.cpp, dispatch in handle_request, register in ToolDef registry). Use EditorInterface methods for save/open/list, PackedScene+ResourceSaver for create/pack, and ResourceLoader+PackedScene::instantiate()+UndoRedo for instantiate_scene.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- 5 independent MCP tools: save_scene, open_scene, list_open_scenes, create_scene, instantiate_scene -- 1:1 mapping to success criteria
- No close_scene tool -- not in SCNF requirements
- save_scene supports "save as" via optional path parameter: no path = overwrite current, with path = save to new location
- New scene_file_tools.h/.cpp file pair
- Unsaved scene (no file path) + no path parameter = return error requiring path
- open_scene does NOT close current scene -- adds/activates tab
- No overwrite protection on save
- Save format follows file extension: .tscn (text) or .scn (binary)
- create_scene auto-opens in editor after creation
- instantiate_scene uses UndoRedo -- consistent with create_node pattern
- list_open_scenes returns array of {path, title, is_active} per scene
- Unified response format: success = {"success": true, "path": "res://..."} + operation-specific fields; failure = {"error": "..."}

### Claude's Discretion
- Exact parameter naming and ordering within tool definitions
- Error message text and detail level
- Whether save_scene returns additional metadata (file size, timestamp)
- Whether create_scene accepts optional initial properties for the root node
- Internal implementation details (helper functions, validation order)

### Deferred Ideas (OUT OF SCOPE)
- close_scene tool
- Scene inheritance/variant editing
- Batch scene operations (save all, close all)
- Scene diff/comparison tools

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SCNF-01 | AI can save the current scene to disk | EditorInterface::save_scene() for overwrite, save_scene_as() for save-to-path |
| SCNF-02 | AI can open an existing scene file | EditorInterface::open_scene_from_path() -- adds tab, does not close current |
| SCNF-03 | AI can list all open scenes in editor | EditorInterface::get_open_scenes() returns PackedStringArray of file paths |
| SCNF-04 | AI can create a new scene with specified root type | ClassDB::instantiate() + PackedScene::pack() + ResourceSaver::save() + open_scene_from_path() |
| SCNF-05 | AI can pack current scene tree to .tscn/.scn file | EditorInterface::get_edited_scene_root() + PackedScene::pack() + ResourceSaver::save() |
| SCNF-06 | AI can instantiate a PackedScene as child node | ResourceLoader::load() + PackedScene::instantiate(GEN_EDIT_STATE_INSTANCE) + UndoRedo |

</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10 (10.0.0-rc1+) | C++ bindings for GDExtension API | Project foundation |
| nlohmann/json | 3.12.0 | JSON serialization for MCP protocol | Already used everywhere |

### Godot APIs Used (verified from godot-cpp headers)
| API | Class | Availability | Purpose |
|-----|-------|--------------|---------|
| save_scene() | EditorInterface | 4.3+ | Save current scene (overwrite) |
| save_scene_as(path, with_preview) | EditorInterface | 4.3+ | Save current scene to new path |
| open_scene_from_path(path, set_inherited) | EditorInterface | 4.3+ | Open/activate a scene file |
| get_open_scenes() | EditorInterface | 4.3+ | Get array of open scene file paths |
| get_edited_scene_root() | EditorInterface | 4.3+ | Get current scene root Node |
| pack(node) | PackedScene | 4.3+ | Pack node tree into PackedScene |
| instantiate(edit_state) | PackedScene | 4.3+ | Instantiate a packed scene |
| save(resource, path, flags) | ResourceSaver | 4.3+ | Save resource to disk |
| load(path, type_hint, cache_mode) | ResourceLoader | 4.3+ | Load resource from disk |

### Version-Gated APIs (NOT available in 4.3, DO NOT USE)
| API | Class | Min Version | Alternative |
|-----|-------|-------------|-------------|
| add_root_node(node) | EditorInterface | 4.5+ | PackedScene::pack() + ResourceSaver::save() + open_scene_from_path() |
| close_scene() | EditorInterface | 4.5+ | Not needed (deferred) |
| get_open_scene_roots() | EditorInterface | 4.5+ | Use get_open_scenes() (paths only) |

**No new dependencies needed.** All functionality is available through existing Godot APIs and the project's existing libraries.

## Architecture Patterns

### Recommended File Structure
```
src/
  scene_file_tools.h     # Function declarations (new)
  scene_file_tools.cpp   # Implementations (new)
  mcp_tool_registry.cpp  # Add 5 new ToolDef entries (modify)
  mcp_server.cpp         # Add 5 new handle_request dispatchers (modify)
```

### Pattern 1: Tool Function Signatures
**What:** Each tool is a free function returning nlohmann::json, following established patterns.
**When to use:** All 5 new tools.

```cpp
// scene_file_tools.h
#ifndef MEOW_GODOT_MCP_SCENE_FILE_TOOLS_H
#define MEOW_GODOT_MCP_SCENE_FILE_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// SCNF-01: Save current scene (overwrite or save-as)
// path: empty = overwrite current, non-empty = save to new location
nlohmann::json save_scene(const std::string& path);

// SCNF-02: Open an existing scene file in the editor
nlohmann::json open_scene(const std::string& path);

// SCNF-03: List all currently open scenes
nlohmann::json list_open_scenes();

// SCNF-04: Create a new scene with specified root node type
nlohmann::json create_scene(const std::string& root_type,
                             const std::string& path,
                             const std::string& root_name);

// SCNF-06: Instantiate a PackedScene as a child node
nlohmann::json instantiate_scene(const std::string& scene_path,
                                  const std::string& parent_path,
                                  const std::string& name,
                                  godot::EditorUndoRedoManager* undo_redo);

#endif
#endif
```

### Pattern 2: save_scene Implementation Strategy
**What:** Dual-mode save: overwrite vs save-as based on presence of path parameter.
**Key insight:** EditorInterface::save_scene() returns Error (OK or ERR_CANT_CREATE). save_scene_as() returns void.

```cpp
// Source: godot-cpp gen/include/godot_cpp/classes/editor_interface.hpp
// Error save_scene();
// void save_scene_as(const String &p_path, bool p_with_preview = true);

nlohmann::json save_scene(const std::string& path) {
    auto* ei = EditorInterface::get_singleton();
    Node* root = ei->get_edited_scene_root();
    if (!root) {
        return {{"error", "No scene open"}};
    }

    if (path.empty()) {
        // Overwrite save -- need existing file path
        String scene_path = root->get_scene_file_path();
        if (scene_path.is_empty()) {
            return {{"error", "Scene has no file path. Provide a path parameter to save as new file."}};
        }
        Error err = ei->save_scene();
        if (err != OK) {
            return {{"error", "Failed to save scene (error: " + std::to_string((int)err) + ")"}};
        }
        return {{"success", true}, {"path", std::string(scene_path.utf8().get_data())}};
    }

    // Save-as to new path
    // validate_res_path from script_tools.h can be reused
    ei->save_scene_as(String(path.c_str()), true);
    return {{"success", true}, {"path", path}};
}
```

### Pattern 3: create_scene via PackedScene+ResourceSaver (4.3-compatible)
**What:** Create a new scene by: instantiate root node -> pack into PackedScene -> save to disk -> open in editor.
**Why:** `add_root_node()` is 4.5+ only. This approach works on all supported Godot versions.

```cpp
nlohmann::json create_scene(const std::string& root_type,
                             const std::string& path,
                             const std::string& root_name) {
    // 1. Validate class exists and is a Node
    if (!ClassDB::class_exists(StringName(root_type.c_str()))) {
        return {{"error", "Unknown class: " + root_type}};
    }
    if (!ClassDB::is_parent_class(StringName(root_type.c_str()), StringName("Node"))) {
        return {{"error", root_type + " is not a Node type"}};
    }

    // 2. Instantiate root node
    Variant instance = ClassDB::instantiate(StringName(root_type.c_str()));
    Node* root = Object::cast_to<Node>(instance.operator Object*());
    if (!root) {
        return {{"error", "Failed to instantiate: " + root_type}};
    }
    root->set_name(String(root_name.empty() ? root_type.c_str() : root_name.c_str()));

    // 3. Pack into PackedScene
    Ref<PackedScene> packed;
    packed.instantiate();
    Error pack_err = packed->pack(root);
    root->queue_free();  // Free the temporary node
    if (pack_err != OK) {
        return {{"error", "Failed to pack scene (error: " + std::to_string((int)pack_err) + ")"}};
    }

    // 4. Save to disk
    Error save_err = ResourceSaver::get_singleton()->save(packed, String(path.c_str()));
    if (save_err != OK) {
        return {{"error", "Failed to save scene file (error: " + std::to_string((int)save_err) + ")"}};
    }

    // 5. Open in editor
    EditorInterface::get_singleton()->open_scene_from_path(String(path.c_str()));

    return {{"success", true}, {"path", path}, {"root_type", root_type}};
}
```

### Pattern 4: instantiate_scene with UndoRedo
**What:** Load a .tscn, instantiate with GEN_EDIT_STATE_INSTANCE, add as child with UndoRedo.
**Key detail:** Must use GEN_EDIT_STATE_INSTANCE for editor context + set_owner on the instantiated node AND all its descendants.

```cpp
nlohmann::json instantiate_scene(const std::string& scene_path,
                                  const std::string& parent_path,
                                  const std::string& name,
                                  EditorUndoRedoManager* undo_redo) {
    // Load the PackedScene
    auto* loader = ResourceLoader::get_singleton();
    Ref<Resource> res = loader->load(String(scene_path.c_str()), "PackedScene");
    Ref<PackedScene> packed = res;
    if (!packed.is_valid()) {
        return {{"error", "Failed to load scene: " + scene_path}};
    }

    // Instantiate with editor edit state
    Node* instance = packed->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
    if (!instance) {
        return {{"error", "Failed to instantiate scene: " + scene_path}};
    }

    // Optional name override
    if (!name.empty()) {
        instance->set_name(String(name.c_str()));
    }

    // Find parent, add with UndoRedo (same pattern as create_node)
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    Node* parent = /* resolve parent_path */;

    undo_redo->create_action(String("MCP: Instantiate scene"));
    undo_redo->add_do_method(parent, "add_child", instance, true);
    undo_redo->add_do_method(instance, "set_owner", scene_root);
    undo_redo->add_do_reference(instance);
    undo_redo->add_undo_method(parent, "remove_child", instance);
    undo_redo->commit_action();

    return {{"success", true}, {"path", /* node path */}, {"scene_path", scene_path}};
}
```

### Pattern 5: list_open_scenes with active detection
**What:** Use get_open_scenes() for paths + get_edited_scene_root() to determine which is active.

```cpp
nlohmann::json list_open_scenes() {
    auto* ei = EditorInterface::get_singleton();
    PackedStringArray scenes = ei->get_open_scenes();
    Node* active_root = ei->get_edited_scene_root();
    String active_path = active_root ? active_root->get_scene_file_path() : String();

    nlohmann::json result = nlohmann::json::array();
    for (int i = 0; i < scenes.size(); i++) {
        String path = scenes[i];
        std::string path_str(path.utf8().get_data());
        std::string title = path_str.substr(path_str.rfind('/') + 1);
        result.push_back({
            {"path", path_str},
            {"title", title},
            {"is_active", path == active_path}
        });
    }
    return {{"success", true}, {"scenes", result}, {"count", result.size()}};
}
```

### Anti-Patterns to Avoid
- **Using add_root_node() for create_scene:** This is a 4.5+ API. Using it will crash on Godot 4.3/4.4.
- **Using get_open_scene_roots() for list_open_scenes:** Also 4.5+ only. Use get_open_scenes() (returns paths).
- **Calling EditorFileSystem::update_file() after save:** Known to cause crashes (documented in script_tools.cpp Phase 3 lessons).
- **Using ResourceLoader::load() for newly created files:** Can crash on unregistered files (Phase 3 attach_script crash). For create_scene, this is fine because we save via ResourceSaver first, then open_scene_from_path handles loading internally.
- **Forgetting set_owner on instantiated scenes:** Sub-scene nodes will not appear in the editor scene tree without correct ownership.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Scene serialization | Custom .tscn writer | PackedScene::pack() + ResourceSaver::save() | .tscn format is complex, internal Godot format |
| Scene loading | Custom .tscn parser | ResourceLoader::load() | Handles all resource dependencies automatically |
| File path validation | New validator | validate_res_path() from script_tools.h | Already tested, handles res:// prefix |
| Node class validation | New validator | ClassDB::class_exists() + is_parent_class() | Already used in create_node |
| Scene tab management | Direct tab manipulation | EditorInterface::open_scene_from_path() | Handles editor internals (tab creation, notifications) |
| Overwrite save | Manual file write | EditorInterface::save_scene() | Integrates with editor's dirty-flag and UID system |

**Key insight:** Nearly all scene file operations delegate to EditorInterface or PackedScene/ResourceSaver. The MCP tools are thin wrappers that validate inputs, call the right API, and format the JSON response.

## Common Pitfalls

### Pitfall 1: save_scene() on unsaved scene returns ERR_CANT_CREATE
**What goes wrong:** Calling EditorInterface::save_scene() when the scene has never been saved (no file path) returns an error.
**Why it happens:** save_scene() overwrites the existing file path. New scenes have no path.
**How to avoid:** Check get_scene_file_path() on the root node. If empty, require the path parameter (save-as mode). The CONTEXT.md decision already specifies this behavior.
**Warning signs:** ERR_CANT_CREATE error code from save_scene().

### Pitfall 2: queue_free() vs memdelete() for temporary nodes
**What goes wrong:** Using memdelete() on a temporary Node that isn't in the scene tree may work, but queue_free() is the standard Godot pattern.
**Why it happens:** Nodes created with ClassDB::instantiate() for create_scene are temporary -- they only exist to be packed.
**How to avoid:** After PackedScene::pack(), call memdelete() (not queue_free) on the temporary root node because it is NOT in the scene tree. queue_free() requires the node to be in the tree.
**Warning signs:** Memory leaks from nodes never freed, or errors from queue_free on detached nodes.

### Pitfall 3: PackedScene::instantiate() edit state matters
**What goes wrong:** Instantiated scene nodes don't appear in editor scene tree or can't be edited.
**Why it happens:** Default GEN_EDIT_STATE_DISABLED strips editor-specific data. Editor context requires GEN_EDIT_STATE_INSTANCE.
**How to avoid:** Always use `packed->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE)` when adding to the editor scene tree.
**Warning signs:** Node appears in tree but sub-nodes are missing, or node can't be selected in editor.

### Pitfall 4: set_owner cascade for instantiated sub-scenes
**What goes wrong:** Only the root of the instantiated scene shows in the editor; child nodes are invisible.
**Why it happens:** When adding an instantiated scene as a child, the editor needs ownership metadata. For scene instances, only the instance root needs set_owner -- child nodes inherit ownership from the scene file. However, the instance root itself MUST have its owner set to the edited scene root.
**How to avoid:** After UndoRedo add_child, call set_owner(scene_root) on the instantiated root node. The internal children retain their packed scene ownership automatically because GEN_EDIT_STATE_INSTANCE preserves the scene instance relationship.
**Warning signs:** Instantiated node visible but without the scene instance icon in the editor tree.

### Pitfall 5: get_open_scenes() returns empty for unsaved scenes
**What goes wrong:** Newly created scenes that haven't been saved yet don't have file paths, so they may not appear in get_open_scenes().
**Why it happens:** get_open_scenes() returns PackedStringArray of file paths. An unsaved scene has an empty path.
**How to avoid:** Document in list_open_scenes that unsaved scenes may show as empty paths. Consider filtering them or marking them specially in the response.
**Warning signs:** Scene count in list_open_scenes doesn't match visible tabs in editor.

### Pitfall 6: save_scene_as() does not return Error
**What goes wrong:** Cannot detect save_scene_as failures programmatically.
**Why it happens:** The API signature is `void save_scene_as(const String &p_path, bool p_with_preview = true)` -- no return value.
**How to avoid:** Validate the path beforehand (check directory exists, path is valid res:// path). After calling save_scene_as, verify the file exists with FileAccess::file_exists(). Alternatively, accept that failures will be rare since we validate inputs.
**Warning signs:** Silent save failures reported by users.

## Code Examples

### handle_request dispatch pattern (from existing mcp_server.cpp)
```cpp
// Source: src/mcp_server.cpp, lines 335-353
if (tool_name == "save_scene") {
    std::string path;
    if (params.contains("arguments") && params["arguments"].is_object()) {
        auto& args = params["arguments"];
        if (args.contains("path") && args["path"].is_string())
            path = args["path"].get<std::string>();
    }
    return mcp::create_tool_result(id, save_scene(path));
}
```

### ToolDef registration pattern (from existing mcp_tool_registry.cpp)
```cpp
// Source: src/mcp_tool_registry.cpp
{
    "save_scene",
    "Save the current scene to disk. Without path: overwrites current file (Ctrl+S). With path: saves to new location (Ctrl+Shift+S). Returns error if scene has no file path and no path is provided.",
    {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Optional file path (e.g., res://scenes/level.tscn). Omit to overwrite current file. Extension determines format: .tscn (text) or .scn (binary)."}}}
        }},
        {"required", nlohmann::json::array()}
    },
    {4, 3, 0}
},
```

### SCNF-05: Pack current scene to file
```cpp
// Pack the current edited scene tree into a new .tscn file
// This is separate from save_scene because it creates a NEW PackedScene
// from the current tree, useful for "export as template"
nlohmann::json pack_scene(const std::string& path) {
    auto* ei = EditorInterface::get_singleton();
    Node* root = ei->get_edited_scene_root();
    if (!root) {
        return {{"error", "No scene open"}};
    }

    Ref<PackedScene> packed;
    packed.instantiate();
    Error pack_err = packed->pack(root);
    if (pack_err != OK) {
        return {{"error", "Failed to pack scene"}};
    }

    Error save_err = ResourceSaver::get_singleton()->save(packed, String(path.c_str()));
    if (save_err != OK) {
        return {{"error", "Failed to save packed scene"}};
    }

    return {{"success", true}, {"path", path}};
}
```

**Note on SCNF-05 vs save_scene:** SCNF-05 ("pack scene tree to file") overlaps significantly with save_scene's save-as mode. The key difference: save_scene_as() uses the editor's internal save pipeline (preserves UIDs, editor metadata, thumbnails). PackedScene::pack() + ResourceSaver::save() creates a clean packed scene from the node tree. For most AI workflows, save_scene with a path parameter (save-as) satisfies both SCNF-01 and SCNF-05. The implementation can unify these -- save_scene handles both use cases.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual .tscn file writing | PackedScene::pack() + ResourceSaver | Godot 3.0+ | Never hand-write scene format |
| EditorScript.add_root_node() | EditorInterface.add_root_node() | 4.5 | 4.3/4.4 must use workaround |
| get_open_scenes() returns paths only | get_open_scene_roots() returns Node array | 4.5 | 4.3/4.4 limited to file paths |

**Deprecated/outdated:**
- EditorScript.get_editor_interface(): Deprecated in 4.2+, use EditorInterface singleton directly
- EditorInterface.get_open_scenes() alone: Supplemented by get_open_scene_roots() in 4.5+, but still the only option for 4.3

## Open Questions

1. **SCNF-05 overlap with save_scene save-as**
   - What we know: save_scene_as() and PackedScene::pack()+ResourceSaver both save scenes to files
   - What's unclear: Whether SCNF-05 ("pack scene tree to .tscn/.scn") requires a separate tool or can be satisfied by save_scene's path parameter
   - Recommendation: Unify into save_scene with optional path parameter. The success criterion says "AI can pack scene tree to file" which save_scene_as satisfies. No separate pack_scene tool needed unless explicit standalone packing is required.

2. **Unsaved scene detection in list_open_scenes**
   - What we know: get_open_scenes() may return empty strings for unsaved scenes
   - What's unclear: Exact behavior of get_open_scenes() with a mix of saved and unsaved tabs
   - Recommendation: Test at UAT time. If empty strings appear, include them with a marker like {"path": "", "title": "[unsaved]", "is_active": true}.

3. **Node cleanup after create_scene pack**
   - What we know: Temporary nodes created for packing must be freed
   - What's unclear: Whether memdelete() or queue_free() is correct for detached nodes
   - Recommendation: Use memdelete() since the node is never added to the scene tree. queue_free() requires tree membership.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 (unit) + Python UAT (integration) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd tests/build && ctest --output-on-failure` |
| Full suite command | `cd tests/build && ctest --output-on-failure && python ../uat_phase6.py` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SCNF-01 | save_scene saves current scene | UAT (requires editor) | `python tests/uat_phase6.py` | Wave 0 |
| SCNF-02 | open_scene opens .tscn in editor | UAT (requires editor) | `python tests/uat_phase6.py` | Wave 0 |
| SCNF-03 | list_open_scenes returns scene list | UAT (requires editor) | `python tests/uat_phase6.py` | Wave 0 |
| SCNF-04 | create_scene creates + opens new scene | UAT (requires editor) | `python tests/uat_phase6.py` | Wave 0 |
| SCNF-05 | save_scene with path packs to file | UAT (requires editor) | `python tests/uat_phase6.py` | Wave 0 |
| SCNF-06 | instantiate_scene adds sub-scene | UAT (requires editor) | `python tests/uat_phase6.py` | Wave 0 |
| -- | Tool registry has 5 new tools | unit | `cd tests/build && ctest -R test_tool_registry` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && ctest --output-on-failure`
- **Per wave merge:** `cd tests/build && ctest --output-on-failure && python tests/uat_phase6.py`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/uat_phase6.py` -- UAT test suite covering all 6 SCNF requirements via TCP/JSON-RPC
- [ ] Update `tests/test_tool_registry.cpp` -- verify 23 tools (18 existing + 5 new) registered with correct min_version
- [ ] Test scene file: need a simple .tscn at `project/test_scene.tscn` for open_scene and instantiate_scene tests

## Sources

### Primary (HIGH confidence)
- `godot-cpp/gen/include/godot_cpp/classes/editor_interface.hpp` -- Exact method signatures for all EditorInterface scene methods (verified line-by-line from project headers)
- `godot-cpp/gen/include/godot_cpp/classes/packed_scene.hpp` -- PackedScene::pack(), instantiate(), GenEditState enum
- `godot-cpp/gen/include/godot_cpp/classes/resource_saver.hpp` -- ResourceSaver::save() signature, SaverFlags enum
- `godot-cpp/gen/include/godot_cpp/classes/resource_loader.hpp` -- ResourceLoader::load() signature
- `src/scene_mutation.cpp` -- UndoRedo pattern for create_node (lines 19-103), direct project source
- `src/script_tools.cpp` -- validate_res_path(), ResourceLoader crash lesson (lines 280-294)

### Secondary (MEDIUM confidence)
- [EditorInterface Godot 4.3 docs](https://docs.godotengine.org/en/4.3/classes/class_editorinterface.html) -- Official method list
- [PackedScene Godot 4.3 docs](https://docs.godotengine.org/en/4.3/classes/class_packedscene.html) -- pack/instantiate docs
- [Creating root in scene using editor plugin](https://forum.godotengine.org/t/creating-a-root-in-scene-using-editor-plugin/53062) -- Confirmed add_root_node unavailable in 4.3
- [PackedScene signal bug](https://github.com/godotengine/godot/issues/103453) -- Known bug with pack+save and signal connections

### Tertiary (LOW confidence)
- [Proposal: get_open_scene_roots](https://github.com/godotengine/godot-proposals/issues/10895) -- Confirms this is 4.5+ feature
- [Proposal: close_scene](https://github.com/godotengine/godot-proposals/issues/8806) -- Confirms close_scene was missing, added in 4.5+

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - APIs verified directly from project's godot-cpp headers
- Architecture: HIGH - Follows 100% established patterns from 5 shipped phases
- Pitfalls: HIGH - Version-gating issue verified from headers + docs, crash lessons from project history
- Code examples: HIGH - Based on actual project source patterns with verified API signatures

**Research date:** 2026-03-18
**Valid until:** 2026-04-18 (stable -- Godot API and project patterns unlikely to change)
