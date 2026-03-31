# Architecture Patterns: v1.6 MCP Detail Optimizations

**Domain:** C++ GDExtension MCP Server -- reliability and persistence improvements
**Researched:** 2026-03-31
**Confidence:** HIGH (based on direct codebase analysis + Godot API docs)

## Existing Architecture Overview

The system uses a two-process, two-thread architecture:

```
[AI Client] --stdio--> [Bridge EXE ~50KB] --TCP--> [GDExtension DLL in Godot]
                                                      |
                                                      +-- IO Thread (TCP accept/read/write)
                                                      +-- Main Thread (Godot API calls via poll())
```

**Key threading contract:**
- IO thread: TCP recv/send, JSON-RPC parse, queue requests, wait on `response_cv`, send responses
- Main thread (`MCPServer::poll()` called from `MCPPlugin::_process`): dequeue requests, call Godot APIs, enqueue responses, notify `response_cv`
- Cross-thread sync: `std::recursive_mutex queue_mutex`, `std::condition_variable_any response_cv`
- Godot scene tree calls MUST happen on main thread only

**Key data structures:**
- `request_queue`: IO thread pushes, main thread pops
- `response_queue`: main thread pushes, IO thread pops after `response_cv` wakeup
- `s_disabled_tools`: static `std::set<std::string>` (in-memory only, no persistence)
- `PendingType` enum + `pending_id`: single-slot deferred request state in GameBridge
- `waiting_for_bridge` + `bridge_wait_deadline`: bridge-wait state in MCPServer

---

## Integration Analysis: Five Features

### 1. ProjectSettings::save() -- Where to Call Safely

**Current state:** `MCPPlugin::_on_port_changed()` calls `ps->set_setting()` but never calls `ps->save()`. Settings are in-memory only. On Godot restart, `project.godot` has no `meow_mcp/server/port` entry, so the default 6800 is used.

**Thread safety finding:** ProjectSettings internally uses `_THREAD_SAFE_METHOD_` macro for concurrent access. However, `save()` performs file I/O to `project.godot` which should only be called from the main thread to avoid potential issues with the editor's own save operations.

**Recommendation:** Call `ProjectSettings::save()` on the main thread, specifically inside `MCPPlugin::_on_port_changed()` immediately after `set_setting()`. This is already on the main thread (it is a signal callback from a SpinBox `value_changed` signal, which fires on the main thread).

**Integration point -- MODIFY `mcp_plugin.cpp`:**

```cpp
void MCPPlugin::_on_port_changed(double new_port) {
    int new_port_int = static_cast<int>(new_port);
    auto* ps = ProjectSettings::get_singleton();
    if (ps) {
        ps->set_setting("meow_mcp/server/port", new_port_int);
        ps->save();  // <-- ADD: persist to project.godot
    }
    // ... rest of restart logic unchanged
}
```

**Why not save on every toggle/restart:** Toggle and restart read the port but do not change it. Only `_on_port_changed` actually modifies the setting.

**Risk:** LOW. `save()` writes the entire project.godot. This is what the editor does when you click Save in Project Settings. The only concern is if the user has unsaved changes in Project Settings UI at the same moment, but `save()` merges in-memory state to disk, which is the expected behavior.

**Confidence:** HIGH -- direct API usage pattern, same as what Godot's own project settings dialog does.

---

### 2. IO Thread wait_for Timeout

**Current state:** The IO thread blocks indefinitely on `response_cv.wait()`:

```cpp
// mcp_server.cpp line 277-278
response_cv.wait(lock, [this]{ return !response_queue.empty() || !running.load(); });
```

If the main thread hangs or a deferred response never arrives, the IO thread blocks forever. The MCP client (bridge) sees the connection as alive but gets no response, and the client eventually times out on its end.

**Recommendation:** Replace `response_cv.wait()` with `response_cv.wait_for()` using a configurable timeout.

**Integration point -- MODIFY `mcp_server.cpp` io_thread_func():**

```cpp
// Replace the wait call with wait_for
bool got_response = response_cv.wait_for(
    lock,
    std::chrono::seconds(30),  // 30s timeout
    [this]{ return !response_queue.empty() || !running.load(); }
);

if (!running.load()) break;

if (!got_response) {
    // Timeout: send error response for the pending request
    // We need to track which request we're waiting for
    nlohmann::json timeout_error = mcp::create_error_response(
        pending_request_id,
        mcp::INTERNAL_ERROR,
        "Request timed out after 30 seconds. The Godot main thread may be blocked."
    );
    send_json(client_peer, timeout_error);
    continue;
}

// Send all pending responses (existing logic)
while (!response_queue.empty()) { ... }
```

**Critical design consideration:** The IO thread currently does NOT track which request it is waiting for. When `process_message_io()` returns false (request queued for main thread), the IO thread immediately blocks. We need to capture the request's `id` at that point.

**New field in MCPServer:**

```cpp
nlohmann::json io_pending_request_id;  // Set before wait, used for timeout error
```

**Updated flow:**

```cpp
if (!handled) {
    // Capture the request ID for potential timeout error
    {
        std::lock_guard<std::recursive_mutex> lock(queue_mutex);
        // The request was just pushed; peek at the back
        io_pending_request_id = request_queue.back().id;
    }
    // Wait with timeout
    std::unique_lock<std::recursive_mutex> lock(queue_mutex);
    bool got = response_cv.wait_for(lock, std::chrono::seconds(30),
        [this]{ return !response_queue.empty() || !running.load(); });
    if (!running.load()) break;
    if (!got) {
        // Timeout
        auto err = mcp::create_error_response(io_pending_request_id,
            mcp::INTERNAL_ERROR,
            "Request timed out after 30 seconds");
        send_json(client_peer, err);
        continue;  // Move on to next message
    }
    // Drain response queue (existing logic)
    ...
}
```

**Why 30 seconds:** Most Godot API calls complete in <100ms. Deferred requests (game bridge) have their own timeout mechanism (see item 3). The 30s timeout is a safety net for catastrophic hangs, not for normal deferred flow. The deferred flow works by: main thread sets `__deferred`, then later `queue_deferred_response()` pushes to `response_queue` and notifies `response_cv`. The 30s limit covers the case where deferred callback never fires AND game disconnect callback also fails.

**Confidence:** HIGH -- standard C++ `condition_variable::wait_for` pattern, well-documented behavior.

---

### 3. Deferred Game Bridge Request Timeouts in poll()

**Current state:** Deferred requests in GameBridge (`PendingType != NONE`) wait indefinitely for the game companion to respond via the debugger channel `_capture()` callback. The only escape is game disconnection (handled in `_on_session_stopped`). If the game is running but the companion script hangs, the request waits forever.

**The bridge-wait pattern (run_game wait_for_bridge) already HAS a timeout:**

```cpp
// mcp_server.cpp poll(), lines 324-341
if (waiting_for_bridge) {
    if (game_bridge && game_bridge->is_game_connected()) { /* success */ }
    else if (std::chrono::steady_clock::now() >= bridge_wait_deadline) { /* timeout */ }
}
```

**Recommendation:** Add the same deadline pattern to GameBridge deferred requests. Since GameBridge's `_capture()` fires on the main thread and `poll()` also runs on the main thread, the timeout check belongs in `MCPServer::poll()`, not in GameBridge itself.

**Integration approach -- add timeout checking to MCPServer::poll():**

New fields in `MeowDebuggerPlugin` (game_bridge.h):

```cpp
std::chrono::steady_clock::time_point pending_deadline;
bool has_pending_timeout() const;
nlohmann::json expire_pending();  // Returns timeout error response, resets state
```

**Modified MCPServer::poll():**

```cpp
void MCPServer::poll() {
    std::lock_guard<std::recursive_mutex> lock(queue_mutex);

    // Check bridge-wait state (existing)
    if (waiting_for_bridge) { ... }

    // NEW: Check deferred game bridge request timeout
    if (game_bridge && game_bridge->has_pending_timeout()) {
        auto timeout_response = game_bridge->expire_pending();
        response_queue.push({timeout_response});
        response_cv.notify_one();
    }

    // Process request queue (existing)
    while (!request_queue.empty()) { ... }
}
```

**GameBridge implementation:**

```cpp
// Default timeout: 15 seconds (game operations should respond within a few frames)
static constexpr auto DEFERRED_TIMEOUT = std::chrono::seconds(15);

// Called when setting up any deferred request (add to each request_* method)
void set_pending_deadline() {
    pending_deadline = std::chrono::steady_clock::now() + DEFERRED_TIMEOUT;
}

bool has_pending_timeout() const {
    return pending_type != PendingType::NONE &&
           std::chrono::steady_clock::now() >= pending_deadline;
}

nlohmann::json expire_pending() {
    std::string type_name;
    switch (pending_type) {
        case PendingType::VIEWPORT_CAPTURE: type_name = "viewport capture"; break;
        case PendingType::CLICK_NODE: type_name = "click_node"; break;
        // ... etc for each type
        default: type_name = "unknown"; break;
    }
    auto response = mcp::create_tool_error_result(pending_id,
        "Game bridge request timed out after 15 seconds (" + type_name +
        "). The game may be unresponsive or the MCP bridge companion script is not loaded.");
    pending_type = PendingType::NONE;
    return response;
}
```

**Each deferred request setup must call `set_pending_deadline()`:**

```cpp
nlohmann::json MeowDebuggerPlugin::request_game_viewport_capture(...) {
    // ... existing checks ...
    pending_type = PendingType::VIEWPORT_CAPTURE;
    pending_id = id;
    pending_deadline = std::chrono::steady_clock::now() + DEFERRED_TIMEOUT;  // ADD
    // ... send_to_game ...
}
```

**Why 15 seconds for deferred, 30 for IO:** Deferred requests go to a running game that should respond within 1-2 frames (~33-66ms). 15s is extremely generous. The IO thread 30s timeout covers the broader case including slow main-thread processing of complex tools (scene tree serialization of large scenes, etc.).

**Why check in poll() not in GameBridge itself:** GameBridge's `_capture()` is event-driven (only fires when data arrives). There's no polling loop in GameBridge. The timeout check needs a heartbeat, and `poll()` already provides that (called every frame from `_process`).

**Confidence:** HIGH -- follows the exact same pattern as the existing `waiting_for_bridge` timeout in poll().

---

### 4. Persisting Tool Disable State

**Current state:** `s_disabled_tools` is a static `std::set<std::string>` in `mcp_tool_registry.cpp`. When the user toggles tool checkboxes in the Dock panel, `set_tool_disabled()` modifies this in-memory set. On Godot restart, all tools are re-enabled.

**Storage options considered:**

| Option | Pros | Cons | Verdict |
|--------|------|------|---------|
| ProjectSettings | Per-project, standard Godot mechanism, visible in Project Settings UI | Pollutes project.godot with tool names, not great for version control | **Use this** |
| EditorSettings | Global, not project-specific | Wrong scope -- each project may want different tools | Reject |
| ConfigFile (custom file) | Clean separation, flexible | Extra file to manage, non-standard location | Reject |
| .godot/ folder (editor metadata) | Per-project, not in VCS | Undocumented, may break across versions | Reject |

**Recommendation:** Use ProjectSettings with a single comma-separated string. This keeps project.godot clean (one line) and is per-project.

**Setting key:** `meow_mcp/tools/disabled`
**Format:** Comma-separated tool names, e.g. `"capture_viewport,restart_editor,inject_input"`

**Integration points:**

**Keep `mcp_tool_registry.cpp` pure C++ (no Godot headers).** Move load/save to MCPPlugin, which already depends on Godot. This preserves the unit testability of the tool registry.

**MODIFY `mcp_plugin.cpp` -- add load/save helpers:**

```cpp
// In _enter_tree(), after port settings:
if (!ps->has_setting("meow_mcp/tools/disabled")) {
    ps->set_setting("meow_mcp/tools/disabled", "");
}
ps->set_initial_value("meow_mcp/tools/disabled", "");
// Load saved disabled tools
String disabled_csv = ps->get_setting("meow_mcp/tools/disabled");
// Parse CSV and call set_tool_disabled() for each name

// In _on_tool_toggled(), after set_tool_disabled():
// Build CSV from get_disabled_tools(), save to ProjectSettings + save()
```

**MODIFY `mcp_dock.cpp` -- restore checkbox state after loading:**

`build_tool_checkboxes()` already reads `is_tool_disabled()` for initial state. Since `_enter_tree()` loads disabled tools before `build_tool_checkboxes()` is called, checkboxes will reflect the persisted state automatically.

**Confidence:** HIGH -- follows the same pattern as the port setting, keeps architecture clean.

---

### 5. Port Auto-Increment: Remove Entirely with Error Reporting

**Current state:** When the configured port is busy, the server tries ports `port` through `port+9`:

```cpp
// mcp_plugin.cpp _enter_tree(), line 84-91
for (int i = 0; i < max_attempts; i++) {
    actual_port = server->start(port + i);
    if (actual_port > 0) { port = actual_port; break; }
}
```

This causes a desync: the bridge is configured with `--port 6800` but the server silently switches to 6801. The AI client connects to the bridge on 6800, which relays to 6800, but the GDExtension is listening on 6801. Communication fails silently.

**Recommendation: Remove auto-increment entirely. Fail fast with a clear error.**

**Rationale:**
- The bridge's `--port` flag is set once at configuration time (via `claude mcp add`)
- If the port is busy, the user needs to know and either change it or close the conflicting process
- Silent port switching violates the principle of least surprise
- The dock panel already shows the actual port, but the bridge has no way to discover the changed port

**Integration points -- MODIFY `mcp_plugin.cpp`:**

Replace the auto-increment loop in four places (`_enter_tree`, `_on_toggle_pressed`, `_on_restart_pressed`, `_on_port_changed`) with a single-attempt pattern:

```cpp
// _enter_tree:
int actual_port = server->start(port);
if (actual_port == 0) {
    UtilityFunctions::push_error(
        String::utf8("MCP Meow: 端口 ") + String::num_int64(port)
        + String::utf8(" 被占用，无法启动。请在 Dock 面板修改端口后重试。"));
    // Update dock to show stopped state
    if (dock) {
        dock->update_status(false, false, 0, version_string, tool_count);
        dock->update_buttons(false);
    }
} else {
    port = actual_port;
}
```

**Same pattern for `_on_toggle_pressed`, `_on_restart_pressed`, `_on_port_changed`:** single attempt, error on failure.

**Use `push_error` instead of `printerr`:** This makes the error appear in both the Output panel and the Debugger panel (the dual logging requirement from v1.6). `printerr` only goes to Output; `push_error` goes to both.

**Confidence:** HIGH -- simple removal of existing loop, addresses the documented bug directly.

---

## Component Boundary Summary

| Component | Current Responsibility | v1.6 Changes |
|-----------|----------------------|--------------|
| `mcp_plugin.cpp` | Plugin lifecycle, dock signals, port config | + `ps->save()` calls, + disabled tools load/save, - auto-increment loops |
| `mcp_server.h/cpp` | IO thread, request queue, poll loop | + `wait_for` timeout in IO thread, + deferred timeout check in poll() |
| `game_bridge.h/cpp` | Deferred request state machine | + `pending_deadline` field, + `has_pending_timeout()`, + `expire_pending()` |
| `mcp_tool_registry.h/cpp` | Tool definitions, disabled set | No changes (persistence stays in mcp_plugin) |
| `error_enrichment.cpp` | TOOL_PARAM_HINTS map | - Remove entries for deleted tools |
| `mcp_dock.h/cpp` | UI elements | No changes needed for v1.6 |

## New vs Modified Explicit

**New code:**
- `MeowDebuggerPlugin::pending_deadline` (field)
- `MeowDebuggerPlugin::has_pending_timeout()` (method)
- `MeowDebuggerPlugin::expire_pending()` (method)
- `MCPServer::io_pending_request_id` (field, for timeout error reporting)
- Load/save helpers in `mcp_plugin.cpp` for disabled tools CSV

**Modified code:**
- `MCPServer::io_thread_func()` -- `wait` to `wait_for`
- `MCPServer::poll()` -- add deferred timeout check block
- `MCPPlugin::_on_port_changed()` -- add `ps->save()`
- `MCPPlugin::_enter_tree()` -- add disabled tools loading, remove auto-increment loop
- `MCPPlugin::_on_toggle_pressed()` -- remove auto-increment loop
- `MCPPlugin::_on_restart_pressed()` -- remove auto-increment loop
- `MCPPlugin::_on_port_changed()` -- remove auto-increment loop
- `MCPPlugin::_on_tool_toggled()` -- add save call
- `error_enrichment.cpp` TOOL_PARAM_HINTS -- remove ~20 stale entries
- Each deferred request setup in `game_bridge.cpp` (~10 methods) -- add deadline assignment

**Deleted code:**
- Auto-increment for-loops (4 instances across mcp_plugin.cpp)
- ~20 TOOL_PARAM_HINTS entries for tools removed in v1.5 consolidation

## Data Flow Changes

### Before v1.6
```
Port change -> set_setting() -> [lost on restart]
Tool toggle -> s_disabled_tools -> [lost on restart]
IO wait     -> wait() indefinitely
Deferred    -> wait for _capture() or game disconnect
Port busy   -> silently increment, desync with bridge
```

### After v1.6
```
Port change -> set_setting() -> save() -> project.godot [persisted]
Tool toggle -> s_disabled_tools + save() -> project.godot [persisted]
IO wait     -> wait_for(30s) -> timeout error response
Deferred    -> poll() checks deadline -> timeout error after 15s
Port busy   -> push_error() -> dock shows stopped state
```

## Suggested Build Order

Dependencies between features determine safe build order:

```
1. Remove auto-increment (standalone, no deps)
   - Touches: mcp_plugin.cpp (4 loop sites)
   - Risk: LOW (deletion only)
   - Test: Start on busy port, verify error message

2. Add ProjectSettings::save() for port
   - Touches: mcp_plugin.cpp (_on_port_changed)
   - Risk: LOW (one line addition)
   - Test: Change port, restart Godot, verify port persisted

3. Persist tool disable state
   - Touches: mcp_plugin.cpp (_enter_tree, _on_tool_toggled)
   - Depends on: save() pattern established in step 2
   - Risk: LOW
   - Test: Disable tool, restart Godot, verify still disabled

4. IO thread wait_for timeout
   - Touches: mcp_server.h, mcp_server.cpp (io_thread_func)
   - Risk: MEDIUM (threading change, needs careful testing)
   - Test: Block main thread artificially, verify timeout fires

5. Deferred game bridge timeout
   - Touches: game_bridge.h, game_bridge.cpp, mcp_server.cpp (poll)
   - Risk: MEDIUM (adds timeout to state machine)
   - Test: Send deferred request to non-responsive game

6. Clean up stale TOOL_PARAM_HINTS
   - Touches: error_enrichment.cpp
   - Risk: LOW (deletion only)
   - Test: Unit tests still pass

7. Dual logging (push_error migration)
   - Touches: mcp_plugin.cpp, mcp_server.cpp (error logging calls)
   - Risk: LOW
   - Test: Verify errors appear in both Output and Debugger panels
```

**Steps 1-3 are independent and LOW risk** -- can be done in any order or parallel.
**Steps 4-5 are the threading changes** -- do 4 before 5 since 5 depends on understanding the timeout pattern.
**Steps 6-7 are cleanup** -- can be done anytime.

## Logging Strategy: push_error vs printerr

**Current mix:**
- `UtilityFunctions::print()` -- informational (5 sites)
- `UtilityFunctions::printerr()` -- errors, Output panel only (3 sites)
- `UtilityFunctions::push_warning()` -- warnings, Output + Debugger (1 site)

**v1.6 rule:** Use `push_error()` for all error conditions. This makes errors appear in both the Output panel and the Debugger's Errors tab. Use `push_warning()` for non-fatal warnings. Keep `print()` for informational messages.

**Migration:**
- `printerr("Failed to start on ports ...")` -> `push_error("...")`
- `printerr("Failed to start TCP server ...")` -> `push_error("...")`
- `printerr("Failed to restart on port ...")` -> `push_error("...")`
- `printerr("Game input injection error ...")` -> `push_error("...")`

**Note:** `push_error()` takes a single `String` argument (no variadic), so multi-argument `printerr` calls need string concatenation.

## Scalability Considerations

| Concern | Current (30 tools) | At 100 tools | At 500 tools |
|---------|-------------------|--------------|--------------|
| Tool disable CSV | Short string | ~2KB in project.godot | Consider JSON array or separate file |
| TOOL_PARAM_HINTS map | ~50 entries | ~100 entries | Consider generating from tool definitions |
| Deferred request queue | Single-slot | Single-slot sufficient | Consider multi-slot if parallel requests needed |
| IO thread timeout | One request at a time | Same | Would need per-request tracking for pipeline |

For v1.6 scope (30 tools, single request), all approaches are well within safe limits.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Saving on Every Toggle
**What:** Calling `ps->save()` on every individual checkbox toggle when user is bulk-toggling tools
**Why bad:** Each `save()` writes the entire project.godot to disk. Rapid toggling = rapid disk I/O.
**Instead:** Debounce saves. Either save after a 1-second idle timer, or save on dock panel focus-lost. For v1.6, immediate save is acceptable (tool toggling is rare), but note this for future optimization.

### Anti-Pattern 2: Adding Timeout to GameBridge Internally
**What:** Having GameBridge start a timer/thread for its own timeout
**Why bad:** GameBridge runs on main thread only. Creating a timer would require a Godot Timer node (adding scene tree complexity) or a separate thread (adding thread safety issues).
**Instead:** Check deadline in the already-running `poll()` loop. Zero new threads, zero new nodes.

### Anti-Pattern 3: Partial Port Increment
**What:** Auto-incrementing and then trying to communicate the new port to the bridge
**Why bad:** The bridge is a separate process started by the AI client. There's no reliable back-channel.
**Instead:** Fail fast. The user changes the port in the dock, which is visible and deterministic.

## Stale TOOL_PARAM_HINTS Entries to Remove

The following entries in `error_enrichment.cpp` TOOL_PARAM_HINTS map reference tools that were removed during the v1.5 consolidation (59 -> 30 tools). They should be deleted:

| Entry | Reason for Removal |
|-------|-------------------|
| `detach_script` | Consolidated into attach_script (detach by passing empty path) |
| `instantiate_scene` | Consolidated into create_node |
| `get_node_signals` | Consolidated into get_scene_tree |
| `set_layout_preset` | Consolidated into set_node_property |
| `set_theme_override` | Consolidated into set_node_property |
| `create_stylebox` | Consolidated into set_node_property |
| `create_animation` | Kept but entry may need verification |
| `add_animation_track` | Kept but entry may need verification |
| `set_keyframe` | Kept but entry may need verification |
| `get_animation_info` | Kept but entry may need verification |
| `set_animation_properties` | Kept but entry may need verification |
| `get_resource_info` | Consolidated into list_project_files |
| `click_node` | Consolidated into inject_input |
| `get_node_rect` | Consolidated into eval_in_game |
| `run_test_sequence` | Removed entirely |
| `get_ui_properties` | Consolidated into get_scene_tree |
| `set_container_layout` | Consolidated into set_node_property |
| `get_theme_overrides` | Consolidated into get_scene_tree |
| `find_nodes` | Still exists -- keep |
| `create_character` | Still exists -- keep |
| `create_ui_panel` | Still exists -- keep |
| `get_tilemap_cell_info` | Consolidated into set_tilemap_cells |
| `get_tilemap_info` | Consolidated into set_tilemap_cells |
| `create_collision_shape` | Consolidated into create_node |
| `get_game_scene_tree` | Consolidated into get_scene_tree |

**Verification method:** Cross-reference each TOOL_PARAM_HINTS key against the 30 tool names in `mcp_tool_registry.cpp`. Any key not in the registry should be removed.

## Sources

- Codebase analysis: `mcp_server.h/cpp`, `mcp_plugin.h/cpp`, `game_bridge.h/cpp`, `mcp_tool_registry.h/cpp`, `mcp_dock.h/cpp`, `error_enrichment.h/cpp`
- [Thread Safety in GDExtension](https://vorlac.github.io/gdextension-docs/advanced_topics/thread-safety/) -- confirms `_THREAD_SAFE_METHOD_` usage in Godot APIs
- [std::condition_variable::wait_for](https://en.cppreference.com/w/cpp/thread/condition_variable/wait_for) -- C++ standard reference for timeout pattern
- [ProjectSettings & EditorSettings](https://deepwiki.com/godotengine/godot/3.3-projectsettings-and-editorsettings) -- confirms ProjectSettings uses thread-safe methods and persists to project.godot
- [ProjectSettings docs](https://docs.godotengine.org/en/stable/classes/class_projectsettings.html) -- save() is intended for editor plugins
