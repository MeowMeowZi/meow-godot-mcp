# Domain Pitfalls: v1.6 MCP Detail Optimizations

**Domain:** C++ GDExtension MCP Server -- reliability and persistence improvements
**Project:** Godot MCP Meow
**Researched:** 2026-03-31
**Confidence:** HIGH (based on direct codebase analysis, C++ threading patterns, Godot API behavior)

---

## Critical Pitfalls

### Pitfall 1: Stale Response After IO Thread Timeout Corrupts Next Request

**What goes wrong:** The IO thread times out after 30 seconds and sends a timeout error to the MCP client. Meanwhile, the main thread (which was slow but not dead) eventually produces the real response and pushes it to `response_queue`. The IO thread has already moved on and is now waiting for the NEXT request's response. When the next request arrives and the IO thread blocks on `response_cv.wait_for()`, the stale response from the previous request wakes it up. The IO thread sends the stale response as if it were the answer to the current request. The MCP client receives a response with the wrong request ID (or wrong content), breaking the protocol.

**Why it happens:** The current architecture uses a single `response_queue` without request ID tagging. Responses are consumed in FIFO order. There is no mechanism to match a response to its originating request. When timeout introduces the possibility of "orphaned" responses, the FIFO assumption breaks.

**Consequences:** Silent protocol corruption. The MCP client may crash, display wrong data, or enter an error state. The bug is intermittent (only triggers when a request genuinely takes >30s, then a follow-up request arrives before the stale response is drained).

**Prevention:**
1. After sending a timeout error, drain ALL pending responses from `response_queue` before processing the next request. This discards the stale response.
2. Better: tag each response with the request ID it corresponds to. In the IO thread, after timeout, set a "skip until next request" flag. When the stale response arrives, discard it (ID mismatch).
3. Best: after timeout, push a poison pill to `request_queue` that tells the main thread to discard the current in-flight response. This prevents the stale response from ever being queued.

**Detection:** Hard to detect in normal testing. Requires: (1) artificially slowing the main thread, (2) waiting for timeout, (3) sending a second request immediately, (4) verifying the second response matches the second request.

---

### Pitfall 2: Deferred Timeout Fires While _capture() Is Delivering the Response

**What goes wrong:** A deferred game bridge request (e.g., viewport capture) times out in `MCPServer::poll()`. At the exact same frame, `MeowDebuggerPlugin::_capture()` receives the viewport data and calls the deferred callback, which calls `queue_deferred_response()`. Both paths write a response for the same request ID. The MCP client receives two responses for one request.

**Why it happens:** Both `poll()` and `_capture()` run on the main thread, but they are called at different points in the frame. `_process()` calls `poll()`, and `_capture()` is called by the debugger session's message dispatch. If both fire in the same frame, there is a race within the main thread itself (not a cross-thread race, but a same-frame ordering issue).

**Consequences:** Duplicate responses for a single request. The MCP protocol expects exactly one response per request. The client may treat the second response as a response to a different request or raise a protocol error.

**Prevention:**
- In `expire_pending()`, set `pending_type = PendingType::NONE` BEFORE constructing the timeout response. This way, if `_capture()` fires later in the same frame, it will see `pending_type == NONE` and skip the delivery.
- Alternatively, check `pending_type` at the start of `_capture()` response delivery: if `pending_type` has already been cleared, do not deliver.
- The existing `_on_session_stopped` handler already handles this pattern (it checks `pending_type != PendingType::NONE` before delivering an error). Follow the same guard pattern.

**Detection:** Hard to reproduce. Requires a game bridge response arriving at the exact timeout boundary. Test by setting timeout to 1 second and sending a request to a game that takes ~1 second to respond.

---

### Pitfall 3: ProjectSettings::save() Skips Default Values

**What goes wrong:** The user sets port to 6800 (the default) via the SpinBox. `set_setting()` stores 6800 in memory. `save()` is called, but Godot's save logic skips values that match `set_initial_value()`. The entry `meow_mcp/server/port=6800` never appears in `project.godot`. On restart, `has_setting()` returns false (since the setting is not in the file), and the code falls through to the default -- which happens to be 6800 anyway. In this specific case, the bug is invisible. But if the default is later changed to 6900, existing projects that explicitly set 6800 will silently switch to 6900.

**Why it happens:** Godot's `ProjectSettings::save()` optimizes file size by not writing values that match the initial/default value. This is documented behavior but easy to forget when the "initial value" is the same as the commonly used value.

**Consequences:** For port 6800 specifically: no visible bug today. For disabled tools (default is empty string ""): if user re-enables all tools, the empty string may not be saved, and any previously saved disabled list persists in the file. This is actually correct behavior. The real risk is if the default ever changes.

**Prevention:**
- Do NOT call `set_initial_value()` for the port setting. Without an initial value, Godot has no "default" to compare against, so `save()` always writes the value.
- Alternatively, accept the behavior: if the user sets port to the default value, it is fine for the file to not contain it, since the code already handles the missing-setting case by falling back to 6800.
- For disabled tools: `set_initial_value("meow_mcp/tools/disabled", "")` means "all enabled is default." If the user disables tools and then re-enables all of them, the empty string matches the initial value and gets skipped in save. This is correct -- no disabled tools = default state = no need to persist.

**Detection:** Set port to 6800 via SpinBox, check `project.godot` for the entry. If absent, the optimization is active.

---

## Moderate Pitfalls

### Pitfall 4: push_error() Argument Differences from printerr()

**What goes wrong:** `UtilityFunctions::printerr()` accepts variadic arguments and concatenates them: `printerr("Port ", port, " failed")`. `UtilityFunctions::push_error()` takes a single `String` argument. Developers migrating from `printerr()` to `push_error()` forget to concatenate manually, leading to compile errors or truncated messages.

**Prevention:** When migrating logging calls, always build a single `String` first:
```cpp
String msg = String::utf8("MCP Meow: 端口 ") + String::num_int64(port) + String::utf8(" 被占用");
UtilityFunctions::printerr(msg);
UtilityFunctions::push_error(msg);
```

---

### Pitfall 5: Disabled Tools CSV Includes Tool Names That No Longer Exist

**What goes wrong:** User disables `capture_viewport` in v1.6 and it is saved to `project.godot`. In v1.7, the tool is renamed to `screenshot_viewport`. The CSV in `project.godot` still contains `capture_viewport`. On startup, the code calls `set_tool_disabled("capture_viewport", true)`, but this tool no longer exists in the registry. The stale entry sits in `s_disabled_tools` permanently, wasting memory (negligible) and cluttering the persisted setting.

**Prevention:**
- On load, validate each tool name against the current registry. Skip (and log a warning for) tool names not found in `get_all_tools()`.
- On save, only persist tool names that currently exist in the registry. This auto-cleans stale entries.

---

### Pitfall 6: Auto-Increment Removal Breaks Multi-Instance Workflows

**What goes wrong:** Some users run two Godot instances for the same project (e.g., testing multiplayer). Currently, the second instance auto-increments to port 6801. After removing auto-increment, the second instance fails to start the MCP server entirely. The user has no workaround except manually changing the port in the second instance's dock panel every time.

**Prevention:**
- This is an acceptable trade-off. The bridge desync bug is worse than the multi-instance inconvenience.
- Document in the error message: "Change port in MCP Meow dock panel or close the other Godot instance."
- For true multi-instance support, users can set different port numbers in each project's `project.godot`.

---

## Minor Pitfalls

### Pitfall 7: Removing TOOL_PARAM_HINTS for Wrong Tools

**What goes wrong:** During cleanup of stale TOOL_PARAM_HINTS entries, a developer removes an entry for a tool that still exists (just with a different name or under a different code path). The tool's error messages lose their parameter format hints.

**Prevention:** Before removing any entry, search `mcp_server.cpp` for the tool name to confirm it is truly not dispatched. Cross-reference against `mcp_tool_registry.cpp`'s 30 active tool names.

### Pitfall 8: Timeout Value Too Short for Legitimate Slow Operations

**What goes wrong:** The 30-second IO timeout fires during a legitimate slow operation. For example, `validate_scripts` on a project with 500+ GDScript files may take 30+ seconds. The IO thread times out, but the main thread is making progress (not hung).

**Prevention:**
- Profile `validate_scripts` on a large project. If it regularly exceeds 10 seconds, it needs incremental processing or a progress mechanism.
- Consider making the IO timeout configurable via ProjectSettings (default 30s, users can increase).
- The 15-second deferred timeout is less risky because game bridge operations are inherently fast (single frame response time).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Port persistence (save) | Default value not saved (P3) | Avoid set_initial_value or accept the behavior |
| Tool disable persistence | Stale tool names in CSV (P5) | Validate against registry on load |
| IO thread timeout | Stale response corruption (P1) | Drain queue after timeout or tag with request ID |
| Deferred timeout | Same-frame race with _capture (P2) | Clear pending_type before constructing timeout response |
| Remove auto-increment | Multi-instance users impacted (P6) | Document workaround in error message |
| push_error migration | Argument type mismatch (P4) | Build String before calling both functions |
| TOOL_PARAM_HINTS cleanup | Remove wrong entry (P7) | Cross-reference against tool registry |

---

## Sources

- Codebase analysis: `mcp_server.cpp` line 278 (wait without timeout), `game_bridge.cpp` (10 deferred request sites with no deadline)
- [std::condition_variable::wait_for](https://en.cppreference.com/w/cpp/thread/condition_variable/wait_for) -- timeout semantics and return values
- [ProjectSettings save behavior](https://forum.godotengine.org/t/projectsettings-save-custom-doesnt-seem-to-work-as-intended/68532) -- known issues with default value skipping
- [Thread Safety in GDExtension](https://vorlac.github.io/gdextension-docs/advanced_topics/thread-safety/) -- main thread requirements for Godot API calls
- v1.0-v1.5 project experience: deferred request patterns, bridge disconnect handling in `_on_session_stopped`
