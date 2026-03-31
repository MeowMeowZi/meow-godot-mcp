# Feature Research: v1.6 MCP Detail Optimizations

**Domain:** MCP Server plugin for Godot Engine (GDExtension/C++) -- v1.6 milestone
**Researched:** 2026-03-31
**Confidence:** HIGH (based on Godot API verification, MCP spec 2025-03-26 review, codebase audit)

## Existing Capabilities (v1.5 baseline)

30 MCP tools, 15 prompt templates, 5 MCP Resources (2 static + 3 URI templates), editor Dock panel with connection status / tool category checkboxes, smart error handling with Levenshtein fuzzy matching, game bridge for runtime interaction (deferred request pattern). Port configured via `ProjectSettings.set_setting()` but NOT persisted to `project.godot`. Tool disabled state stored in static `std::set<std::string>` -- lost on editor restart.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features that represent basic reliability. Missing these = the plugin feels buggy or half-finished.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Port setting persists across editor restarts | Any setting a user configures in a UI should survive restart. Current behavior (port resets to 6800) violates the principle of least surprise. | LOW | `ProjectSettings::save()` call needed, or use `_get_window_layout`/`_set_window_layout` |
| Tool enable/disable state persists across restarts | Users who carefully disable certain tools expect that choice to be remembered. Current behavior (all tools re-enabled on restart) is confusing. | LOW | Same persistence mechanism as port; load disabled set on `_enter_tree` |
| Deferred request timeout | If a game bridge operation (viewport capture, eval_in_game, etc.) hangs, the MCP client should not wait forever. Currently 9 out of 10 deferred operations have NO timeout. Only `run_game` with `wait_for_bridge` has a deadline. | MEDIUM | Need deadline tracking per pending deferred request, checked in `poll()` |
| Remove silent port auto-increment | Port auto-increment causes plugin and bridge to listen on different ports without any obvious indication. AI client connects to the bridge's configured port while plugin silently moved to port+N. | LOW | Fail loudly instead of silently incrementing |

### Differentiators (Competitive Advantage)

Features that improve developer experience beyond what competitors offer.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Dual-panel error logging (Output + Debugger) | Plugin errors currently only appear in Output panel (via `printerr()`). Developers who monitor the Debugger Errors tab miss MCP errors entirely. Logging to both panels catches all users. | LOW | Add `push_error()`/`push_warning()` calls alongside existing `printerr()` calls |
| MCP cancellation support | MCP spec 2025-03-26 defines `notifications/cancelled` for in-flight request cancellation. No competitor implements this. Would allow AI clients to abort long-running operations cleanly. | HIGH | Requires tracking in-flight request IDs, handling cancellation in IO thread, cleaning up deferred state |
| Timeout value configurable per-project | Allow users to set default timeout for deferred operations in ProjectSettings, not hardcoded. Power users running complex test sequences need longer timeouts. | LOW | Single ProjectSettings entry with sensible default (30s) |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems in this specific context.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Per-tool timeout configuration | "Different tools need different timeouts" | Adds UI complexity for 30 tools. No user can reason about appropriate timeouts per tool. The MCP client already has its own timeout. | Single global timeout (30s default) is sufficient. `run_game` already has its own `timeout` parameter for the specific wait_for_bridge case. |
| Persist settings in a custom .cfg file in the addon folder | "Keep plugin settings separate from project.godot" | Adds file management complexity, .gitignore concerns, and the file might not exist on first run. ProjectSettings is the Godot-standard way for plugins. | Use `ProjectSettings::save()` to persist to `project.godot` -- standard, version-controlled, no extra files. |
| Rich logging with custom Output panel tab | "MCP should have its own log panel" | Enormous complexity (custom EditorDebuggerPlugin tabs, message routing). The Dock panel already shows connection status. Output panel is where users look for plugin messages. | Use existing Output panel + Debugger Errors tab via dual logging. |
| Automatic port conflict resolution with notification to bridge | "Auto-increment port but tell the bridge" | The bridge is configured at MCP client startup time (e.g., `claude mcp add --port 6800`). There is no mechanism to notify a running bridge that the port changed. | Fail on port conflict with clear error message telling user to change port or close conflicting process. |

---

## Feature Details

### 1. Port Setting Persistence

**Current behavior:** `MCPPlugin::_enter_tree()` calls `ProjectSettings::set_setting("meow_mcp/server/port", ...)` and `set_initial_value()`, but never calls `ProjectSettings::save()`. The setting exists in the editor's runtime memory but is NOT written to `project.godot`. On restart, it resets to the default 6800.

**Expected behavior:** When user changes port via the Dock SpinBox, the new value should survive editor restart.

**Implementation approach:** Call `ProjectSettings::save()` after `set_setting()` in `_on_port_changed()`. This writes the value to `project.godot`.

**Why ProjectSettings::save() and not _get_window_layout:**
- `_get_window_layout(ConfigFile)` stores data in the editor's layout file (`.godot/editor/editor_layout.cfg`). This file is NOT version-controlled and is user-specific. Port must be consistent across team members to match the bridge `--port` configuration.
- `ProjectSettings::save()` writes to `project.godot`, which IS version-controlled. The port setting in `project.godot` ensures the bridge and plugin always agree on port.

**Caveat from Godot docs:** `ProjectSettings::save()` skips values that match the engine default. Since our default is 6800 (not a Godot built-in), and we register via `set_setting()` (not Godot's internal defaults), our custom setting WILL be saved. Verified: `set_initial_value()` sets the "initial value" for the ProjectSettings UI reset button but does NOT affect `save()` behavior for custom settings. [MEDIUM confidence -- needs runtime verification]

**Complexity:** LOW -- single `save()` call addition.

### 2. Tool Disable State Persistence

**Current behavior:** `set_tool_disabled()` modifies a static `std::set<std::string> s_disabled_tools` in `mcp_tool_registry.cpp`. This set is lost when the editor restarts. On restart, all checkboxes are re-checked.

**Expected behavior:** Disabled tools stay disabled across editor sessions.

**Two viable approaches:**

**Option A: ProjectSettings (recommended)**
- Store a comma-separated string or PackedStringArray in `ProjectSettings` under `meow_mcp/tools/disabled`.
- On `_enter_tree()`, read the setting and populate `s_disabled_tools`.
- On toggle, update the setting and call `ProjectSettings::save()`.
- Pros: Same mechanism as port, version-controlled, team-consistent.
- Cons: Slightly pollutes `project.godot` with tool names.

**Option B: _get_window_layout / _set_window_layout**
- Override these on MCPPlugin to save/restore disabled tools via `ConfigFile`.
- Pros: Clean separation from project settings.
- Cons: NOT version-controlled (stored in `.godot/editor/editor_layout.cfg`), so each team member has different tool visibility. This is actually appropriate for tool checkboxes (personal preference), but it means a fresh clone starts with all tools enabled.

**Recommendation:** Use Option A (ProjectSettings). Tool visibility affects what the MCP client sees in `tools/list` responses. If one developer disables `run_game` and commits, the AI client won't see it -- this is intentional project configuration, not personal preference. A team should agree on which tools are exposed.

**Complexity:** LOW -- read/write a string list, ~20 lines of code.

### 3. Deferred Request Timeout

**Current behavior (the problem):**

The IO thread blocks on `response_cv.wait()` with NO timeout predicate:
```cpp
response_cv.wait(lock, [this]{ return !response_queue.empty() || !running.load(); });
```

For deferred operations (viewport capture, click_node, get_node_rect, eval_in_game, get_game_node_property, get_game_scene_tree, run_test_sequence, inject_input_sequence, inject_text), the response comes asynchronously via `_capture()`. If the game hangs but stays connected, or the companion script has a bug, the IO thread blocks forever. The MCP client eventually times out (TypeScript SDK: 60s), but the IO thread remains stuck, unable to process any further requests.

**Only `run_game` (wait_for_bridge) has a timeout** via `bridge_wait_deadline` checked in `poll()`. The 9 other deferred operations have no timeout whatsoever.

**Expected behavior per MCP spec 2025-03-26:**
> Implementations SHOULD establish timeouts for all sent requests, to prevent hung connections and resource exhaustion.

**Implementation approach:**

Add a deadline field to the deferred request state in `MeowDebuggerPlugin`:
```
std::chrono::steady_clock::time_point pending_deadline;
```

When a deferred request is initiated, set `pending_deadline = now + timeout`.

In `MCPServer::poll()`, check the game bridge's pending deadline. If expired, synthesize an error response and deliver it via `queue_deferred_response()`.

The IO thread wait already self-resolves once a response is queued, so no changes needed there.

**Default timeout:** 30 seconds. This accommodates:
- Viewport captures on complex scenes (~2-5s typical, up to 10s)
- Test sequences with multiple steps (can take 10-20s)
- eval_in_game expressions (~1s typical)

30s is generous enough to avoid false timeouts while catching genuine hangs. Users can override via ProjectSettings if needed.

**Complexity:** MEDIUM -- touches MCPServer::poll() and game_bridge deferred state. Need to handle the race condition where the response arrives just as the timeout fires.

### 4. Dual-Panel Logging

**Current behavior:** Plugin uses `UtilityFunctions::printerr()` and `UtilityFunctions::push_warning()` inconsistently:
- `printerr()` -- Output panel only (stderr stream)
- `push_warning()` -- Debugger Errors tab only
- `UtilityFunctions::print()` -- Output panel only (stdout)

Developers who keep the Debugger tab open (common in game development) miss errors logged via `printerr()`. Developers who keep the Output panel visible miss warnings from `push_warning()`.

**Expected behavior:** Critical errors and warnings appear in BOTH panels.

**Godot logging functions breakdown:**

| Function | Output Panel | Debugger Errors Tab | Use Case |
|----------|-------------|--------------------|----|
| `print()` | Yes (stdout) | No | Info messages |
| `printerr()` | Yes (stderr) | No | Error text to output |
| `push_error()` | No | Yes (with stack trace) | Errors to debugger |
| `push_warning()` | No | Yes | Warnings to debugger |

**Implementation:** For error conditions, call BOTH `printerr()` AND `push_error()`. For warnings, call BOTH `print()` (with a "[WARN]" prefix) AND `push_warning()`. There is no `printwarn()` in Godot (proposal #10648 was rejected).

Pattern:
```cpp
// Error: appears in both panels
UtilityFunctions::printerr("MCP Meow: Failed to start on port ", port);
UtilityFunctions::push_error("MCP Meow: Failed to start on port " + String::num_int64(port));

// Warning: appears in both panels
UtilityFunctions::print("[WARN] MCP Meow: Port auto-changed to ", port);
UtilityFunctions::push_warning("MCP Meow: Port auto-changed to " + String::num_int64(port));
```

Note: `push_error()` in GDExtension includes source file/line information automatically, which aids debugging.

**Complexity:** LOW -- add parallel logging calls at ~10 existing log sites.

### 5. Remove Port Auto-Increment

**Current behavior:** If port 6800 is occupied, the plugin silently tries 6801, 6802, ... up to 6809. The bridge executable was configured with `--port 6800` by the AI client. Result: bridge listens on 6800, plugin on 6803 -- they never connect.

**Expected behavior:** Fail on the configured port with a clear error message. The error should tell the user: "Port 6800 is in use. Close the other Godot instance or change the port in ProjectSettings > MCP Meow > Server > Port."

**Complexity:** LOW -- remove the for-loop, keep single attempt, improve error message.

### 6. Clean Up Dead Code

**Current behavior:** Tool registry and error enrichment may contain parameter hints for tools that were removed during the v1.5 consolidation (59 -> 30 tools).

**Expected behavior:** No orphaned references to deleted tools.

**Complexity:** LOW -- audit `error_enrichment.cpp` parameter hints against current tool list.

---

## Feature Dependencies

```
[Port Persistence]
    (independent -- no dependencies)

[Tool Disable Persistence]
    (independent -- no dependencies, but shares mechanism with Port Persistence)

[Deferred Request Timeout]
    (independent -- but logically follows Port Persistence since timeout value
     could be stored in ProjectSettings too)

[Dual-Panel Logging]
    (independent -- purely additive)

[Remove Port Auto-Increment]
    └──should ship WITH──> [Port Persistence]
    (removing auto-increment only makes sense if the user can reliably set and
     persist their desired port)

[Clean Up Dead Code]
    (independent -- purely subtractive)
```

### Dependency Notes

- **Remove Port Auto-Increment requires Port Persistence:** If we remove auto-increment but the port setting resets to 6800 on restart, users who need a different port face a poor experience. Ship them together.
- **Tool Disable Persistence shares mechanism with Port Persistence:** Both use `ProjectSettings::save()`. Implement Port Persistence first to establish the pattern, then apply it to tool state.
- **Deferred Request Timeout is independent:** Can be implemented in any order. No interaction with persistence features.

---

## Prioritization

### P1: Must Ship (Core reliability)

- [x] **Port setting persistence** -- fundamental UX expectation, LOW complexity
- [x] **Remove port auto-increment** -- fixes bridge/plugin desync bug, LOW complexity
- [x] **Deferred request timeout** -- prevents hung IO thread, MEDIUM complexity, MCP spec compliance

### P2: Should Ship (Polish)

- [x] **Tool disable state persistence** -- user expectation, LOW complexity
- [x] **Dual-panel logging** -- developer experience, LOW complexity

### P3: Nice to Have (Cleanup)

- [x] **Clean up dead code** -- hygiene, LOW complexity

### Future Consideration (v2+)

- [ ] **MCP cancellation support (notifications/cancelled)** -- defer until an AI client actually sends cancellation notifications. No known client does today. HIGH complexity for uncertain benefit.

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Port persistence | HIGH | LOW | P1 |
| Remove auto-increment | HIGH | LOW | P1 |
| Deferred request timeout | HIGH | MEDIUM | P1 |
| Tool disable persistence | MEDIUM | LOW | P2 |
| Dual-panel logging | MEDIUM | LOW | P2 |
| Dead code cleanup | LOW | LOW | P3 |
| MCP cancellation | LOW (no client uses it) | HIGH | Deferred |

---

## Competitor Feature Analysis

| Feature | Godot MCP Pro ($5) | Better Godot MCP | Our Approach |
|---------|-------------------|-----------------|--------------|
| Settings persistence | N/A (Node.js config file) | N/A (Python config) | ProjectSettings in project.godot |
| Request timeout | Node.js handles via event loop | Python async timeout | Manual deadline tracking in poll() |
| Error logging | Console output only | Console output only | Dual-panel (Output + Debugger) |
| Tool enable/disable | Not supported | Not supported | Checkbox UI + persistence |

All competitors are Node.js or Python based with external process architecture. They don't face the GDExtension-specific challenges (no access to stdin/stdout, threading model, Godot API main-thread restriction). Their timeout handling comes "free" from the language runtime's async primitives. Our C++ implementation needs explicit deadline tracking.

---

## Sources

- [Godot ProjectSettings API docs](https://docs.godotengine.org/en/stable/classes/class_projectsettings.html) -- `save()`, `set_setting()`, `set_initial_value()` behavior
- [Godot EditorPlugin API docs](https://docs.godotengine.org/en/stable/classes/class_editorplugin.html) -- `_get_window_layout`, `_set_window_layout`, `_save_external_data`
- [MCP Spec 2025-03-26 Lifecycle](https://modelcontextprotocol.io/specification/2025-06-18/basic/lifecycle) -- timeout SHOULD requirements
- [MCP Spec Cancellation](https://modelcontextprotocol.io/specification/2025-03-26/basic/utilities/cancellation) -- `notifications/cancelled` protocol
- [Godot Forum: Saving data for editor plugins](https://forum.godotengine.org/t/saving-data-for-editor-plugins/52737) -- community patterns for plugin persistence
- [Godot Forum: Print warning to output](https://forum.godotengine.org/t/print-a-warning-to-output-not-debugger/80901) -- `printerr()` vs `push_error()` panel behavior
- [Godot Proposal #10648: printwarn](https://github.com/godotengine/godot-proposals/issues/10648) -- no `printwarn()` in Godot, use workarounds
- [ProjectSettings.save_custom() issues](https://forum.godotengine.org/t/projectsettings-save-custom-doesnt-seem-to-work-as-intended/68532) -- caveats with save behavior
- [MCP TypeScript SDK timeout issue #245](https://github.com/modelcontextprotocol/typescript-sdk/issues/245) -- client-side 60s hard limit
- [How to add Project Settings via GDExtension](https://godotforums.org/d/33797-how-to-add-project-settings-to-godot-using-gdextension) -- GDExtension-specific patterns
- godot-cpp source: `editor_plugin.hpp` line 185-186 confirms `_set_window_layout` / `_get_window_layout` take `Ref<ConfigFile>`
- Codebase audit: `mcp_server.cpp` line 278 -- `response_cv.wait()` has no timeout
- Codebase audit: 10 `__deferred` return sites in `game_bridge.cpp`, only 1 has deadline tracking

---
*Feature research for: Godot MCP Meow v1.6 -- MCP Detail Optimizations*
*Researched: 2026-03-31*
