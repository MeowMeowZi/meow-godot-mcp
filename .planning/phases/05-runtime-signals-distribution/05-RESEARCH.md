# Phase 5: Runtime, Signals & Distribution - Research

**Researched:** 2026-03-17
**Domain:** Godot EditorInterface runtime control, Object signal API, godot-cpp cross-platform CI/CD
**Confidence:** HIGH

## Summary

Phase 5 covers three distinct domains: runtime control (run/stop game + capture debug output), signal management (query/connect/disconnect node signals), and cross-platform distribution (CI/CD pipeline + multi-version compatibility). Research reveals that all required APIs are available in Godot 4.3+ via `EditorInterface` and `Object` class methods, accessible through godot-cpp bindings.

The most architecturally significant finding is that Godot runs the game as a **separate subprocess** (not in-editor), which means the GDExtension plugin cannot directly intercept game stdout/stderr. The practical solution for game output capture is reading the Godot log file (`user://logs/godot.log`), which is written by the game subprocess when file logging is enabled. For distribution, godot-cpp v10's `api_version` parameter enables building a single binary targeting Godot 4.3 that is forward-compatible with 4.4, 4.5, and 4.6.

**Primary recommendation:** Implement runtime tools using `EditorInterface` singleton for play/stop, log file polling for output capture, `Object` methods for signal management, and GitHub Actions with the official godot-cpp-template workflow pattern for CI/CD.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Game run/stop control (RNTM-01, RNTM-02):** Single `run_game` tool with `mode` parameter: `"main"` | `"current"` | `"custom"`. When mode is `"custom"`, accepts optional `scene_path` parameter. Maps to EditorInterface: `play_main_scene()`, `play_current_scene()`, `play_custom_scene(path)`. When game is already running and `run_game` is called again: return success response with `"running": true` status info (not an error, not a restart). Separate `stop_game` tool.
- **Debug output capture (RNTM-03):** Batch pull model: `get_game_output` tool returns accumulated log lines. AI calls on-demand. No real-time streaming.
- **Signal management (RNTM-04, RNTM-05, RNTM-06):** Tools: `get_node_signals`, `connect_signal`, `disconnect_signal`. Query, create, and disconnect signal connections.
- **Cross-platform distribution (DIST-02, DIST-03):** Pre-compiled binaries for Windows (x86_64), Linux (x86_64), macOS (universal). GitHub Actions CI/CD pipeline. Support Godot 4.3, 4.4, 4.5, 4.6 via godot-cpp compatibility. Keep packaging flexible (user may self-host).

### Claude's Discretion
- `stop_game` wait behavior (immediate return vs wait for process exit)
- Whether to provide a separate `is_game_running` status query tool or include status in run/stop responses
- Log buffer size and history retention strategy for `get_game_output`
- `get_game_output` parameters (last_n lines, since_last_call, clear_after_read, etc.)
- Signal query return data richness (signal names, parameter types, connected targets, custom signal detection)
- `connect_signal` parameter scope (basic source+signal+target+method vs full Callable with binds/flags)
- CI build matrix strategy (single godot-cpp version vs multi-version matrix based on ABI compatibility research)
- Release artifact format (GitHub Releases zip with addons/ directory structure)
- `run_game` and `stop_game` response JSON structure

### Deferred Ideas (OUT OF SCOPE)
- Real-time log streaming via MCP notifications -- v1 uses batch pull, streaming if users request
- Game input injection (keyboard/mouse/Action events) -- ADVR-01, v2 requirement
- Viewport screenshot capture -- ADVR-02, v2 requirement
- Asset Library submission -- evaluate after v1 stability proven
- Scheduled/automated testing across Godot versions -- add when user base grows
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| RNTM-01 | AI can start current project in debug mode | `EditorInterface::play_main_scene()`, `play_current_scene()`, `play_custom_scene()` -- all available in Godot 4.3+ via godot-cpp |
| RNTM-02 | AI can stop running project instance | `EditorInterface::stop_playing_scene()` + `is_playing_scene()` for status -- available in 4.3+ |
| RNTM-03 | AI can capture game runtime stdout/stderr log output | Log file polling approach (`user://logs/godot.log`) -- cross-version compatible since game runs as separate subprocess |
| RNTM-04 | AI can query node signals (defined + connected) | `Object::get_signal_list()` + `get_signal_connection_list()` -- returns Array of Dictionaries, available 4.3+ |
| RNTM-05 | AI can create signal connections between nodes | `Object::connect(signal, callable, flags)` -- available 4.3+ |
| RNTM-06 | AI can disconnect signal connections | `Object::disconnect(signal, callable)` -- available 4.3+ |
| DIST-02 | Pre-compiled binaries for Win/Linux/macOS | GitHub Actions matrix build with SCons + lipo for macOS universal |
| DIST-03 | Support Godot 4.3-4.6 via godot-cpp compatibility | godot-cpp v10 `api_version=4.3` produces binary forward-compatible with 4.4/4.5/4.6 |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10.x (10.0.0-rc1+) | C++ GDExtension bindings | Already in use, `api_version` param enables multi-version targeting |
| nlohmann/json | 3.12.0 | JSON serialization | Already in use, zero-dependency header-only |
| GoogleTest | 1.17.0 | Unit testing | Already in use for protocol/registry tests |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| GitHub Actions | N/A | CI/CD pipeline | Cross-platform automated builds |
| SCons | 4.x | Build system | Already the project build system |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Log file polling | EditorDebuggerPlugin IPC | EditorDebuggerPlugin requires autoload script in game + complex 3-script setup; log file is simpler and works across all Godot 4.3+ |
| Log file polling | Godot 4.5 Logger class | Logger only intercepts editor-process messages; game runs as separate subprocess so Logger cannot capture game output |
| Single `api_version=4.3` build | Multi-version build matrix | Single build is simpler; multi-version matrix only needed if version-specific APIs are required |

**Installation:** No new dependencies required. All APIs come from godot-cpp bindings and Godot engine.

## Architecture Patterns

### Recommended Project Structure
```
src/
  runtime_tools.h    # run_game, stop_game, get_game_output + log capture
  runtime_tools.cpp  # EditorInterface calls + log file reader
  signal_tools.h     # get_node_signals, connect_signal, disconnect_signal
  signal_tools.cpp   # Object signal API wrappers
.github/
  workflows/
    builds.yml       # Cross-platform CI/CD
```

### Pattern 1: EditorInterface Singleton Access for Runtime Control
**What:** Access EditorInterface via `EditorInterface::get_singleton()` for play/stop scene control.
**When to use:** All runtime tools (run_game, stop_game, is_game_running).
**Example:**
```cpp
// Source: Godot 4.3+ EditorInterface API (docs.godotengine.org)
#include <godot_cpp/classes/editor_interface.hpp>

nlohmann::json run_game(const std::string& mode, const std::string& scene_path) {
    auto* ei = godot::EditorInterface::get_singleton();
    if (!ei) {
        return {{"success", false}, {"error", "EditorInterface not available"}};
    }

    // Check if already running
    if (ei->is_playing_scene()) {
        godot::String playing = ei->get_playing_scene();
        return {{"success", true}, {"running", true},
                {"scene", std::string(playing.utf8().get_data())}};
    }

    if (mode == "main") {
        ei->play_main_scene();
    } else if (mode == "current") {
        ei->play_current_scene();
    } else if (mode == "custom") {
        ei->play_custom_scene(godot::String(scene_path.c_str()));
    }

    return {{"success", true}, {"running", true}, {"mode", mode}};
}
```

### Pattern 2: Log File Polling for Game Output Capture
**What:** Read Godot's log file to capture game stdout/stderr output. The game runs as a separate subprocess, so the editor plugin cannot directly intercept its output.
**When to use:** `get_game_output` tool implementation.
**Critical insight:** Godot editor spawns the game as a child process (`godot --path <project> --remote-debug 127.0.0.1:<port>`). The game writes to `user://logs/godot.log` when file logging is enabled. The editor plugin reads this file.
**Example:**
```cpp
// Source: Godot file logging system (docs.godotengine.org/en/stable/tutorials/scripting/logging.html)
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>

// Track file position for incremental reads
static int64_t last_log_position = 0;

nlohmann::json get_game_output(bool clear_after_read) {
    // Resolve log file path from project settings
    godot::String log_path = "user://logs/godot.log";

    if (!godot::FileAccess::file_exists(log_path)) {
        return {{"success", true}, {"lines", nlohmann::json::array()},
                {"message", "No log file found"}};
    }

    auto file = godot::FileAccess::open(log_path, godot::FileAccess::READ);
    if (!file.is_valid()) {
        return {{"success", false}, {"error", "Cannot open log file"}};
    }

    // Seek to last read position for incremental reads
    file->seek(last_log_position);
    nlohmann::json lines = nlohmann::json::array();
    while (!file->eof_reached()) {
        godot::String line = file->get_line();
        if (line.length() > 0) {
            lines.push_back(std::string(line.utf8().get_data()));
        }
    }

    if (clear_after_read) {
        last_log_position = file->get_position();
    }

    return {{"success", true}, {"lines", lines}, {"count", lines.size()}};
}
```

### Pattern 3: Object Signal API for Signal Management
**What:** Use `Object` methods to query, connect, and disconnect signals on scene nodes.
**When to use:** All signal tools (get_node_signals, connect_signal, disconnect_signal).
**Example:**
```cpp
// Source: Godot Object API (docs.godotengine.org/en/stable/classes/class_object.html)
#include <godot_cpp/classes/node.hpp>

nlohmann::json get_node_signals(godot::Node* node) {
    nlohmann::json result = nlohmann::json::array();

    // get_signal_list() returns Array<Dictionary>
    // Each dict has: "name" (StringName), "args" (Array of param dicts)
    godot::TypedArray<godot::Dictionary> signals = node->get_signal_list();

    for (int i = 0; i < signals.size(); i++) {
        godot::Dictionary sig = signals[i];
        godot::String name = sig["name"];
        nlohmann::json sig_json;
        sig_json["name"] = std::string(name.utf8().get_data());

        // Get connections for this signal
        godot::TypedArray<godot::Dictionary> conns =
            node->get_signal_connection_list(sig["name"]);
        sig_json["connection_count"] = conns.size();

        // Extract argument info
        godot::Array args = sig["args"];
        nlohmann::json args_json = nlohmann::json::array();
        for (int j = 0; j < args.size(); j++) {
            godot::Dictionary arg = args[j];
            args_json.push_back({
                {"name", std::string(godot::String(arg["name"]).utf8().get_data())},
                {"type", (int)arg["type"]}
            });
        }
        sig_json["args"] = args_json;

        result.push_back(sig_json);
    }
    return result;
}
```

### Pattern 4: GitHub Actions Matrix Build
**What:** Cross-platform CI/CD using the official godot-cpp-template pattern.
**When to use:** DIST-02 (pre-compiled binaries).
**Example:**
```yaml
# Source: github.com/godotengine/godot-cpp-template/.github/workflows/builds.yml
name: Build GDExtension
on: [push, pull_request]
jobs:
  build:
    strategy:
      matrix:
        include:
          - platform: linux
            os: ubuntu-22.04
            arch: x86_64
          - platform: windows
            os: windows-latest
            arch: x86_64
          - platform: macos
            os: macos-latest
            arch: universal
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Setup SCons
        run: pip install scons
      - name: Build GDExtension
        run: scons platform=${{ matrix.platform }} target=template_release arch=${{ matrix.arch }}
      - name: Build Bridge
        run: scons bridge
      - uses: actions/upload-artifact@v4
        with:
          name: godot-mcp-meow-${{ matrix.platform }}
          path: project/addons/godot_mcp_meow/
```

### Anti-Patterns to Avoid
- **DO NOT try to capture game stdout directly from the editor process:** The game runs as a separate subprocess. The editor's Logger/OS.add_logger only intercepts editor-process messages, not game output.
- **DO NOT use EditorDebuggerPlugin for log capture:** It requires a 3-script setup (EditorPlugin + EditorDebuggerPlugin + game-side autoload) and adds complexity. The log file approach is simpler and more reliable.
- **DO NOT build separate binaries per Godot version:** godot-cpp v10 `api_version=4.3` produces a single binary forward-compatible with 4.4/4.5/4.6. Building per-version is unnecessary overhead.
- **DO NOT use `Object::connect()` with lambda/inline callables from GDExtension:** The `Callable` must be constructed from `Object*` + `StringName` method name. Lambdas are not supported across the GDExtension boundary.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Scene play/stop control | Custom process spawning | `EditorInterface::play_main_scene/stop_playing_scene` | Editor manages game lifecycle, handles debug mode, remote debugging |
| Game output capture | Pipe redirection or IPC | Log file reading via `FileAccess` | Godot already writes to `user://logs/godot.log`; just read it |
| Signal introspection | Custom reflection system | `Object::get_signal_list()` | Godot provides complete signal metadata including parameter types |
| Cross-platform builds | Custom build scripts | GitHub Actions + SCons + godot-cpp-template pattern | Proven pattern used by the official template and ecosystem |
| macOS universal binary | Manual architecture merging | SCons `arch=universal` or `lipo` in CI | SCons handles this with `generate_bundle=yes` flag |

**Key insight:** All required functionality exists in Godot's public API. The runtime and signal tools are wrappers around existing EditorInterface and Object methods. The distribution work is build system configuration, not code.

## Common Pitfalls

### Pitfall 1: Game Runs as Separate Subprocess
**What goes wrong:** Developer assumes game output is available in the editor process, tries to use Logger or direct stdout capture.
**Why it happens:** Other engines (Unity, Unreal) run the game in-process. Godot explicitly does NOT -- it spawns a child process for isolation.
**How to avoid:** Use log file polling. Enable file logging in project settings (`debug/file_logging/enable_file_logging`).
**Warning signs:** Empty output from Logger, no intercepted messages despite game printing text.

### Pitfall 2: File Logging Must Be Enabled
**What goes wrong:** `get_game_output` returns empty results because the project doesn't have file logging enabled.
**Why it happens:** File logging is disabled by default in Godot project settings.
**How to avoid:** The `run_game` tool should check and potentially auto-enable `debug/file_logging/enable_file_logging` via ProjectSettings, or at minimum warn the user.
**Warning signs:** Log file doesn't exist at `user://logs/godot.log`.

### Pitfall 3: Log File Contains Editor AND Game Output
**What goes wrong:** `get_game_output` returns editor startup messages mixed with game output.
**Why it happens:** Godot writes both editor and game messages to the same log file.
**How to avoid:** Track the log file position at the moment `run_game` is called. Only return lines written after that point.
**Warning signs:** Output includes "MCP Meow: Server started" or other editor-only messages.

### Pitfall 4: StringName/String Conversion in Signal API
**What goes wrong:** Passing `std::string` directly to `Object::connect()` or signal methods.
**Why it happens:** godot-cpp v10 requires explicit `StringName` or `String` construction.
**How to avoid:** Always construct `godot::StringName(signal_name.c_str())` for signal names, `godot::String(method_name.c_str())` for method names.
**Warning signs:** Compilation errors about no conversion from `std::string` to `StringName`.

### Pitfall 5: Callable Construction for Signal Connections
**What goes wrong:** Trying to use C++ lambdas or function pointers with `Object::connect()`.
**Why it happens:** GDExtension boundary requires `Callable(Object*, const StringName&)` format.
**How to avoid:** Use `godot::Callable(target_node, godot::StringName(method_name.c_str()))` explicitly.
**Warning signs:** Runtime crashes, "Invalid Callable" errors.

### Pitfall 6: get_signal_connection_list Known Bug
**What goes wrong:** `get_signal_connection_list()` may return incomplete results for code-connected signals.
**Why it happens:** Known Godot bug (Issue #94791) where connections made via code may not appear in the list on some versions.
**How to avoid:** Use both `get_signal_connection_list()` for specific signals AND `get_incoming_connections()` for comprehensive results. Test on target Godot versions.
**Warning signs:** Missing connections that are known to exist.

### Pitfall 7: macOS Code Signing for CI Builds
**What goes wrong:** macOS binaries fail Gatekeeper checks or show "unsigned" warnings.
**Why it happens:** GitHub Actions macOS runners can build but not sign binaries by default.
**How to avoid:** For v1, skip code signing -- unsigned GDExtensions work fine in Godot editor (not a sandboxed app). Document this limitation.
**Warning signs:** User reports about macOS security dialogs.

### Pitfall 8: Bridge Executable Not Built in CI
**What goes wrong:** Release artifacts include the GDExtension library but missing the bridge executable.
**Why it happens:** Bridge is a separate SCons target (`scons bridge`) not included in default build.
**How to avoid:** CI workflow must explicitly build both targets: `scons platform=X target=template_release` AND `scons bridge`.
**Warning signs:** Users download release, bridge is missing, MCP connection fails.

## Code Examples

Verified patterns from official sources and established project conventions:

### EditorInterface Access (Godot 4.3+)
```cpp
// Source: docs.godotengine.org/en/4.3/classes/class_editorinterface.html
// All these methods are available via godot-cpp bindings since Godot 4.3
#include <godot_cpp/classes/editor_interface.hpp>

auto* ei = godot::EditorInterface::get_singleton();

// Play scenes
ei->play_main_scene();           // F5 equivalent
ei->play_current_scene();         // F6 equivalent
ei->play_custom_scene("res://test.tscn");  // F7 with path

// Query state
bool playing = ei->is_playing_scene();
godot::String scene = ei->get_playing_scene();  // empty if not playing

// Stop
ei->stop_playing_scene();         // F8 equivalent
```

### Signal Query and Connection
```cpp
// Source: docs.godotengine.org/en/stable/classes/class_object.html
// Object signal methods available since Godot 4.0

// Query signals on a node
godot::TypedArray<godot::Dictionary> signals = node->get_signal_list();
// Each dict: {"name": StringName, "args": Array<Dictionary>}
// Each arg dict: {"name": String, "type": int (Variant::Type)}

// Query connections for specific signal
godot::TypedArray<godot::Dictionary> conns =
    node->get_signal_connection_list(godot::StringName("pressed"));
// Each dict: {"signal": Signal, "callable": Callable, "flags": int}

// Connect signal: source.signal -> target.method
godot::Error err = source_node->connect(
    godot::StringName("body_entered"),
    godot::Callable(target_node, godot::StringName("_on_body_entered")),
    0  // flags: 0=default, CONNECT_DEFERRED=1, CONNECT_PERSIST=2, CONNECT_ONE_SHOT=4
);

// Disconnect
source_node->disconnect(
    godot::StringName("body_entered"),
    godot::Callable(target_node, godot::StringName("_on_body_entered"))
);

// Check if connected
bool connected = source_node->is_connected(
    godot::StringName("body_entered"),
    godot::Callable(target_node, godot::StringName("_on_body_entered"))
);
```

### Log File Reading
```cpp
// Source: Godot FileAccess API + logging docs
#include <godot_cpp/classes/file_access.hpp>

godot::String log_path = "user://logs/godot.log";
if (godot::FileAccess::file_exists(log_path)) {
    auto file = godot::FileAccess::open(log_path, godot::FileAccess::READ);
    file->seek(last_position);  // Resume from last read
    while (!file->eof_reached()) {
        godot::String line = file->get_line();
        // Process line...
    }
    last_position = file->get_position();
}
```

### Tool Registration Pattern (Established)
```cpp
// Source: Existing mcp_tool_registry.cpp pattern
// New tools follow exact same pattern as existing tools
{
    "run_game",
    "Run the game in debug mode. Modes: 'main' (F5), 'current' (F6), 'custom' (specify scene_path).",
    {
        {"type", "object"},
        {"properties", {
            {"mode", {{"type", "string"}, {"enum", {"main", "current", "custom"}},
                      {"description", "Play mode: main scene, current scene, or custom scene"}}},
            {"scene_path", {{"type", "string"},
                           {"description", "Scene path for custom mode (e.g., res://levels/test.tscn)"}}}
        }},
        {"required", {"mode"}}
    },
    {4, 3, 0}  // min_version: available on all supported Godot versions
},
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate godot-cpp versions per Godot | godot-cpp v10 with `api_version` param | godot-cpp 10.0 (2025) | Single build can target 4.3 and work on 4.4/4.5/4.6 |
| No log interception | Godot 4.5 Logger class | Godot 4.5 (PR #91006) | Native log interception possible, but only in-process (editor side) |
| No play/stop signals | Proposed `scene_started`/`scene_stopped` signals (PR #103056) | Not yet merged | Polling `is_playing_scene()` remains the only approach |

**Deprecated/outdated:**
- godot-cpp version branches (e.g., `godot-4.3-stable`): Still work but `api_version` param on v10 is the modern approach
- Direct stdout capture from game process: Never worked reliably in Godot 4 (especially on Windows per Issue #78114)

## Open Questions

1. **Log file timing vs `play_main_scene()` return**
   - What we know: `play_main_scene()` returns void and is likely asynchronous (spawns subprocess).
   - What's unclear: Exact timing between `play_main_scene()` call and when the game subprocess starts writing to the log file.
   - Recommendation: Record file position immediately before calling play, then poll with small delay. Include a "game not started yet" state in `get_game_output`.

2. **File logging auto-enable**
   - What we know: File logging is off by default. `ProjectSettings::set_setting()` can change it.
   - What's unclear: Whether changing the setting takes effect for the next game launch without restarting the editor.
   - Recommendation: Check setting on `run_game`, warn in response if disabled. Consider auto-enabling and saving.

3. **Log file path across platforms**
   - What we know: Default is `user://logs/godot.log`. Users can customize it in project settings.
   - What's unclear: Whether `user://` resolves correctly from the editor process context for reading game logs.
   - Recommendation: Use `ProjectSettings` to read the configured log path. Fall back to `user://logs/godot.log`.

4. **Bridge build in CI for all platforms**
   - What we know: Bridge uses plain C++ (no godot-cpp). Current SConstruct builds it on Windows with ws2_32 and Linux with pthread.
   - What's unclear: macOS bridge build requirements (likely just pthread, but untested).
   - Recommendation: Test bridge build on macOS runner. May need `#ifdef __APPLE__` for platform-specific socket includes.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 + Python UAT scripts |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd tests/build && ctest --output-on-failure` |
| Full suite command | `cd tests/build && cmake .. && cmake --build . && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| RNTM-01 | run_game tool dispatches correctly | unit | `ctest -R test_tool_registry --output-on-failure` | Wave 0 (new tool entries) |
| RNTM-02 | stop_game tool dispatches correctly | unit | `ctest -R test_tool_registry --output-on-failure` | Wave 0 (new tool entries) |
| RNTM-03 | get_game_output returns log lines | UAT | `python tests/uat_phase5.py` | Wave 0 |
| RNTM-04 | get_node_signals returns signal list | UAT | `python tests/uat_phase5.py` | Wave 0 |
| RNTM-05 | connect_signal creates connection | UAT | `python tests/uat_phase5.py` | Wave 0 |
| RNTM-06 | disconnect_signal removes connection | UAT | `python tests/uat_phase5.py` | Wave 0 |
| DIST-02 | CI builds for 3 platforms | CI | GitHub Actions pipeline | Wave 0 |
| DIST-03 | Binary works on 4.3-4.6 | manual-only | Manual: load plugin in each Godot version | N/A (requires multiple Godot installs) |

### Sampling Rate
- **Per task commit:** `cd tests/build && ctest --output-on-failure`
- **Per wave merge:** Full GoogleTest suite + UAT against live Godot editor
- **Phase gate:** Full suite green + CI pipeline green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_tool_registry.cpp` -- add new tool entries (run_game, stop_game, get_game_output, get_node_signals, connect_signal, disconnect_signal)
- [ ] `tests/uat_phase5.py` -- UAT covering runtime tools + signal tools against live Godot
- [ ] `.github/workflows/builds.yml` -- CI pipeline for cross-platform builds

## Sources

### Primary (HIGH confidence)
- [EditorInterface 4.3 docs](https://docs.godotengine.org/en/4.3/classes/class_editorinterface.html) - play/stop/is_playing scene methods confirmed in 4.3
- [EditorInterface stable docs](https://docs.godotengine.org/en/stable/classes/class_editorinterface.html) - full method listing
- [Godot engine source: editor_interface.cpp](https://github.com/godotengine/godot/blob/master/editor/editor_interface.cpp) - implementation delegates to EditorRunBar
- [Object signal methods (rokojori docs)](https://rokojori.com/en/labs/godot/docs/4.3/object-class) - get_signal_list, connect, disconnect signatures
- [godot-cpp repository](https://github.com/godotengine/godot-cpp) - v10 independent versioning, api_version parameter
- [godot-cpp-template CI workflow](https://github.com/godotengine/godot-cpp-template/blob/main/.github/workflows/builds.yml) - official cross-platform build matrix

### Secondary (MEDIUM confidence)
- [Godot logging docs](https://docs.godotengine.org/en/stable/tutorials/scripting/logging.html) - file logging configuration
- [Game subprocess architecture](https://github.com/godotengine/godot-proposals/issues/7213) - confirmed game runs as separate process
- [Godot Issue #78114](https://github.com/godotengine/godot/issues/78114) - game process STDOUT not captured by editor
- [NodotProject/godot-cpp-builds](https://github.com/NodotProject/godot-cpp-builds) - pre-built godot-cpp binaries for CI
- [Godot Forum: Logger class tutorial](https://forum.godotengine.org/t/how-to-use-the-new-logger-class-in-godot-4-5/127006) - Logger only intercepts in-process messages

### Tertiary (LOW confidence)
- [Godot Issue #94791](https://github.com/godotengine/godot/issues/94791) - get_signal_connection_list may miss code-connected signals (unverified if fixed)
- [Godot Issue #97066](https://github.com/godotengine/godot/issues/97066) - log file rotation bug in 4.3+ (may affect log reading)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all APIs exist in Godot 4.3+ and are verified in official docs
- Architecture: HIGH - patterns follow existing project conventions and use proven Godot APIs
- Runtime tools: HIGH - EditorInterface play/stop API is stable, well-documented
- Log capture: MEDIUM - log file approach is proven but has edge cases (timing, file logging enablement, path resolution)
- Signal tools: HIGH - Object signal API is core Godot, stable since 4.0
- Distribution: MEDIUM - godot-cpp-template CI pattern is standard but project-specific bridge executable adds complexity
- Pitfalls: HIGH - based on official bug reports, documented limitations, and ecosystem patterns

**Research date:** 2026-03-17
**Valid until:** 2026-04-17 (30 days - stable APIs, mature ecosystem)
