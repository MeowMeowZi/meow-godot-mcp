# Phase 4: Editor Integration - Research

**Researched:** 2026-03-17
**Domain:** Godot EditorPlugin dock UI, MCP Prompts protocol, version detection, godot-cpp programmatic UI
**Confidence:** HIGH

## Summary

Phase 4 adds four capabilities: a dock panel showing MCP connection status (EDIT-01), server start/stop/restart controls (EDIT-02), version-aware tool filtering (EDIT-03), and MCP Prompt templates (EDIT-04). The Godot APIs needed are well-established -- `add_control_to_dock()` has been stable since Godot 3.x and remains the correct API for our 4.3+ target. Programmatic UI creation in godot-cpp uses `memnew()` + `add_child()` for VBoxContainer/Label/Button hierarchies, following the same pattern used in Godot's own engine source code.

The critical architectural finding is that **signal connection from UI buttons requires the callback target to be a Godot Object**. Since MCPDock is a plain C++ class (not registered to ClassDB), button signals must be connected to MCPPlugin (which IS a Godot Object via EditorPlugin), with MCPPlugin delegating to MCPDock/MCPServer. This is the same pattern used for MCPServer today -- MCPPlugin owns and delegates to it.

The MCP Prompts protocol (spec 2025-03-26) is straightforward: add `prompts` capability to the initialize response, implement `prompts/list` and `prompts/get` handlers. The protocol layer remains pure C++ (no Godot dependency), keeping it unit-testable with GoogleTest.

**Primary recommendation:** Build MCPDock as a plain C++ class that creates and manages a VBoxContainer tree. Wire button signals through MCPPlugin using `callable_mp(this, &MCPPlugin::_on_X)`. Refactor tool schemas from hardcoded JSON in `create_tools_list_response()` to a `ToolDef` array with `min_version` fields. Add prompts protocol builders to `mcp_protocol.h/cpp`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Right-side dock using `add_control_to_dock(DOCK_SLOT_RIGHT_BL, ...)`
- Pure C++ UI built with godot-cpp (VBoxContainer, Label, Button, etc.)
- MCPDock class: plain C++ wrapper, NOT registered to ClassDB -- manages VBoxContainer internally
- MCPPlugin creates and owns MCPDock via raw pointer (same pattern as MCPServer)
- Status display: connection state, listening port, Godot version, tool count -- NO request log
- 2 buttons: toggle Start/Stop + separate Restart
- Polling via timer: accumulate delta in `_process()`, check every 0.5-1s, dirty check
- Version detection via `Engine.get_version_info()` -- ToolDef struct with min_version filtering
- Currently all tools min_version 4.3 -- framework for Phase 5
- Prompts: parameterized templates, 3-4 core workflows, hardcoded in C++
- MCP prompts capability + prompts/list + prompts/get methods needed

### Claude's Discretion
- Exact UI layout spacing, font sizes, colors for dock panel
- Specific prompt template content and parameter names
- ToolDef data structure details and where to store the registry
- How to expose MCPServer state to MCPDock (direct pointer, getter methods, etc.)
- Timer interval precision (0.5s vs 1s)

### Deferred Ideas (OUT OF SCOPE)
- Request log/history display in dock panel
- Client name display from MCP initialize handshake
- User-configurable/extensible prompt templates via external JSON
- Port configuration UI in dock panel
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| EDIT-01 | Editor dock panel shows MCP connection status (connected/disconnected/waiting) | Dock API via `add_control_to_dock()` verified in godot-cpp headers. Status polling pattern via `_process()` delta accumulation. MCPServer already exposes `is_running()` and client connection state. |
| EDIT-02 | Dock panel provides MCP server start/stop/restart buttons | Button signal connection pattern researched -- must use `callable_mp(plugin, &MCPPlugin::method)` since MCPDock is not a Godot Object. MCPServer already has `start()`/`stop()` lifecycle methods. |
| EDIT-03 | Runtime detection of Godot version, dynamically enable/disable MCP tools by version | `Engine::get_singleton()->get_version_info()` returns Dictionary with major/minor/patch/status/build/string keys. ToolDef struct with min_version filtering replaces hardcoded tool list. |
| EDIT-04 | Pre-built MCP Prompt templates for common workflows | MCP prompts spec fully researched: `prompts` capability, `prompts/list`, `prompts/get` with arguments. Protocol builders follow existing pattern from resources protocol. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ (matches project) | C++ bindings for Godot API (UI widgets, Engine singleton, EditorPlugin) | Already in project, provides all needed UI classes |
| nlohmann/json | 3.12.0 (already in project) | JSON construction for prompts protocol responses | Already used throughout mcp_protocol.cpp |

### Supporting
No additional libraries needed. All UI is built with godot-cpp built-in classes.

## Architecture Patterns

### Recommended New File Structure
```
src/
  mcp_dock.h          # MCPDock class declaration
  mcp_dock.cpp        # MCPDock UI construction and update logic
  mcp_prompts.h       # Prompt template definitions and MCP prompts protocol helpers
  mcp_prompts.cpp     # Prompt content generation and protocol builders
  mcp_tool_registry.h # ToolDef struct, tool registry, version-filtered listing
  mcp_tool_registry.cpp # Tool schema definitions and version filtering logic
```

### Pattern 1: MCPDock as Plain C++ Wrapper

**What:** MCPDock is a plain C++ class (NOT a Godot Object) that creates and manages a VBoxContainer hierarchy. It holds raw pointers to its child widgets for updates.

**When to use:** Same ownership pattern as MCPServer -- plain C++ class owned by MCPPlugin via raw pointer.

**Key constraint:** Since MCPDock is NOT a Godot Object, it does NOT have `get_instance_id()`. This means `callable_mp(dock, &MCPDock::method)` will NOT compile. Button signals must be connected to MCPPlugin instead.

**Example:**
```cpp
// Source: godot-cpp gen headers + engine source patterns
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_separator.hpp>

class MCPDock {
public:
    MCPDock();
    ~MCPDock();

    // Returns the root Control to pass to add_control_to_dock()
    godot::VBoxContainer* get_root() const { return root; }

    // Update status display (called from MCPPlugin::_process via timer)
    void update_status(bool server_running, bool client_connected,
                       int port, const std::string& godot_version,
                       int tool_count);

    // Update button state based on server state
    void update_buttons(bool server_running);

private:
    godot::VBoxContainer* root = nullptr;
    godot::Label* status_label = nullptr;
    godot::Label* port_label = nullptr;
    godot::Label* version_label = nullptr;
    godot::Label* tools_label = nullptr;
    godot::Button* toggle_button = nullptr;
    godot::Button* restart_button = nullptr;
};
```

### Pattern 2: UI Construction with memnew()

**What:** All Godot nodes are allocated with `memnew()` and assembled via `add_child()`. The root VBoxContainer's name becomes the dock tab title.

**When to use:** Any programmatic UI in godot-cpp.

**Example:**
```cpp
// Source: Godot engine internal patterns (AcceptDialog, etc.)
MCPDock::MCPDock() {
    root = memnew(godot::VBoxContainer);
    root->set_name("MCP Meow");  // This becomes the dock tab title

    // Status section
    status_label = memnew(godot::Label);
    status_label->set_text("Status: Disconnected");
    root->add_child(status_label);

    port_label = memnew(godot::Label);
    port_label->set_text("Port: 6800");
    root->add_child(port_label);

    version_label = memnew(godot::Label);
    version_label->set_text("Godot: detecting...");
    root->add_child(version_label);

    tools_label = memnew(godot::Label);
    tools_label->set_text("Tools: 0");
    root->add_child(tools_label);

    // Separator
    auto* sep = memnew(godot::HSeparator);
    root->add_child(sep);

    // Buttons
    auto* btn_box = memnew(godot::HBoxContainer);
    root->add_child(btn_box);

    toggle_button = memnew(godot::Button);
    toggle_button->set_text("Stop");
    btn_box->add_child(toggle_button);

    restart_button = memnew(godot::Button);
    restart_button->set_text("Restart");
    btn_box->add_child(restart_button);
}
```

### Pattern 3: Signal Connection via MCPPlugin (Godot Object)

**What:** Button signals connect to MCPPlugin methods using `callable_mp()`. MCPPlugin delegates to MCPServer/MCPDock. This is required because `callable_mp` needs the target to be a Godot Object with `get_instance_id()`.

**When to use:** Any signal from UI widgets in MCPDock.

**Example:**
```cpp
// Source: godot-cpp callable_method_pointer.hpp, test/src/example.cpp
#include <godot_cpp/variant/callable_method_pointer.hpp>

// In MCPPlugin::_enter_tree():
void MCPPlugin::_enter_tree() {
    server = new MCPServer();
    server->set_undo_redo(get_undo_redo());
    server->start(port);

    dock = new MCPDock();
    // Connect button signals TO MCPPlugin (which IS a Godot Object)
    dock->get_toggle_button()->connect("pressed",
        callable_mp(this, &MCPPlugin::_on_toggle_pressed));
    dock->get_restart_button()->connect("pressed",
        callable_mp(this, &MCPPlugin::_on_restart_pressed));

    add_control_to_dock(DOCK_SLOT_RIGHT_BL, dock->get_root());
    set_process(true);
}

void MCPPlugin::_on_toggle_pressed() {
    if (server->is_running()) {
        server->stop();
    } else {
        server->start(port);
    }
    dock->update_buttons(server->is_running());
}

void MCPPlugin::_on_restart_pressed() {
    server->stop();
    server->start(port);
    dock->update_buttons(server->is_running());
}

// In MCPPlugin::_exit_tree():
void MCPPlugin::_exit_tree() {
    if (dock) {
        remove_control_from_docks(dock->get_root());
        // memdelete the root VBoxContainer (Godot will clean up children)
        memdelete(dock->get_root());
        delete dock;
        dock = nullptr;
    }
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
}
```

### Pattern 4: Timer-Based Status Polling

**What:** Accumulate delta in `_process()`, check MCPServer state every ~1 second, only update UI when state changes.

**When to use:** Polling server state without adding callback infrastructure to MCPServer.

**Example:**
```cpp
void MCPPlugin::_process(double delta) {
    if (server && server->is_running()) {
        server->poll();
    }

    // Status polling timer
    status_timer += delta;
    if (status_timer >= 1.0 && dock) {
        status_timer = 0.0;
        bool running = server && server->is_running();
        bool connected = server && server->has_client();
        dock->update_status(running, connected, port, godot_version, tool_count);
        dock->update_buttons(running);
    }
}
```

### Pattern 5: ToolDef Registry with Version Filtering

**What:** Replace hardcoded JSON tool schemas in `create_tools_list_response()` with a `ToolDef` struct array. Each ToolDef has name, description, inputSchema, and min_version. At initialization, filter tools by detected Godot version.

**When to use:** EDIT-03 version-aware tool filtering.

**Example:**
```cpp
// Source: project architecture decision
struct GodotVersion {
    int major;
    int minor;
    int patch;

    bool operator>=(const GodotVersion& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch >= other.patch;
    }
};

struct ToolDef {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
    GodotVersion min_version;  // {4, 3, 0} for all current tools
};

// Global/static registry
const std::vector<ToolDef>& get_all_tools();

// Filtered list based on detected version
nlohmann::json create_tools_list_response(const nlohmann::json& id,
                                           const GodotVersion& current_version);
```

### Pattern 6: MCP Prompts Protocol

**What:** Add `prompts` capability to initialize response, implement `prompts/list` and `prompts/get` handlers following the same pure C++ pattern as existing protocol builders.

**When to use:** EDIT-04 prompt templates.

**Example:**
```cpp
// Source: MCP spec 2025-03-26 prompts page
// In mcp_protocol.h:
nlohmann::json create_prompts_list_response(const nlohmann::json& id);
nlohmann::json create_prompt_get_response(const nlohmann::json& id,
    const std::string& description,
    const nlohmann::json& messages);
nlohmann::json create_prompt_not_found_error(const nlohmann::json& id,
    const std::string& prompt_name);
```

### Anti-Patterns to Avoid
- **Registering MCPDock to ClassDB:** The context decision explicitly states MCPDock is NOT a Godot Object. Do not use `GDCLASS()` or `GDREGISTER_CLASS()` for it. This keeps it simple and avoids unnecessary ClassDB overhead.
- **Using `callable_mp(dock, ...)` on a plain C++ object:** Will fail to compile because `get_instance_id()` is not available. Always route signals through MCPPlugin.
- **Using the new `add_dock()` / `EditorDock` API:** This was introduced in Godot 4.6 development branch. Since we target Godot 4.3+, we MUST use `add_control_to_dock()` which is stable across 4.3, 4.4, and 4.5.
- **Calling Godot API from MCPDock constructor:** Node allocation with `memnew()` is safe anywhere, but do not call methods that require the scene tree (like `get_tree()`) from the constructor. Construction happens in `_enter_tree()` which is safe.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON-RPC protocol | Custom JSON serializer | nlohmann/json (already used) | Handles all edge cases, well-tested |
| UI layout/widgets | Custom drawing or rendering | godot-cpp VBoxContainer/Label/Button | Native Godot widgets, consistent with editor theme |
| Version parsing | Custom string parser | `Engine::get_singleton()->get_version_info()` | Returns pre-parsed Dictionary with int major/minor/patch |
| Dock positioning | Custom window management | `add_control_to_dock()` | Godot handles persistence, layout, and tab management |

**Key insight:** The entire dock UI uses standard Godot Control nodes. No custom rendering or drawing needed -- just compose existing widgets programmatically.

## Common Pitfalls

### Pitfall 1: callable_mp on Non-Object Types
**What goes wrong:** Trying to use `callable_mp(dock_ptr, &MCPDock::method)` where MCPDock is a plain C++ class causes a compile error because `CallableCustomMethodPointer` calls `data.instance->get_instance_id()`.
**Why it happens:** `callable_mp` template requires the instance type to be a Godot Object subclass.
**How to avoid:** Route all signal connections through MCPPlugin (which extends EditorPlugin, a Godot Object). MCPPlugin callback methods then delegate to MCPDock/MCPServer.
**Warning signs:** Compile error mentioning `get_instance_id` not found.

### Pitfall 2: Dock Control Cleanup in _exit_tree
**What goes wrong:** Memory leaks or crashes if dock control is not properly removed and freed.
**Why it happens:** `add_control_to_dock()` adds the control to the editor's scene tree. It must be removed before freeing.
**How to avoid:** In `_exit_tree()`: (1) `remove_control_from_docks(root)`, (2) `memdelete(root)` to free the VBoxContainer and all children, (3) `delete dock` to free the MCPDock wrapper. Order matters.
**Warning signs:** Crash on plugin disable, or "node still has parent" warnings.

### Pitfall 3: Dock Tab Title Comes from Node Name
**What goes wrong:** Dock tab shows "VBoxContainer" instead of a meaningful name.
**Why it happens:** `add_control_to_dock()` uses the root control's node name as the tab title.
**How to avoid:** Call `root->set_name("MCP Meow")` on the VBoxContainer before adding to dock.
**Warning signs:** Generic class name appearing as dock tab label.

### Pitfall 4: Thread Safety for Status Display
**What goes wrong:** Reading MCPServer state from the main thread while IO thread modifies it causes data races.
**Why it happens:** MCPServer has an IO thread that accepts/reads connections.
**How to avoid:** `is_running()` already uses `std::atomic<bool>`, which is thread-safe. For client connection state, add a similar `std::atomic<bool>` flag (e.g., `has_client_connected`). Do NOT access `client_peer` from the main thread.
**Warning signs:** Intermittent wrong status display, TSAN warnings.

### Pitfall 5: Tool Registry Refactor Breaking Existing Tests
**What goes wrong:** Moving tool schemas from `create_tools_list_response()` to a ToolDef registry changes the function signature, breaking 12+ existing unit tests in `test_protocol.cpp`.
**Why it happens:** Tests call `create_tools_list_response(id)` and expect hardcoded JSON.
**How to avoid:** Two approaches: (a) keep backward-compatible overload that calls the version-filtered variant with a permissive version like {99,99,99}, or (b) update all tests to pass a version parameter. Approach (a) is safer for incremental development.
**Warning signs:** Test compilation errors or test failures after refactor.

### Pitfall 6: Prompts Protocol Missing from Initialize Capabilities
**What goes wrong:** AI clients don't discover prompts because `prompts` capability isn't advertised.
**Why it happens:** Forgetting to add `{"prompts", {{"listChanged", false}}}` to the capabilities object in `create_initialize_response()`.
**How to avoid:** Add prompts capability alongside existing tools and resources capabilities. Add a unit test for it.
**Warning signs:** `prompts/list` calls fail with "method not found" on some clients.

## Code Examples

### Engine Version Detection (EDIT-03)

```cpp
// Source: godot-cpp gen/include/godot_cpp/classes/engine.hpp (verified)
#include <godot_cpp/classes/engine.hpp>

// Call from _enter_tree() or initialization (main thread only)
GodotVersion detect_godot_version() {
    GodotVersion ver = {4, 3, 0};  // fallback
    godot::Engine* engine = godot::Engine::get_singleton();
    if (engine) {
        godot::Dictionary info = engine->get_version_info();
        ver.major = static_cast<int>(info["major"]);
        ver.minor = static_cast<int>(info["minor"]);
        ver.patch = static_cast<int>(info["patch"]);
    }
    return ver;
}
// Returns Dictionary with keys:
// "major" (int), "minor" (int), "patch" (int),
// "status" (String, e.g. "stable"),
// "build" (String, e.g. "official"),
// "string" (String, e.g. "4.4.stable.official")
```

### Dock Registration (EDIT-01, EDIT-02)

```cpp
// Source: godot-cpp gen/include/godot_cpp/classes/editor_plugin.hpp (verified)
// add_control_to_dock signature:
//   void add_control_to_dock(EditorPlugin::DockSlot p_slot, Control *p_control,
//                            const Ref<Shortcut> &p_shortcut = nullptr);
// remove_control_from_docks signature:
//   void remove_control_from_docks(Control *p_control);

// DockSlot enum values (from godot-cpp header):
//   DOCK_SLOT_NONE     = -1
//   DOCK_SLOT_LEFT_UL  = 0
//   DOCK_SLOT_LEFT_BL  = 1
//   DOCK_SLOT_LEFT_UR  = 2
//   DOCK_SLOT_LEFT_BR  = 3
//   DOCK_SLOT_RIGHT_UL = 4
//   DOCK_SLOT_RIGHT_BL = 5   <-- Our target
//   DOCK_SLOT_RIGHT_UR = 6
//   DOCK_SLOT_RIGHT_BR = 7
//   DOCK_SLOT_BOTTOM   = 8
//   DOCK_SLOT_MAX      = 9
```

### MCP Prompts Protocol Responses (EDIT-04)

```cpp
// Source: MCP spec 2025-03-26 (https://modelcontextprotocol.io/docs/concepts/prompts)

// prompts/list response format:
nlohmann::json create_prompts_list_response(const nlohmann::json& id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"prompts", {
                {
                    {"name", "create_player_controller"},
                    {"description", "Create a player controller with movement, input handling, and collision"},
                    {"arguments", {
                        {
                            {"name", "movement_type"},
                            {"description", "Type of movement: 2d_platformer, 2d_top_down, 3d_first_person, 3d_third_person"},
                            {"required", true}
                        }
                    }}
                },
                {
                    {"name", "setup_scene_structure"},
                    {"description", "Set up a well-organized scene tree structure for a game"},
                    {"arguments", {
                        {
                            {"name", "game_type"},
                            {"description", "Type of game: platformer, rpg, puzzle, shooter"},
                            {"required", true}
                        }
                    }}
                },
                {
                    {"name", "debug_physics"},
                    {"description", "Debug physics issues by inspecting collision shapes, layers, and body properties"},
                    {"arguments", {
                        {
                            {"name", "node_path"},
                            {"description", "Path to the physics node to debug (optional, defaults to scene root)"},
                            {"required", false}
                        }
                    }}
                },
                {
                    {"name", "create_ui_interface"},
                    {"description", "Create a UI interface with common elements like health bars, menus, or HUD"},
                    {"arguments", {
                        {
                            {"name", "ui_type"},
                            {"description", "Type of UI: hud, main_menu, pause_menu, inventory"},
                            {"required", true}
                        }
                    }}
                }
            }}
        }}
    };
}

// prompts/get response format:
nlohmann::json create_prompt_get_response(const nlohmann::json& id,
    const std::string& description, const nlohmann::json& messages) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"description", description},
            {"messages", messages}
        }}
    };
}

// Example prompt message content:
nlohmann::json messages = {
    {
        {"role", "user"},
        {"content", {
            {"type", "text"},
            {"text", "Create a 2D platformer player controller in Godot..."}
        }}
    }
};
```

### Adding Client Connection State to MCPServer

```cpp
// MCPServer needs to expose client connection state for dock display.
// Add an atomic flag (thread-safe for reading from main thread):

// In mcp_server.h, add:
std::atomic<bool> client_connected{false};

// In mcp_server.cpp io_thread_func(), update when client connects/disconnects:
// After: client_peer = tcp_server->take_connection();
client_connected.store(true);
// After: client_peer.unref(); (in disconnect path)
client_connected.store(false);

// Public getter:
bool MCPServer::has_client() const {
    return client_connected.load();
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `add_control_to_dock()` | `add_dock()` with EditorDock | Godot 4.6 dev (late 2025) | We use the OLD approach since we target 4.3+. `add_control_to_dock()` is NOT deprecated in 4.3/4.4/4.5. |
| Hardcoded tool schemas | ToolDef registry (our refactor) | This phase | Enables version filtering, cleaner maintenance |
| No prompts capability | MCP prompts spec | MCP 2025-03-26 | New capability we add this phase |

**Deprecated/outdated:**
- `add_dock()` / `EditorDock`: Only available in Godot 4.6 development branch. NOT available in our godot-cpp bindings targeting 4.3+. However, our godot-cpp headers (which track a recent development branch) DO include both APIs. We must use `add_control_to_dock()` for backward compatibility with Godot 4.3.

**Note:** The project's godot-cpp submodule appears to be built against a recent API snapshot that includes both `add_control_to_dock()` and `add_dock()`. Both methods are available in the headers. We use `add_control_to_dock()` because it is the only method available across Godot 4.3-4.5.

## Open Questions

1. **MCPServer client connection state granularity**
   - What we know: `is_running()` uses `atomic<bool>` for the server. Client connection is tracked via `client_peer.is_valid()` but that is NOT atomic and is accessed from the IO thread.
   - What's unclear: Whether we need a three-state enum (disconnected/waiting/connected) or two booleans (server_running, client_connected).
   - Recommendation: Add `std::atomic<bool> client_connected` to MCPServer. Three display states derive from: !running = "Stopped", running && !client = "Waiting", running && client = "Connected". Simple and thread-safe.

2. **ToolDef registry location**
   - What we know: Currently tool schemas are inline in `mcp_protocol.cpp`. CONTEXT.md says "Claude's Discretion" on ToolDef data structure details.
   - What's unclear: Whether ToolDef registry should live in `mcp_protocol.cpp` (keeping all protocol logic together) or a new `mcp_tool_registry.h/cpp` (separation of concerns).
   - Recommendation: New `mcp_tool_registry.h/cpp` files. The ToolDef struct and tool definitions are data, not protocol logic. This also keeps `mcp_protocol.cpp` clean for testability. `mcp_protocol.cpp` calls into the registry to build responses.

3. **Button signal cleanup**
   - What we know: Signals connected via `callable_mp` are automatically cleaned up when the source node is freed.
   - What's unclear: Whether we need explicit `disconnect()` calls in `_exit_tree()` before `memdelete()`.
   - Recommendation: No explicit disconnect needed. When `memdelete(root)` frees the button nodes, any connected signals are cleaned up by Godot. The MCPPlugin (target) is also being destroyed in `_exit_tree()`.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd tests/build && ctest --output-on-failure` |
| Full suite command | `cd tests/build && cmake .. && cmake --build . && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| EDIT-01 | Dock shows connection status | manual-only | N/A (requires running Godot editor with plugin) | N/A |
| EDIT-02 | Start/stop/restart buttons work | manual-only | N/A (requires running Godot editor with plugin) | N/A |
| EDIT-03 | Version detection + tool filtering | unit | `cd tests/build && ctest -R test_tool_registry --output-on-failure` | -- Wave 0 |
| EDIT-03 | Tools list response filtered by version | unit | `cd tests/build && ctest -R test_protocol --output-on-failure` | -- Wave 0 (update existing) |
| EDIT-04 | Prompts capability in initialize response | unit | `cd tests/build && ctest -R test_protocol --output-on-failure` | -- Wave 0 (update existing) |
| EDIT-04 | prompts/list response format | unit | `cd tests/build && ctest -R test_protocol --output-on-failure` | -- Wave 0 |
| EDIT-04 | prompts/get response format | unit | `cd tests/build && ctest -R test_protocol --output-on-failure` | -- Wave 0 |
| EDIT-04 | prompts/get with missing prompt returns error | unit | `cd tests/build && ctest -R test_protocol --output-on-failure` | -- Wave 0 |
| ALL | End-to-end UAT in running Godot | manual + script | `python tests/uat_phase4.py` | -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && ctest --output-on-failure`
- **Per wave merge:** Full suite + manual Godot verification
- **Phase gate:** All unit tests green + manual dock verification in Godot editor

### Wave 0 Gaps
- [ ] `tests/test_tool_registry.cpp` -- unit tests for ToolDef, version comparison, filtered tool listing (covers EDIT-03)
- [ ] `tests/test_protocol.cpp` updates -- add tests for prompts capability, prompts/list, prompts/get responses (covers EDIT-04)
- [ ] `tests/CMakeLists.txt` update -- add `test_tool_registry` executable with `mcp_tool_registry.cpp` source
- [ ] `tests/uat_phase4.py` -- automated UAT script for prompts/list, prompts/get via TCP (covers EDIT-04 protocol)
- [ ] Manual test checklist for EDIT-01 and EDIT-02 (dock panel visibility and button behavior)

## Sources

### Primary (HIGH confidence)
- `godot-cpp/gen/include/godot_cpp/classes/editor_plugin.hpp` -- Verified `add_control_to_dock()` signature, `DockSlot` enum values, `remove_control_from_docks()` signature. Both old and new dock APIs present in headers.
- `godot-cpp/gen/include/godot_cpp/classes/engine.hpp` -- Verified `Engine::get_singleton()` and `get_version_info()` returning `Dictionary`.
- `godot-cpp/include/godot_cpp/variant/callable_method_pointer.hpp` -- Verified `callable_mp` macro, confirmed it requires `get_instance_id()` (Godot Object only).
- `godot-cpp/gen/include/godot_cpp/classes/v_box_container.hpp`, `label.hpp`, `button.hpp`, `h_box_container.hpp`, `h_separator.hpp` -- All UI classes confirmed available.
- [MCP Prompts Specification](https://modelcontextprotocol.io/docs/concepts/prompts) -- Full protocol for `prompts/list`, `prompts/get`, capability declaration, argument schemas.

### Secondary (MEDIUM confidence)
- [Godot EditorPlugin docs](https://docs.godotengine.org/en/stable/classes/class_editorplugin.html) -- Dock tab title comes from control's node name (`set_name()`).
- [Godot Engine.get_version_info() docs](https://docs.godotengine.org/en/3.0/classes/class_engine.html) -- Dictionary keys: major, minor, patch, status, build, string. Confirmed via multiple sources.
- [godot-cpp test/src/example.cpp](godot-cpp/test/src/example.cpp) -- `callable_mp` usage patterns in official godot-cpp examples.
- [Godot EditorPlugin.xml source](https://github.com/godotengine/godot/blob/master/doc/classes/EditorPlugin.xml) -- `add_control_to_dock` deprecated in 4.6 dev, NOT in 4.3/4.4/4.5.

### Tertiary (LOW confidence)
- Godot forum discussions on `callable_mp` with non-Object types -- Corroborates our finding but no official documentation explicitly states the requirement.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries already in project, no new dependencies
- Architecture: HIGH -- patterns verified against godot-cpp source headers and existing project code
- Dock API: HIGH -- verified exact function signatures in project's godot-cpp gen headers
- Signal connection: HIGH -- verified `callable_mp` template constraints in source code
- MCP Prompts protocol: HIGH -- spec directly fetched from official MCP documentation
- Version detection: HIGH -- `Engine::get_version_info()` confirmed in godot-cpp headers
- Pitfalls: HIGH -- derived from source code analysis, not speculation

**Research date:** 2026-03-17
**Valid until:** 2026-04-17 (stable APIs, low churn risk)
