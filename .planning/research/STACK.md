# Technology Stack: v1.6 MCP Detail Optimizations

**Project:** Godot MCP Meow
**Researched:** 2026-03-31
**Scope:** Stack additions/changes for settings persistence, timeout mechanisms, and dual-panel logging

## No New Dependencies Required

v1.6 requires **zero new libraries or dependencies**. All needed APIs are already available in the existing stack:

- `godot-cpp v10+` -- ProjectSettings, UtilityFunctions logging (already included)
- `C++17 <chrono> + <condition_variable>` -- wait_for timeout (already included via mcp_server.h)
- `nlohmann/json 3.12.0` -- Serializing disabled-tools list to/from Variant strings (already included)

## Existing Stack (Unchanged)

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| C++17 | -- | Core language | Unchanged |
| godot-cpp | v10+ | GDExtension bindings | Unchanged |
| nlohmann/json | 3.12.0 | JSON handling | Unchanged |
| SCons | -- | Build system | Unchanged |
| GoogleTest | -- | Protocol unit tests | Unchanged |

---

## Feature 1: Settings Persistence (Port + Tool Disable)

### The Problem

The current code calls `ProjectSettings::set_setting()` but **never calls `ProjectSettings::save()`**. This means:
- Port changes from the dock SpinBox update the in-memory ProjectSettings but are NOT written to `project.godot`
- The disabled tools set (`s_disabled_tools`) is a pure in-memory `std::set<std::string>` with no persistence at all
- Both settings are lost on every editor restart

### Recommended Pattern: ProjectSettings + save()

**Use `ProjectSettings` (not `EditorSettings`)** because:
1. Port setting is already in ProjectSettings at `meow_mcp/server/port` -- changing storage would break existing users
2. Tool disable state should be per-project (different projects may need different tool subsets)
3. ProjectSettings persists to `project.godot` which is version-controlled -- team members share the same MCP config
4. EditorSettings would be per-user/per-machine, wrong granularity for "which tools does this project expose"

### API Surface (all confirmed in godot-cpp v10+ headers)

```cpp
// All from <godot_cpp/classes/project_settings.hpp>
auto* ps = ProjectSettings::get_singleton();

// Read/write settings
ps->has_setting("meow_mcp/server/port");           // bool
ps->set_setting("meow_mcp/server/port", 6800);     // void
ps->get_setting("meow_mcp/server/port");            // Variant

// Make setting visible in Project Settings UI
ps->set_initial_value("meow_mcp/server/port", 6800);  // void
ps->add_property_info(port_info);                       // void (Dictionary)

// CRITICAL: Actually write to project.godot
ps->save();  // Error return -- this is what's currently missing
```

**Confidence:** HIGH -- verified against local `godot-cpp/gen/include/godot_cpp/classes/project_settings.hpp` line 75: `Error save();`

### Persistence Pattern for Port

```cpp
void MCPPlugin::_on_port_changed(double new_port) {
    int new_port_int = static_cast<int>(new_port);
    auto* ps = ProjectSettings::get_singleton();
    if (ps) {
        ps->set_setting("meow_mcp/server/port", new_port_int);
        ps->save();  // <-- ADD THIS: persist to project.godot
    }
    // ... restart server logic unchanged ...
}
```

### Persistence Pattern for Disabled Tools

Store disabled tools as a comma-separated string in ProjectSettings. Use `nlohmann::json` array serialization or simple string join -- both work, but comma-separated string is simpler for a Variant::STRING setting.

**Setting path:** `meow_mcp/tools/disabled`

```cpp
// Save disabled tools to ProjectSettings
void save_disabled_tools() {
    auto* ps = ProjectSettings::get_singleton();
    if (!ps) return;
    
    const auto& disabled = get_disabled_tools();
    String value;
    for (const auto& name : disabled) {
        if (!value.is_empty()) value += ",";
        value += String::utf8(name.c_str());
    }
    ps->set_setting("meow_mcp/tools/disabled", value);
    ps->save();
}

// Load disabled tools from ProjectSettings (call in _enter_tree)
void load_disabled_tools() {
    auto* ps = ProjectSettings::get_singleton();
    if (!ps || !ps->has_setting("meow_mcp/tools/disabled")) return;
    
    String value = ps->get_setting("meow_mcp/tools/disabled");
    PackedStringArray parts = value.split(",", false);
    for (int i = 0; i < parts.size(); i++) {
        std::string name(parts[i].utf8().get_data());
        set_tool_disabled(name, true);
    }
}
```

### set_initial_value() Behavior

`set_initial_value()` tells the editor what the "default" is, so the Project Settings UI can show which values have been modified (non-default values appear bold). It does NOT set the actual value. The pattern is:

1. `set_initial_value("meow_mcp/server/port", 6800)` -- "6800 is the default"
2. `set_setting("meow_mcp/server/port", 6800)` -- "set the actual value to 6800" (only if not already set)
3. `add_property_info(...)` -- "show this as an integer range 1024-65535 in the UI"
4. `save()` -- "write to project.godot"

### When to Call save()

Call `save()` sparingly -- only on user-initiated actions:
- When port SpinBox value changes (user explicitly changed it)
- When a tool checkbox is toggled (user explicitly toggled it)
- NOT in `_enter_tree()` during initial setup (avoid writing project.godot on every editor open)
- NOT on every `_process()` tick

### project.godot Result

After persistence, `project.godot` will contain:

```ini
[meow_mcp]

server/port=6800
tools/disabled="capture_game_viewport,inject_input"
```

---

## Feature 2: Timeout for condition_variable Waits

### The Problem

Two timeout gaps exist:

1. **IO thread `response_cv.wait()` (line 278 of mcp_server.cpp)** -- Indefinite wait. If main thread hangs or a tool takes forever, the IO thread blocks permanently, killing the MCP connection.

2. **Game bridge deferred requests** -- When a deferred request (viewport capture, eval_in_game, etc.) is sent to the running game, there is no timeout. If the game crashes or the companion script never responds, the MCP server hangs indefinitely waiting for a response that will never come.

### Pattern 1: IO Thread Timeout (condition_variable_any::wait_for)

The codebase already uses `std::condition_variable_any` (compatible with `std::recursive_mutex`). Replace `wait()` with `wait_for()`:

```cpp
// BEFORE (current code, line 278):
response_cv.wait(lock, [this]{ return !response_queue.empty() || !running.load(); });

// AFTER (with 30-second timeout):
bool got_response = response_cv.wait_for(
    lock,
    std::chrono::seconds(30),
    [this]{ return !response_queue.empty() || !running.load(); }
);

if (!got_response) {
    // Timeout -- send error response to client
    // The request ID was saved when queuing the request
    auto timeout_error = mcp::create_error_response(
        pending_request_id, -32000, "Tool execution timed out (30s)");
    send_json(client_peer, timeout_error);
    continue;
}
```

**API details (verified from cppreference.com):**
- `wait_for` overload (2) with predicate returns `bool` -- `true` if predicate satisfied, `false` if timeout
- Uses `std::chrono::steady_clock` internally (monotonic, not affected by system clock changes)
- Handles spurious wakeups correctly (re-checks predicate)
- Works with `std::recursive_mutex` via `condition_variable_any`

**Confidence:** HIGH -- `std::condition_variable_any::wait_for` is standard C++17, already included in the project headers.

### Timeout Value Recommendation

| Timeout | Purpose | Value | Rationale |
|---------|---------|-------|-----------|
| IO thread response wait | Normal tool calls | 30 seconds | Most tools complete in <1s; 30s covers slow scene operations |
| Bridge wait (run_game) | Already has timeout | 10s default (user-configurable) | Already implemented correctly |
| Deferred game requests | viewport capture, eval, etc. | 15 seconds | Game companion should respond within a few seconds |

### Pattern 2: Deferred Game Bridge Request Timeout

The game bridge deferred requests currently have no timeout. Add a deadline pattern matching the existing `bridge_wait_deadline` approach:

```cpp
// In game_bridge.h -- add to the class:
std::chrono::steady_clock::time_point deferred_deadline;
static constexpr int DEFERRED_TIMEOUT_MS = 15000;

// When creating a deferred request (e.g., request_game_viewport_capture):
deferred_deadline = std::chrono::steady_clock::now() 
    + std::chrono::milliseconds(DEFERRED_TIMEOUT_MS);

// In MCPServer::poll() -- add deferred timeout check:
if (game_bridge && game_bridge->has_pending_deferred()) {
    if (std::chrono::steady_clock::now() >= game_bridge->get_deferred_deadline()) {
        auto timeout_response = mcp::create_tool_error_result(
            game_bridge->get_pending_id(),
            "Game bridge request timed out (15s). The game may have crashed or the companion script is not responding.");
        response_queue.push({timeout_response});
        response_cv.notify_one();
        game_bridge->clear_pending();
    }
}
```

### Implementation Note: Request ID Tracking

The IO thread currently does NOT save the request ID before waiting on `response_cv`. To generate a timeout error response, the pending request ID must be tracked. Options:

- **Option A (simple):** Save `pending_request_id` as a member before entering `wait_for`. This works because the IO thread processes requests sequentially (one at a time).
- **Option B (complex):** Use a map of in-flight request IDs. Unnecessary given the single-client, sequential architecture.

Recommend Option A -- add a `nlohmann::json pending_io_request_id` member to MCPServer.

---

## Feature 3: Dual-Panel Logging (Output + Debugger)

### Godot Editor Logging Functions (godot-cpp)

All confirmed in `godot-cpp/gen/include/godot_cpp/variant/utility_functions.hpp`:

| Function | Panel | Color | Use For |
|----------|-------|-------|---------|
| `UtilityFunctions::print(...)` | **Output** only | White/default | Informational messages, status updates |
| `UtilityFunctions::printerr(...)` | **Output** only | Red | Errors visible alongside gameplay output |
| `UtilityFunctions::print_rich(...)` | **Output** only | Custom (BBCode) | Rich-formatted output (color, bold) |
| `UtilityFunctions::push_error(...)` | **Debugger** only | Red | Errors in Debugger > Errors tab |
| `UtilityFunctions::push_warning(...)` | **Debugger** only | Yellow | Warnings in Debugger > Errors tab |

**Confidence:** HIGH -- verified both in godot-cpp headers and corroborated by Godot forum discussions and GitHub proposal #10648.

### The Key Insight: No Single Call Writes to Both Panels

There is **no** Godot function that writes to both Output and Debugger panels simultaneously. The v1.6 requirement "errors appear in both Output and Debugger panels" requires **calling two functions**:

```cpp
// To show an error in BOTH panels:
UtilityFunctions::printerr("MCP Meow: ", error_message);  // Output panel (red)
UtilityFunctions::push_error("MCP Meow: ", error_message); // Debugger panel
```

### Recommended Logging Strategy

Define a thin wrapper to enforce consistency:

```cpp
// In a shared header (e.g., mcp_log.h):
namespace mcp_log {

// Info: Output panel only (white)
template<typename... Args>
inline void info(const Args&... args) {
    godot::UtilityFunctions::print(args...);
}

// Warning: Both panels
template<typename... Args>
inline void warn(const Args&... args) {
    godot::UtilityFunctions::printerr(args...);     // Output (red -- no yellow available)
    godot::UtilityFunctions::push_warning(args...);  // Debugger (yellow)
}

// Error: Both panels
template<typename... Args>
inline void error(const Args&... args) {
    godot::UtilityFunctions::printerr(args...);    // Output (red)
    godot::UtilityFunctions::push_error(args...);  // Debugger (red)
}

} // namespace mcp_log
```

### Current Logging Audit

The codebase currently uses logging inconsistently:

| Current Usage | Count | Panel | Should Be |
|---------------|-------|-------|-----------|
| `UtilityFunctions::print(...)` | ~15 | Output only | Keep as-is (info messages) |
| `UtilityFunctions::printerr(...)` | ~5 | Output only | Change to `mcp_log::error()` for dual-panel |
| `UtilityFunctions::push_warning(...)` | 1 | Debugger only | Change to `mcp_log::warn()` for dual-panel |

### The Missing `printwarn()` Problem

Godot has **no** `printwarn()` or equivalent that prints yellow text to the Output panel. This is a known gap (see GitHub proposal #10568, #10648 -- both closed as "not planned"). Workarounds:

1. `print_rich("[color=yellow]warning text[/color]")` -- verbose, not filterable
2. `printerr(...)` -- red, not yellow, but at least visible in Output
3. Accept the limitation -- warnings go to Debugger (yellow) and Output (red via printerr)

**Recommendation:** Use `printerr()` + `push_warning()` for warnings. The color mismatch (red in Output, yellow in Debugger) is acceptable -- visibility in both panels matters more than color consistency.

---

## What NOT to Add

| Rejected Addition | Why Not |
|-------------------|---------|
| **EditorSettings** for tool disable state | Per-user, not per-project. Would break team workflows. Port is already in ProjectSettings. |
| **ConfigFile** for custom config | Adds another file to manage. ProjectSettings already works and integrates with the editor UI. |
| **Mutex per tool** for fine-grained locking | Overkill. The existing recursive_mutex + queue pattern is correct for single-client sequential processing. |
| **Async timeout library** (e.g., Boost.Asio) | C++17 `<chrono>` + `condition_variable_any::wait_for` is sufficient. Zero new dependencies. |
| **Custom log file** | Godot already captures Output and Debugger panel content. Adding file logging duplicates this. |
| **spdlog or similar** | Header-only but adds dependency. The thin `mcp_log` wrapper over Godot's built-in functions is sufficient. |

---

## Integration Considerations

### Save() Thread Safety

`ProjectSettings::save()` must be called from the **main thread only** (it writes to disk and is a Godot API). This is already satisfied since all save triggers come from UI callbacks (`_on_port_changed`, `_on_tool_toggled`) which run on the main thread.

### Timeout vs Deferred Response Interaction

When the IO thread times out waiting for a response, but the main thread later produces the response (e.g., a slow tool finishes), the late response will be pushed to `response_queue`. The IO thread should **drain stale responses** after sending a timeout error, or the stale response will be sent as a response to the *next* request.

Mitigation: After sending a timeout error, set a flag to skip the next `response_queue` drain. Or better: tag each response with its request ID and only send responses matching the current pending request.

### Backward Compatibility

- Port setting path `meow_mcp/server/port` is unchanged -- existing project.godot entries will be read correctly
- New setting `meow_mcp/tools/disabled` defaults to empty string (all tools enabled) -- backward compatible
- Timeout changes are purely additive (more resilient than infinite wait)

---

## Sources

- Local file: `godot-cpp/gen/include/godot_cpp/classes/project_settings.hpp` -- `save()` at line 75, `save_custom()` at line 77
- Local file: `godot-cpp/gen/include/godot_cpp/variant/utility_functions.hpp` -- all logging functions confirmed
- Local file: `src/mcp_server.h` -- `condition_variable_any` at line 80, `recursive_mutex` at line 77
- Local file: `src/mcp_plugin.cpp` -- current ProjectSettings usage without save()
- Local file: `src/mcp_tool_registry.cpp` -- current in-memory-only disabled tools set
- [cppreference: condition_variable_any::wait_for](https://en.cppreference.com/w/cpp/thread/condition_variable_any/wait_for.html) -- C++17 timeout API
- [Godot proposal #10648: Add printwarn()](https://github.com/godotengine/godot-proposals/issues/10648) -- confirms no Output-panel warning function exists
- [Godot proposal #10568: Add PrintWarn](https://github.com/godotengine/godot-proposals/issues/10568) -- closed as not planned
- [Godot issue #55145: ProjectSettings save inconsistency](https://github.com/godotengine/godot/issues/55145) -- known issue with Ctrl+S, not with API save()
- [Godot Forum: Plugin settings convention](https://forum.godotengine.org/t/is-there-a-convention-for-projectsettings-created-by-a-plugin/54031) -- use `addons/plugin_name/` or custom prefix
- [DeepWiki: ProjectSettings & EditorSettings](https://deepwiki.com/godotengine/godot/3.3-project-settings) -- persistence flow
