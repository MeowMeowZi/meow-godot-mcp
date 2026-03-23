---
phase: quick
plan: 260323-nab
type: execute
wave: 1
depends_on: []
files_modified:
  - src/variant_parser.cpp
  - src/project_tools.cpp
  - src/mcp_server.cpp
autonomous: true
must_haves:
  truths:
    - "All substr() prefix checks in variant_parser.cpp and project_tools.cpp replaced with compare()"
    - "All ~40 tool handlers in mcp_server.cpp use helper functions instead of raw contains+is_type patterns"
    - "Error handling warnings added to variant_parser.cpp silent failure paths"
    - "Duplicate PackedByteArray send pattern in mcp_server.cpp extracted to send_json helper"
  artifacts:
    - path: "src/variant_parser.cpp"
      provides: "Optimized string prefix checks and descriptive error warnings"
    - path: "src/project_tools.cpp"
      provides: "Optimized string prefix checks"
    - path: "src/mcp_server.cpp"
      provides: "Helper functions for JSON parameter extraction and TCP sending"
  key_links:
    - from: "src/mcp_server.cpp"
      to: "get_args/get_string/get_int/get_bool/get_double helpers"
      via: "static inline functions used by all tool handlers"
      pattern: "get_args\\(params\\)"
    - from: "src/mcp_server.cpp"
      to: "send_json helper"
      via: "static function replacing 3 PackedByteArray+dump patterns"
      pattern: "send_json\\("
---

<objective>
Optimize plugin C++ code: replace inefficient string operations, refactor repetitive parameter validation into helpers, and enhance error handling with descriptive warnings.

Purpose: Reduce unnecessary memory allocations from substr(), eliminate ~500 lines of boilerplate in tool handlers, and add diagnostic output to silent failure paths.
Output: Three modified source files with cleaner, more maintainable code.
</objective>

<execution_context>
@D:\Workspace\Godot\godot-mcp-meow\.claude\get-shit-done\workflows\execute-plan.md
@D:\Workspace\Godot\godot-mcp-meow\.claude\get-shit-done\templates\summary.md
</execution_context>

<context>
@src/variant_parser.cpp
@src/project_tools.cpp
@src/mcp_server.cpp
</context>

<tasks>

<task type="auto">
  <name>Task 1: String operations optimization and error handling warnings</name>
  <files>src/variant_parser.cpp, src/project_tools.cpp</files>
  <action>
**variant_parser.cpp — Replace substr() prefix checks with compare():**

1. Line 85: `value_str.substr(0, 6) == "res://"` -> `value_str.compare(0, 6, "res://") == 0`
2. Line 90: `value_str.substr(0, 4) == "new:"` -> `value_str.compare(0, 4, "new:") == 0`
3. Line 205: Same pattern as line 85 -> `value_str.compare(0, 6, "res://") == 0`
4. Line 217: Same pattern as line 90 -> `value_str.compare(0, 4, "new:") == 0`
5. Line 308: Same pattern as line 85 -> `value_str.compare(0, 6, "res://") == 0`

Keep line 218 `value_str.substr(4)` as-is (needs the extracted value, not just a check).

**variant_parser.cpp — Add push_warning() calls to silent failure paths in parse_variant():**

6. Line 236 (class not found): After the `if (!godot::ClassDB::class_exists(gd_class))` check, before `return godot::Variant();`, add:
   `godot::UtilityFunctions::push_warning("MCP Meow: Inline resource creation failed - unknown class: " + std::string(class_name));`

7. Line 238-239 (not a resource): After the `if (!godot::ClassDB::is_parent_class(...))` check, before `return godot::Variant();`, add:
   `godot::UtilityFunctions::push_warning("MCP Meow: Inline resource creation failed - " + std::string(class_name) + " is not a Resource subclass");`

8. Line 244-246 (instantiate failed): After the `if (!obj)` check, before `return godot::Variant();`, add:
   `godot::UtilityFunctions::push_warning("MCP Meow: Failed to instantiate resource: " + std::string(class_name));`

9. Line 212-213 (resource not found): In the resource path handler, before `return godot::Variant();`, add:
   `godot::UtilityFunctions::push_warning("MCP Meow: Resource not found: " + value_str);`

Note: push_warning takes godot::String, so wrap with String().utf8() or construct properly. Since UtilityFunctions::push_warning() accepts Variant args, passing a godot::String works. Use `godot::String(("MCP Meow: ..." + std_string).c_str())` for std::string concatenation.

**project_tools.cpp — Replace substr() prefix checks with compare():**

10. Line 103: `name_str.substr(0, category.size()) != category` -> `name_str.compare(0, category.size(), category) != 0`
11. Line 110: `name_str.substr(0, prefix.size()) == prefix` -> `name_str.compare(0, prefix.size(), prefix) == 0`
  </action>
  <verify>
    <automated>cd D:/Workspace/Godot/godot-mcp-meow && grep -n "substr" src/variant_parser.cpp src/project_tools.cpp | grep -v "substr(4)\|substr(0, paren_pos)\|substr(paren_pos\|substr(eq_pos\|substr(start\|substr(s,\|substr(ks\|substr(vs\|find_first_not_of\|find_last_not_of" | head -20</automated>
  </verify>
  <done>All 7 substr()-based prefix checks replaced with compare(). Only value-extracting substr() calls remain. 4 push_warning() calls added to variant_parser.cpp silent failure paths.</done>
</task>

<task type="auto">
  <name>Task 2: Parameter validation refactoring and send_json helper</name>
  <files>src/mcp_server.cpp</files>
  <action>
**Add helper functions after the includes and `using namespace godot;` line (before MCPServer::MCPServer()):**

```cpp
// Helper: extract arguments object from params, returns empty object if missing
static inline const nlohmann::json& get_args(const nlohmann::json& params) {
    static const nlohmann::json empty_obj = nlohmann::json::object();
    if (params.contains("arguments") && params["arguments"].is_object()) {
        return params["arguments"];
    }
    return empty_obj;
}

// Helper: extract string parameter from JSON, returns empty string if missing/wrong type
static inline std::string get_string(const nlohmann::json& obj, const char* key) {
    if (obj.contains(key) && obj[key].is_string()) {
        return obj[key].get<std::string>();
    }
    return {};
}

// Helper: extract int parameter from JSON, returns default_val if missing/wrong type
static inline int get_int(const nlohmann::json& obj, const char* key, int default_val = 0) {
    if (obj.contains(key) && obj[key].is_number_integer()) {
        return obj[key].get<int>();
    }
    return default_val;
}

// Helper: extract bool parameter from JSON, returns default_val if missing/wrong type
static inline bool get_bool(const nlohmann::json& obj, const char* key, bool default_val = false) {
    if (obj.contains(key) && obj[key].is_boolean()) {
        return obj[key].get<bool>();
    }
    return default_val;
}

// Helper: extract double parameter from JSON, returns default_val if missing/wrong type
static inline double get_double(const nlohmann::json& obj, const char* key, double default_val = 0.0) {
    if (obj.contains(key) && obj[key].is_number()) {
        return obj[key].get<double>();
    }
    return default_val;
}

// Helper: serialize JSON and send over TCP peer
static void send_json(const Ref<StreamPeerTCP>& peer, const nlohmann::json& json_data) {
    std::string json_str = json_data.dump() + "\n";
    PackedByteArray data;
    data.resize(static_cast<int64_t>(json_str.size()));
    memcpy(data.ptrw(), json_str.data(), json_str.size());
    peer->put_data(data);
}
```

**Replace 3 duplicate PackedByteArray+dump patterns in io_thread_func() and process_message_io():**

1. Lines 158-162 (io_thread_func, response_queue drain): Replace with `send_json(client_peer, resp.response);`
2. Lines 197-201 (io_thread_func, request response send): Replace with `send_json(client_peer, resp.response);`
3. Lines 215-219 (process_message_io, error response): Replace with `send_json(client_peer, result.error_response);`

**Refactor ALL tool handlers in handle_request() to use get_args/get_string/get_int/get_bool/get_double helpers.** Apply to every tool handler that currently uses the `params.contains("arguments") && params["arguments"].is_object()` pattern. Key examples:

BEFORE (get_scene_tree):
```cpp
if (params.contains("arguments") && params["arguments"].is_object()) {
    auto& args = params["arguments"];
    if (args.contains("max_depth") && args["max_depth"].is_number_integer()) {
        max_depth = args["max_depth"].get<int>();
    }
    ...
}
```

AFTER:
```cpp
auto& args = get_args(params);
max_depth = get_int(args, "max_depth", -1);
include_properties = get_bool(args, "include_properties", true);
root_path = get_string(args, "root_path");
```

Apply this pattern to ALL handlers. For `has_node_path` tracking, use: `bool has_node_path = args.contains("node_path") && args["node_path"].is_string();` after `auto& args = get_args(params);` -- this is still cleaner as it removes the outer `params.contains("arguments")` check.

For JSON object/array parameters (properties, overrides, cells, coords, steps, layout_params) that do not fit the scalar helpers, keep the `args.contains("X") && args["X"].is_Y()` pattern but operate on the already-extracted `args` ref from `get_args()`.

Complete list of tool handlers to refactor (all ~40 in handle_request):
- get_scene_tree, create_node, set_node_property, delete_node
- read_script, write_script, edit_script, attach_script, detach_script
- get_project_settings, get_resource_info
- run_game, stop_game (no args), get_game_output
- get_node_signals, connect_signal, disconnect_signal
- save_scene, open_scene, list_open_scenes (no args)
- create_scene, instantiate_scene
- set_layout_preset, set_theme_override, create_stylebox, get_ui_properties, set_container_layout, get_theme_overrides
- create_animation, add_animation_track, set_keyframe, get_animation_info, set_animation_properties
- capture_viewport
- inject_input, capture_game_viewport, get_game_bridge_status (no args)
- click_node, get_node_rect
- get_game_node_property, eval_in_game, get_game_scene_tree
- run_test_sequence
- set_tilemap_cells, erase_tilemap_cells, get_tilemap_cell_info, get_tilemap_info
- create_collision_shape
- restart_editor

Also remove the unnecessary lock in stop() at lines 103-107 (after io_thread.join(), the IO thread is dead so no contention possible). The queue clearing can happen without the mutex lock.
  </action>
  <verify>
    <automated>cd D:/Workspace/Godot/godot-mcp-meow && scons target=template_debug 2>&1 | tail -5</automated>
  </verify>
  <done>All tool handlers refactored to use helper functions. 3 send_json replacements. Unnecessary mutex lock removed from stop(). Project compiles successfully with scons target=template_debug.</done>
</task>

</tasks>

<verification>
1. `scons target=template_debug` compiles with zero errors
2. `grep -c "get_args(params)" src/mcp_server.cpp` returns ~40 (one per tool handler)
3. `grep -c "substr" src/variant_parser.cpp` returns only the value-extraction substr calls (line 218 and string parsing helpers), not prefix checks
4. `grep -c "substr" src/project_tools.cpp` returns 0 for prefix checks (only unrelated substr in string parsing)
5. `grep -c "send_json" src/mcp_server.cpp` returns 3 (replacing the 3 PackedByteArray patterns)
6. `grep -c "push_warning" src/variant_parser.cpp` returns 4 (the new diagnostic messages)
</verification>

<success_criteria>
- All substr()-based prefix checks replaced with compare() in variant_parser.cpp and project_tools.cpp
- All ~40 tool handlers in mcp_server.cpp use get_args/get_string/get_int/get_bool/get_double helpers
- 3 duplicate PackedByteArray+dump patterns replaced with send_json() helper
- 4 push_warning() calls added to silent failure paths in parse_variant()
- Unnecessary mutex lock removed from stop()
- Project compiles cleanly with scons target=template_debug
</success_criteria>

<output>
After completion, create `.planning/quick/260323-nab-code-optimization/260323-nab-SUMMARY.md`
</output>
