# Phase 2: Scene CRUD - Research

**Researched:** 2026-03-16
**Domain:** Godot GDExtension C++ -- scene tree manipulation, UndoRedo integration, Variant type parsing
**Confidence:** HIGH

## Summary

Phase 2 implements the core AI-driven scene manipulation tools: creating nodes by class name, modifying node properties, and deleting nodes -- all integrated with the Godot editor's undo/redo system. The key APIs are well-documented and verified against the actual godot-cpp v10 headers in this project.

The main architectural challenge is that `MCPServer` is a plain C++ class (not a Godot Object) and has no access to `EditorPlugin::get_undo_redo()`. The plugin must pass the `EditorUndoRedoManager*` pointer to the server so scene mutation tools can use it. Additionally, every node added to the scene tree must have its `owner` set to the edited scene root -- failure to do this causes nodes to not appear in the editor's Scene panel.

Type parsing (SCNE-06) has a clean solution: Godot's `UtilityFunctions::str_to_var()` handles the `"Vector2(1,2)"` constructor format natively, and `Color::html()` handles hex colors like `"#ff0000"`. A two-layer approach (try `str_to_var` first, fall back to custom parsing for hex colors, bare numbers, and booleans) covers all practical AI input formats.

**Primary recommendation:** Implement three MCP tools (`create_node`, `set_node_property`, `delete_node`) each wrapping EditorUndoRedoManager actions, with a shared Variant type parser module. Keep scene_tools.cpp for read operations, add a new `scene_mutation.h/cpp` for write operations.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SCNE-02 | AI can create new nodes with specified type, parent, and initial properties | `ClassDB::instantiate()` creates nodes by string name; `EditorUndoRedoManager` wraps add_child/set_owner in undo-able actions |
| SCNE-03 | AI can modify node properties (transform, name, visibility, custom) | `Object::set()` + `Object::get()` with StringName property access; `add_do_property`/`add_undo_property` for undo integration |
| SCNE-04 | AI can delete nodes by path | `EditorUndoRedoManager` wraps remove_child; `add_undo_reference` keeps node alive for redo |
| SCNE-05 | All modifications integrated with Godot UndoRedo (Ctrl+Z/Y) | `EditorPlugin::get_undo_redo()` returns `EditorUndoRedoManager*`; verified in godot-cpp headers |
| SCNE-06 | Property values auto-parsed from string format to Godot types | `UtilityFunctions::str_to_var()` + `Color::html()` + property type discovery via `get_property_list()` |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10 (in-tree) | C++ bindings for Godot 4.3+ | Already in project, provides all editor API bindings |
| nlohmann/json | 3.12.0 (in-tree) | JSON parsing for MCP protocol | Already in project, used by protocol layer |

### Key Godot APIs
| API | Header | Purpose |
|-----|--------|---------|
| `ClassDB::instantiate(StringName)` | `class_db_singleton.hpp` | Create any built-in node type by string name |
| `EditorUndoRedoManager` | `editor_undo_redo_manager.hpp` | Undo/redo integration for all scene mutations |
| `EditorPlugin::get_undo_redo()` | `editor_plugin.hpp` | Obtain EditorUndoRedoManager from plugin |
| `Object::set(StringName, Variant)` | `object.hpp` (gen) | Set any property by name |
| `Object::get(StringName)` | `object.hpp` (gen) | Get any property by name |
| `Node::add_child()` | `node.hpp` | Add child node to scene tree |
| `Node::remove_child()` | `node.hpp` | Remove child node from scene tree |
| `Node::set_owner()` | `node.hpp` | Set node owner (CRITICAL for editor visibility) |
| `Node::set_name()` | `node.hpp` | Rename a node |
| `UtilityFunctions::str_to_var()` | `utility_functions.hpp` | Parse "Vector2(1,2)" format strings to Variant |
| `Color::html()` | `color.hpp` | Parse "#ff0000" hex strings to Color |
| `Expression` | `expression.hpp` | Fallback parser for complex expressions |

### No New Dependencies
No new libraries needed. Everything required is already available in godot-cpp.

## Architecture Patterns

### Recommended File Structure
```
src/
  mcp_server.h/cpp       # TCP server + request routing (MODIFY: add new tool handlers)
  mcp_protocol.h/cpp     # JSON-RPC + MCP messages (MODIFY: register new tools in tools/list)
  scene_tools.h/cpp      # Read-only scene queries (EXISTING, unchanged)
  scene_mutation.h/cpp    # NEW: create/modify/delete nodes with undo/redo
  variant_parser.h/cpp   # NEW: string-to-Variant type conversion
  mcp_plugin.h/cpp       # EditorPlugin (MODIFY: pass undo_redo to server)
```

### Pattern 1: MCPPlugin Passes UndoRedo to MCPServer

**What:** MCPServer needs access to `EditorUndoRedoManager*` but is a plain C++ class, not a Godot Object. The plugin must pass this pointer during server construction or via a setter.

**When to use:** All scene mutation tools (create, modify, delete).

**Example:**
```cpp
// In mcp_plugin.cpp (_enter_tree):
server = new MCPServer();
server->set_undo_redo(get_undo_redo());  // EditorUndoRedoManager*
server->start(port);

// In mcp_server.h:
class MCPServer {
    // ...
    void set_undo_redo(godot::EditorUndoRedoManager* ur);
private:
    godot::EditorUndoRedoManager* undo_redo = nullptr;
};
```

### Pattern 2: Node Creation with UndoRedo

**What:** Create a node by class name, add to parent with owner set to scene root, wrapped in undo action.

**When to use:** SCNE-02 (create_node tool).

**Example:**
```cpp
// Source: verified against godot-cpp gen headers
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/editor_interface.hpp>

nlohmann::json create_node(const std::string& type, const std::string& parent_path,
                           const std::string& name, EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) return {{"error", "No scene open"}};

    // Validate class exists and is a Node
    if (!ClassDB::class_exists(StringName(type.c_str()))) {
        return {{"error", "Unknown class: " + type}};
    }
    if (!ClassDB::is_parent_class(StringName(type.c_str()), "Node")) {
        return {{"error", type + " is not a Node type"}};
    }

    // Find parent node
    Node* parent = scene_root;
    if (!parent_path.empty()) {
        parent = scene_root->get_node_or_null(NodePath(parent_path.c_str()));
        if (!parent) return {{"error", "Parent not found: " + parent_path}};
    }

    // Create the node
    Variant instance = ClassDB::instantiate(StringName(type.c_str()));
    Node* new_node = Object::cast_to<Node>(instance.operator Object*());
    if (!new_node) return {{"error", "Failed to instantiate: " + type}};

    new_node->set_name(StringName(name.c_str()));

    // UndoRedo action
    undo_redo->create_action("MCP: Create " + String(type.c_str()));
    undo_redo->add_do_method(parent, "add_child", new_node, true);
    undo_redo->add_do_method(new_node, "set_owner", scene_root);
    undo_redo->add_do_reference(new_node);
    undo_redo->add_undo_method(parent, "remove_child", new_node);
    undo_redo->commit_action();

    return {{"success", true}, {"path", String(scene_root->get_path_to(new_node)).utf8().get_data()}};
}
```

### Pattern 3: Property Modification with UndoRedo

**What:** Set a property on a node, recording old value for undo.

**When to use:** SCNE-03 (set_node_property tool).

**Example:**
```cpp
nlohmann::json set_node_property(const std::string& node_path, const std::string& property,
                                  const std::string& value_str, EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    if (!node) return {{"error", "Node not found: " + node_path}};

    // Parse value string to Variant
    Variant new_value = parse_variant(value_str, node, property);

    // Get old value for undo
    Variant old_value = node->get(StringName(property.c_str()));

    undo_redo->create_action("MCP: Set " + String(property.c_str()));
    undo_redo->add_do_property(node, StringName(property.c_str()), new_value);
    undo_redo->add_undo_property(node, StringName(property.c_str()), old_value);
    undo_redo->commit_action();

    return {{"success", true}};
}
```

### Pattern 4: Node Deletion with UndoRedo

**What:** Remove a node, keeping it alive via `add_undo_reference` for redo.

**When to use:** SCNE-04 (delete_node tool).

**Example:**
```cpp
nlohmann::json delete_node(const std::string& node_path, EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    if (!node) return {{"error", "Node not found: " + node_path}};
    if (node == scene_root) return {{"error", "Cannot delete scene root"}};

    Node* parent = node->get_parent();

    undo_redo->create_action("MCP: Delete " + String(node->get_name()));
    undo_redo->add_do_method(parent, "remove_child", node);
    undo_redo->add_undo_method(parent, "add_child", node, true);
    undo_redo->add_undo_method(node, "set_owner", scene_root);
    undo_redo->add_undo_reference(node);
    undo_redo->commit_action();

    return {{"success", true}};
}
```

### Pattern 5: Variant Type Parsing Strategy

**What:** Convert AI-supplied string values into correct Godot Variant types.

**When to use:** SCNE-06 (all property-setting operations).

**Strategy (two-layer):**

1. **Property-type-aware parsing:** Query the property's expected type via `get_property_list()` or by getting the current value's `Variant::Type`. Use this to guide conversion.
2. **String format parsing:**
   - **Godot constructor format** (e.g., `"Vector2(1, 2)"`): Use `UtilityFunctions::str_to_var()`
   - **Hex color** (e.g., `"#ff0000"`): Use `Color::html()`
   - **Bare numbers** (e.g., `"42"`, `"3.14"`): Parse as int/float based on target property type
   - **Booleans** (e.g., `"true"`, `"false"`): Parse to bool
   - **Plain strings**: Pass through as String

```cpp
Variant parse_variant(const std::string& value_str, Node* node, const std::string& property) {
    String godot_str(value_str.c_str());

    // 1. Try Godot's built-in parser (handles "Vector2(1,2)", "Color(1,0,0,1)", etc.)
    Variant parsed = UtilityFunctions::str_to_var(godot_str);
    if (parsed.get_type() != Variant::NIL || godot_str == "null") {
        return parsed;
    }

    // 2. Try hex color (str_to_var doesn't handle "#ff0000")
    if (value_str.size() > 0 && value_str[0] == '#') {
        if (Color::html_is_valid(godot_str)) {
            return Color::html(godot_str);
        }
    }

    // 3. Try type-aware parsing based on current property type
    Variant current = node->get(StringName(property.c_str()));
    Variant::Type target_type = current.get_type();

    switch (target_type) {
        case Variant::BOOL:
            if (value_str == "true") return Variant(true);
            if (value_str == "false") return Variant(false);
            break;
        case Variant::INT:
            try { return Variant((int64_t)std::stoll(value_str)); } catch (...) {}
            break;
        case Variant::FLOAT:
            try { return Variant(std::stod(value_str)); } catch (...) {}
            break;
        default:
            break;
    }

    // 4. Fallback: return as String
    return Variant(godot_str);
}
```

### Anti-Patterns to Avoid

- **Forgetting `set_owner(scene_root)` after `add_child()`:** Node will be invisible in the editor Scene panel and will not be saved to the .tscn file. This is the most common pitfall.
- **Using `new Node()` instead of `ClassDB::instantiate()`:** Raw `new` can cause `get_class()` to return the base class name instead of the actual class, breaking signal connections and type checks.
- **Setting owner to parent instead of scene root:** Owner must ALWAYS be the edited scene root, not the immediate parent. Nested nodes still point to the root.
- **Not wrapping mutations in UndoRedo:** Any direct scene modification without UndoRedo will break Ctrl+Z and leave the scene in an inconsistent undo state.
- **Assuming property names match API names:** Godot property names use snake_case (e.g., `"position"`, `"rotation_degrees"`, `"visible"`, `"modulate"`) -- these are the names to use with `Object::set()`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Node instantiation by class name | Manual class name -> constructor mapping | `ClassDB::instantiate(StringName)` | Covers all 500+ built-in classes including future ones |
| Variant type parsing from string | Full custom parser for all Godot types | `UtilityFunctions::str_to_var()` | Handles Vector2/3/4, Color, Rect2, Transform, AABB, Quaternion, etc. |
| Hex color parsing | Regex-based color parser | `Color::html()` + `Color::html_is_valid()` | Handles #RGB, #RGBA, #RRGGBB, #RRGGBBAA, named colors |
| Class validation | Manual list of Node subclasses | `ClassDB::class_exists()` + `ClassDB::is_parent_class(cls, "Node")` | Always up-to-date with engine version |
| Property type discovery | Hardcoded type maps per class | `Object::get()` to read current value type, or `ClassDB::class_get_property_list()` | Works for any class including user-defined ones |

**Key insight:** Godot's ClassDB and Variant systems provide comprehensive runtime reflection. Type parsing should leverage `str_to_var` for the heavy lifting, with a thin custom layer only for formats it does not handle (hex colors, bare numbers).

## Common Pitfalls

### Pitfall 1: Node Not Visible in Editor After add_child
**What goes wrong:** Node is created and added to the tree, but doesn't appear in the Scene panel.
**Why it happens:** `set_owner(scene_root)` was not called after `add_child()`. Godot uses `owner` to determine which nodes belong to the scene and should be displayed/saved.
**How to avoid:** Always call `set_owner(EditorInterface::get_singleton()->get_edited_scene_root())` immediately after `add_child()`. Both must be recorded in UndoRedo.
**Warning signs:** Node appears in `get_scene_tree` output but not in editor; node disappears on scene save/reload.

### Pitfall 2: ClassDB::instantiate Returns Variant, Not Node*
**What goes wrong:** `ClassDB::instantiate()` returns a `Variant`, not a `Node*`. Direct assignment fails.
**Why it happens:** The ClassDB singleton binding wraps the return as Variant since it can create any Object type.
**How to avoid:** Use `Object::cast_to<Node>(instance.operator Object*())` to safely extract the Node pointer.
**Warning signs:** Compile error on direct assignment; null pointer after incorrect cast.

### Pitfall 3: UndoRedo Method Calls Use String Method Names
**What goes wrong:** Trying to pass C++ function pointers or Callables to `add_do_method`.
**Why it happens:** `EditorUndoRedoManager::add_do_method` takes `Object*` + `StringName` method name + args (variadic template), not Callable.
**How to avoid:** Always use string method names: `undo_redo->add_do_method(parent, "add_child", node, true)`. Verified in godot-cpp header.
**Warning signs:** Compile errors about Callable conversion; runtime "method not found" errors.

### Pitfall 4: UndoRedo and Newly Created Nodes
**What goes wrong:** After undo, the node is freed. After redo, the node pointer is dangling.
**Why it happens:** Without `add_do_reference(node)`, Godot may free the node when the undo stack loses its do-history.
**How to avoid:** Always call `add_do_reference(new_node)` for create actions and `add_undo_reference(node)` for delete actions.
**Warning signs:** Crash on redo after undo; "freed object" errors.

### Pitfall 5: StringName Construction in godot-cpp v10
**What goes wrong:** Passing `std::string` directly to Godot API calls that expect `StringName`.
**Why it happens:** godot-cpp v10 does not have implicit conversion from `const char*` to `StringName` in all contexts.
**How to avoid:** Explicit construction: `StringName(value.c_str())` or `String(value.c_str())`.
**Warning signs:** Compile errors about ambiguous conversions; empty property names at runtime.

### Pitfall 6: Deleting Scene Root
**What goes wrong:** AI tries to delete the scene root node, leaving the editor in a broken state.
**Why it happens:** No validation that the target node is not the scene root.
**How to avoid:** Check `node == scene_root` before allowing deletion. Return an error if attempting to delete the root.
**Warning signs:** Editor crash or blank scene after deletion.

### Pitfall 7: Property Name vs Display Name
**What goes wrong:** AI passes "Position" or "Rotation Degrees" instead of "position" or "rotation_degrees".
**Why it happens:** AI reads display names from the Inspector, which uses formatted names.
**How to avoid:** Document that property names must be Godot's internal snake_case names. Optionally implement a fuzzy name resolver.
**Warning signs:** "Property not found" errors; no visible change after set_node_property.

## Code Examples

### Tool Registration in MCP Protocol

New tools must be registered in `create_tools_list_response()`:

```cpp
// In mcp_protocol.cpp, add to the tools array in create_tools_list_response:
{
    {"name", "create_node"},
    {"description", "Create a new node in the scene tree"},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"type", {{"type", "string"}, {"description", "Node class name (e.g., Sprite2D, CharacterBody3D)"}}},
            {"parent_path", {{"type", "string"}, {"description", "Path to parent node (empty = scene root)"}}},
            {"name", {{"type", "string"}, {"description", "Name for the new node"}}},
            {"properties", {{"type", "object"}, {"description", "Initial property values as key-value pairs"}}}
        }},
        {"required", {"type"}}
    }}
},
{
    {"name", "set_node_property"},
    {"description", "Set a property on an existing node"},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"node_path", {{"type", "string"}, {"description", "Path to the target node"}}},
            {"property", {{"type", "string"}, {"description", "Property name (e.g., position, visible, modulate)"}}},
            {"value", {{"type", "string"}, {"description", "Property value as string (e.g., 'Vector2(100,200)', '#ff0000', 'true')"}}}
        }},
        {"required", {"node_path", "property", "value"}}
    }}
},
{
    {"name", "delete_node"},
    {"description", "Delete a node from the scene tree"},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"node_path", {{"type", "string"}, {"description", "Path to the node to delete"}}}
        }},
        {"required", {"node_path"}}
    }}
}
```

### Request Routing in MCPServer

```cpp
// In mcp_server.cpp handle_request(), add tool dispatch:
if (tool_name == "create_node") {
    std::string type, parent_path, name;
    nlohmann::json properties;
    auto& args = params["arguments"];
    if (args.contains("type")) type = args["type"].get<std::string>();
    if (args.contains("parent_path")) parent_path = args["parent_path"].get<std::string>();
    if (args.contains("name")) name = args["name"].get<std::string>();
    if (args.contains("properties")) properties = args["properties"];
    return mcp::create_tool_result(id, create_node(type, parent_path, name, properties, undo_redo));
}
```

### ClassDB Variant to Node* Conversion

```cpp
// Verified pattern from godot-cpp class_db_singleton.hpp
Variant instance = ClassDB::instantiate(StringName("Sprite2D"));
// ClassDB::instantiate returns Variant -- extract Object* then cast
Node* node = Object::cast_to<Node>(instance.operator Object*());
// Alternative using Variant conversion:
// Object* obj = instance;  // Variant implicit conversion to Object*
// Node* node = Object::cast_to<Node>(obj);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `UndoRedo` class directly | `EditorUndoRedoManager` via EditorPlugin | Godot 4.0+ | Manager handles per-scene undo histories automatically |
| `UndoRedo::add_do_method(Callable)` | `EditorUndoRedoManager::add_do_method(Object*, StringName, args...)` | Godot 4.0+ | Uses object+method name instead of Callable for editor actions |
| `str2var()` | `str_to_var()` | Godot 4.0 | Renamed for naming consistency |
| `instance()` | `instantiate()` | Godot 4.0 | Renamed for clarity |

**Deprecated/outdated:**
- `UndoRedo` class: Still exists but should NOT be used for editor plugins. Use `EditorUndoRedoManager` instead.
- Raw `new NodeType()` for creating nodes: Use `ClassDB::instantiate()` or `memnew()` instead -- raw `new` breaks `get_class()`.

## Open Questions

1. **ClassDB::instantiate() return value extraction in godot-cpp**
   - What we know: Returns `Variant`. Need to extract `Node*`.
   - What's unclear: Whether `instance.operator Object*()` works reliably or if `Object::cast_to<Node>((Object*)(int64_t)instance)` is needed.
   - Recommendation: Test both patterns during implementation. The `operator Object*()` approach is more idiomatic. If it fails, use `static_cast` via Variant's Object pointer.

2. **Initial properties on create_node**
   - What we know: Can set properties after add_child using `set()`. Each property set should be part of the same UndoRedo action.
   - What's unclear: Whether setting properties BEFORE add_child (while node is orphaned) works correctly for all property types.
   - Recommendation: Set properties AFTER add_child/set_owner within the same UndoRedo action, recording old values for undo.

3. **Node name uniqueness**
   - What we know: Godot auto-deduplicates names when add_child is called (e.g., "Sprite2D" becomes "Sprite2D2").
   - What's unclear: Whether the undo_redo method-based add_child passes through the force_readable_name parameter correctly.
   - Recommendation: Pass `true` for `force_readable_name` parameter in add_child. Accept the auto-renamed name and return it in the response.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 (via CMake FetchContent) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd tests/build && "C:/Program Files/CMake/bin/ctest.exe" --output-on-failure` |
| Full suite command | `cd tests/build && "C:/Program Files/CMake/bin/ctest.exe" --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SCNE-02 | create_node tool creates correct JSON response format | unit | `ctest -R test_scene_mutation` | No -- Wave 0 |
| SCNE-03 | set_node_property tool creates correct JSON response format | unit | `ctest -R test_scene_mutation` | No -- Wave 0 |
| SCNE-04 | delete_node tool creates correct JSON response format | unit | `ctest -R test_scene_mutation` | No -- Wave 0 |
| SCNE-05 | UndoRedo integration | manual-only | Manual: Ctrl+Z/Y in Godot editor | N/A (requires running editor) |
| SCNE-06 | Variant type parsing from strings | unit | `ctest -R test_variant_parser` | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && "C:/Program Files/CMake/bin/ctest.exe" --output-on-failure`
- **Per wave merge:** Full ctest suite + manual Godot editor UAT
- **Phase gate:** Full suite green + UAT before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_variant_parser.cpp` -- covers SCNE-06 (string-to-Variant parsing, can test without Godot)
- [ ] `tests/test_scene_mutation.cpp` -- covers SCNE-02/03/04 tool response JSON format (limited: actual node creation requires Godot runtime, but can test tool registration and argument validation)
- [ ] Update `tests/CMakeLists.txt` -- add new test targets

**Note:** Scene mutation operations (SCNE-02/03/04/05) fundamentally require a running Godot editor with scene tree. Unit tests can only validate:
1. Tool registration (correct JSON schema in tools/list response)
2. Argument extraction and validation logic
3. Variant parsing (SCNE-06 can be fully unit-tested)
4. Error response formats

The primary validation for SCNE-02/03/04/05 is UAT in the running editor.

## Sources

### Primary (HIGH confidence)
- `godot-cpp/gen/include/godot_cpp/classes/editor_undo_redo_manager.hpp` -- verified full API: create_action, add_do_method (variadic template), add_undo_method, add_do_property, add_undo_property, add_do_reference, add_undo_reference, commit_action
- `godot-cpp/gen/include/godot_cpp/classes/editor_plugin.hpp` -- verified `get_undo_redo()` returns `EditorUndoRedoManager*`
- `godot-cpp/gen/include/godot_cpp/classes/class_db_singleton.hpp` -- verified `instantiate(StringName)` returns `Variant`, `class_exists()`, `is_parent_class()`, `can_instantiate()`
- `godot-cpp/gen/include/godot_cpp/classes/object.hpp` -- verified `set(StringName, Variant)` and `get(StringName)` methods
- `godot-cpp/gen/include/godot_cpp/classes/node.hpp` -- verified `add_child()`, `remove_child()`, `set_owner()`, `set_name()`, `get_node_or_null()`, `queue_free()`
- `godot-cpp/gen/include/godot_cpp/variant/utility_functions.hpp` -- verified `str_to_var(String)` at line 302
- `godot-cpp/include/godot_cpp/variant/color.hpp` -- verified `Color::html()`, `Color::html_is_valid()`, `Color::named()`, `Color::from_string()`
- `godot-cpp/gen/include/godot_cpp/classes/expression.hpp` -- verified `parse()` + `execute()` as fallback parser

### Secondary (MEDIUM confidence)
- [EditorUndoRedoManager docs](https://docs.godotengine.org/en/stable/classes/class_editorundoredomanager.html) -- official reference for undo/redo patterns
- [Godot Forum: UndoRedoManager add/remove nodes](https://forum.godotengine.org/t/undoredomanager-add-and-remove-nodes-for-plugin/111380) -- community pattern for node creation/deletion with undo
- [Godot Forum: add_child/set_owner not visible](https://forum.godotengine.org/t/add-child-set-owner-not-permanent-or-visible-in-tree/24411) -- confirms set_owner requirement
- [Godot Issue #37144](https://github.com/godotengine/godot/issues/37144) -- confirms owner must be scene root, not parent
- [ClassDB docs](https://docs.godotengine.org/en/stable/classes/class_classdb.html) -- official reference for instantiate()
- [str_to_var format](https://github.com/godotengine/godot/issues/11438) -- confirms str_to_var requires type-prefixed format ("Vector2(x,y)" not "(x,y)")

### Tertiary (LOW confidence)
- ClassDB::instantiate() Variant-to-Node* extraction pattern -- synthesized from multiple sources, not found as a complete working example in godot-cpp specifically. Needs validation during implementation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All APIs verified against actual godot-cpp header files in this project
- Architecture: HIGH - Patterns derived from official Godot docs + verified header signatures
- Pitfalls: HIGH - Multiple sources confirm set_owner requirement; UndoRedo patterns from official docs
- Variant parsing: HIGH - `str_to_var` confirmed in utility_functions.hpp; Color::html confirmed in color.hpp
- ClassDB::instantiate return handling: MEDIUM - API signature verified but Variant-to-Node* extraction pattern needs runtime validation

**Research date:** 2026-03-16
**Valid until:** 2026-04-16 (stable APIs, godot-cpp v10 headers unlikely to change)
