# Phase 10: Running Game Bridge - Research

**Researched:** 2026-03-18
**Domain:** Godot EditorDebuggerPlugin IPC, game-side EngineDebugger, input injection, viewport capture
**Confidence:** HIGH

## Summary

Phase 10 implements bidirectional communication between the editor-side MCP server and a running game process using Godot's built-in debugger messaging system. The editor registers an `EditorDebuggerPlugin` subclass that routes MCP tool calls as debugger messages to the game. The game runs a companion GDScript autoload that registers message handlers via `EngineDebugger`, executes input injection (`Input.parse_input_event`) and viewport capture (`get_viewport().get_texture().get_image()`), and sends results back through the same channel.

The architecture uses three new components: (1) a C++ `EditorDebuggerPlugin` subclass registered with Godot's class system, (2) a GDScript companion autoload shipped in the addon directory, and (3) three new MCP tools (`inject_input`, `capture_game_viewport`, `get_game_bridge_status`) wired into the existing MCPServer dispatch. The debugger message protocol has an 8 MiB per-message limit, which is sufficient for PNG-encoded viewport screenshots (typical 1080p PNG is 1-3 MB).

**Primary recommendation:** Implement the EditorDebuggerPlugin as a C++ class (GDCLASS) registered alongside MCPPlugin, with the companion autoload as pure GDScript handling all game-side operations through EngineDebugger message capture.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- IPC via **EditorDebuggerPlugin** -- Godot's built-in debugging IPC, zero extra dependencies
- Data format: **Godot Variant arrays** via `send_message(name, data)` with Array
- Connection lifecycle: **Automatic** -- EditorDebuggerPlugin connects on game launch, disconnects on game close
- Editor-side: Register EditorDebuggerPlugin subclass in MCPPlugin
- Game-side: Companion autoload registers as debugger peer, receives and executes commands
- 3 MCP tools: **inject_input** (unified with type param), **capture_game_viewport**, **get_game_bridge_status**
- inject_input type="key": keycode string + pressed bool
- inject_input type="mouse": action ("move"/"click"/"scroll") + position + button
- inject_input type="action": action_name + pressed bool
- Companion autoload registered in **MCPPlugin::_enter_tree()** via ProjectSettings + add_autoload_singleton
- Companion is **GDScript** -- shipped in addons/meow_godot_mcp/companion/
- Plugin cleanup: **MCPPlugin::_exit_tree()** removes autoload registration
- New `game_bridge.h` / `game_bridge.cpp` for C++ EditorDebuggerPlugin + tool functions
- New companion GDScript: `addons/meow_godot_mcp/companion/meow_mcp_bridge.gd`

### Claude's Discretion
- EditorDebuggerPlugin subclass name and registration details
- Companion script exact message protocol (message names, data array structure)
- Keycode string parsing strategy (Godot Key enum mapping)
- Mouse coordinate handling (viewport-relative vs screen-relative)
- Game viewport capture implementation (screenshot request -> response via debugger messages)
- Error handling for disconnected game, unsupported input types
- Whether get_game_bridge_status returns additional session details

### Deferred Ideas (OUT OF SCOPE)
- Game state inspection (variable reading/writing)
- Real-time game event streaming
- Gamepad input injection
- Touch input injection
- Game audio capture
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BRDG-01 | Plugin auto-injects companion autoload, establishes editor-game communication | EditorDebuggerPlugin + EngineDebugger message protocol; add_autoload_singleton API |
| BRDG-02 | AI can inject keyboard key events (press/release) into running game | Input.parse_input_event() + InputEventKey; Key enum mapping from string |
| BRDG-03 | AI can inject mouse events (move, click, scroll) into running game | Input.parse_input_event() + InputEventMouseButton/MouseMotion; position via set_position |
| BRDG-04 | AI can inject Input Action events into running game | Input.parse_input_event() + InputEventAction; action_name via set_action |
| BRDG-05 | AI can capture running game viewport screenshot | get_viewport().get_texture().get_image() + save_png_to_buffer + base64 encode; return via debugger message |
</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ | GDExtension C++ bindings | Project foundation |
| nlohmann/json | 3.12.0 | JSON handling for MCP protocol | Existing dependency |

### New Godot APIs Used
| API Class | Purpose | Availability |
|-----------|---------|--------------|
| EditorDebuggerPlugin | Editor-side debugger IPC base class | Godot 4.0+ |
| EditorDebuggerSession | Per-game-instance session, send/receive messages | Godot 4.0+ |
| EngineDebugger | Game-side debugger singleton for message capture | Godot 4.0+ |
| Input | Input.parse_input_event() for injecting events | Godot 4.0+ |
| InputEventKey | Keyboard event with keycode + pressed | Godot 4.0+ |
| InputEventMouseButton | Mouse click/scroll with button + position | Godot 4.0+ |
| InputEventMouseMotion | Mouse movement with position + relative | Godot 4.0+ |
| InputEventAction | Named input action with pressed + strength | Godot 4.0+ |
| Marshalls | raw_to_base64() for PNG encoding | Godot 4.0+ |

### No New Dependencies
This phase requires zero new external libraries. All functionality comes from Godot's built-in APIs.

## Architecture Patterns

### Recommended Project Structure
```
src/
  game_bridge.h          # MeowDebuggerPlugin class + tool functions
  game_bridge.cpp        # EditorDebuggerPlugin impl + inject_input/capture/status
  mcp_plugin.h/cpp       # Modified: add_debugger_plugin in _enter_tree
  mcp_server.cpp         # Modified: dispatch 3 new tools
  mcp_tool_registry.cpp  # Modified: 3 new tool definitions
  register_types.cpp     # Modified: GDREGISTER_CLASS(MeowDebuggerPlugin)
project/addons/meow_godot_mcp/
  companion/
    meow_mcp_bridge.gd   # GDScript autoload for game side
```

### Pattern 1: EditorDebuggerPlugin Subclass (C++)
**What:** C++ class registered with Godot that receives debugger messages from the running game
**When to use:** Editor-side message routing

```cpp
// game_bridge.h
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>

class MeowDebuggerPlugin : public godot::EditorDebuggerPlugin {
    GDCLASS(MeowDebuggerPlugin, godot::EditorDebuggerPlugin);

public:
    // Virtual overrides for message routing
    void _setup_session(int32_t p_session_id) override;
    bool _has_capture(const godot::String &p_capture) const override;
    bool _capture(const godot::String &p_message, const godot::Array &p_data,
                  int32_t p_session_id) override;

    // Called by MCPServer tool handlers
    void send_to_game(const godot::String &message, const godot::Array &data);

    // Session state
    bool is_game_connected() const;
    int get_active_session_id() const;

protected:
    static void _bind_methods();

private:
    int active_session_id = -1;
    // Response data stored here for synchronous MCP tool calls
};
```

**Registration in register_types.cpp:**
```cpp
void initialize_mcp_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        GDREGISTER_CLASS(MCPPlugin);
        GDREGISTER_CLASS(MeowDebuggerPlugin);
    }
}
```

### Pattern 2: Message Protocol Convention
**What:** Prefix-based message routing between editor and game
**Convention:** All messages use prefix `meow_mcp`

**Editor -> Game messages:**
| Message | Data Array | Description |
|---------|------------|-------------|
| `meow_mcp:inject_key` | [keycode: String, pressed: bool] | Inject keyboard event |
| `meow_mcp:inject_mouse_click` | [x: float, y: float, button: String, pressed: bool] | Inject mouse click |
| `meow_mcp:inject_mouse_move` | [x: float, y: float] | Inject mouse motion |
| `meow_mcp:inject_mouse_scroll` | [x: float, y: float, direction: String] | Inject scroll |
| `meow_mcp:inject_action` | [action_name: String, pressed: bool] | Inject input action |
| `meow_mcp:capture_viewport` | [] | Request viewport screenshot |

**Game -> Editor messages:**
| Message | Data Array | Description |
|---------|------------|-------------|
| `meow_mcp:ready` | [] | Companion autoload initialized |
| `meow_mcp:input_result` | [success: bool, error: String] | Input injection result |
| `meow_mcp:viewport_data` | [base64_png: String, width: int, height: int] | Screenshot response |

### Pattern 3: Companion Autoload (GDScript)
**What:** Game-side script that handles messages from editor and executes them
**Key insight:** GDScript is the correct choice because the game process loads the addon (and its GDScript files) but does NOT have access to editor-only C++ classes.

```gdscript
# meow_mcp_bridge.gd
extends Node

func _ready():
    if not EngineDebugger.is_active():
        return  # Not running from editor, skip
    EngineDebugger.register_message_capture("meow_mcp", _on_message)
    EngineDebugger.send_message("meow_mcp:ready", [])

func _exit_tree():
    if EngineDebugger.is_active():
        EngineDebugger.unregister_message_capture("meow_mcp")

func _on_message(message: String, data: Array) -> bool:
    match message:
        "inject_key":
            return _handle_inject_key(data)
        "inject_mouse_click":
            return _handle_inject_mouse_click(data)
        # ... etc
    return false
```

### Pattern 4: Asynchronous Request-Response Over Debugger
**What:** MCP tool calls are synchronous but debugger messages are async. Need a bridge pattern.
**Key challenge:** MCPServer.handle_request runs on main thread and must return a JSON result. But the game is a separate process -- sending a message and waiting for response requires a polling/timeout pattern.

**Recommended approach:**
1. MCPServer calls game_bridge tool function
2. Tool function sends message via EditorDebuggerSession::send_message
3. Tool function enters a polling loop (checking for response in _capture callback)
4. _capture stores response data in shared state
5. Tool function returns when response arrives (or times out)

**Alternative (simpler, for input injection):** Input injection is fire-and-forget. The game processes the input immediately; no response needed. Return success immediately after sending.

**For viewport capture:** Must wait for response. Use a short timeout (e.g., 2-3 seconds).

```cpp
// Pseudocode for capture_game_viewport
nlohmann::json capture_game_viewport(MeowDebuggerPlugin* bridge) {
    if (!bridge->is_game_connected()) {
        return {{"error", "No game running or bridge not connected"}};
    }
    bridge->clear_viewport_response();
    bridge->send_to_game("meow_mcp:capture_viewport", Array());

    // Poll for response (called from main thread during _process)
    // The _capture callback will set the response data
    // Timeout after ~3 seconds
    auto start = std::chrono::steady_clock::now();
    while (!bridge->has_viewport_response()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(3)) {
            return {{"error", "Timeout waiting for game viewport capture"}};
        }
        // NOTE: cannot sleep here -- main thread must process
        // Need deferred/callback pattern instead
    }
    return bridge->get_viewport_response();
}
```

**CRITICAL PITFALL:** The MCPServer::poll() runs on the main thread during _process. The EditorDebuggerPlugin::_capture callback ALSO runs on the main thread. So when a tool function is waiting for a debugger response, _capture cannot fire. This means we CANNOT use a synchronous polling loop.

**Correct approach:** Process viewport capture as a two-phase operation:
1. Send capture request to game
2. Store a pending request marker
3. Return from poll() -- let the main loop continue
4. When _capture receives the viewport data, store it
5. On next MCPServer::poll(), check for pending viewport response and send MCP response

However, this conflicts with the current MCPServer architecture (IO thread waits for main thread response). The simplest solution: use `OS::get_singleton()->delay_msec()` or just accept that the response comes on a subsequent poll cycle. Actually, re-reading the MCPServer code: the IO thread BLOCKS until a response is queued. So we need the tool function to return immediately with the response. This means viewport capture must be handled differently.

**Practical solution for viewport capture:** The companion GDScript can capture the viewport immediately when requested and send the data back synchronously (in the same frame). But the _capture callback fires asynchronously from the tool handler perspective.

**Best solution:** Make game_bridge tool functions store the pending MCP request (id, etc.) and return a special "deferred" marker. MCPServer recognizes deferred responses and does NOT queue a response immediately. When the debugger _capture callback receives the viewport data, it constructs the MCP response and queues it directly. This is a more complex change to MCPServer but cleanly handles async.

**Simplest viable solution:** Have the MCPPlugin::_process poll game_bridge for pending responses. When MCPServer::poll() processes a capture_game_viewport request, it stores the request info and returns. A separate mechanism in _process checks for completed viewport responses and delivers them to MCPServer's response queue.

### Pattern 5: Autoload Registration
**What:** Register the companion GDScript as a project autoload so it loads when the game starts
**Source:** EditorPlugin::add_autoload_singleton / remove_autoload_singleton

```cpp
// In MCPPlugin::_enter_tree()
add_autoload_singleton("MeowMCPBridge",
    "res://addons/meow_godot_mcp/companion/meow_mcp_bridge.gd");

// In MCPPlugin::_exit_tree()
remove_autoload_singleton("MeowMCPBridge");
```

**Note on _enable_plugin vs _enter_tree:** Best practice recommends `_enable_plugin`/`_disable_plugin` for persistent autoloads. However, `_enter_tree`/`_exit_tree` is acceptable here because the companion is only useful while the plugin is active in the current session, and it's a locked decision in CONTEXT.md.

### Anti-Patterns to Avoid
- **Synchronous wait for debugger response in tool handler:** Will deadlock the main thread since _capture callbacks also run on the main thread.
- **Sending raw PNG bytes in debugger message without base64:** Variant serialization supports PackedByteArray, but base64 String is safer for cross-process data and reuses the ImageContent pattern from Phase 9.
- **Registering EngineDebugger capture in C++ (editor side):** EngineDebugger is game-side only. The editor uses EditorDebuggerPlugin. Don't confuse the two APIs.
- **Using Input.action_press/action_release instead of InputEventAction:** Input.parse_input_event with InputEventAction is more complete (propagates through entire input tree).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Editor-game IPC | Custom TCP/file-based messaging | EditorDebuggerPlugin + EngineDebugger | Built-in, handles connection lifecycle, no extra ports |
| Input simulation | Direct OS-level key injection | Input.parse_input_event() | Works within Godot's input tree, cross-platform |
| Image encoding | Custom PNG encoder | Image.save_png_to_buffer() | Godot built-in, handles all formats |
| Base64 encoding | Custom base64 | Marshalls.raw_to_base64() | Godot built-in singleton |
| Key string to enum | Manual ASCII mapping | Static lookup table (C++ side) or OS.find_keycode_from_string (GDScript) | Comprehensive, handles special keys |

## Common Pitfalls

### Pitfall 1: Main Thread Deadlock on Async Response
**What goes wrong:** Tool handler sends message to game, then loops waiting for response. Response never arrives because _capture callback is also on main thread.
**Why it happens:** Both MCPServer::poll() and EditorDebuggerPlugin::_capture run on Godot's main thread. Blocking one blocks the other.
**How to avoid:** Use deferred response pattern. Tool handler initiates the request and returns. Response is delivered asynchronously when _capture fires on a subsequent frame.
**Warning signs:** Game appears to freeze, MCP client times out, no response from capture_game_viewport.

### Pitfall 2: Companion Script Not Found at Runtime
**What goes wrong:** Game starts but companion autoload fails to load. EngineDebugger messages go unhandled.
**Why it happens:** The companion .gd file path is wrong, or the file isn't shipped with the addon.
**How to avoid:** Use absolute `res://` path. Verify the file exists in `project/addons/meow_godot_mcp/companion/`. Add the file to version control.
**Warning signs:** Godot console error "Cannot load autoload script", bridge status shows disconnected even when game is running.

### Pitfall 3: EditorDebuggerPlugin Not Receiving Messages
**What goes wrong:** `_has_capture` returns true but `_capture` never called.
**Why it happens:** Message prefix mismatch. Game sends "meow_mcp:action" but `_has_capture` checks for different string. Also: the message param to `_capture` includes the full "prefix:action" string, not just "action".
**How to avoid:** Ensure exact prefix match. In `_has_capture`, return `p_capture == "meow_mcp"`. In `_capture`, the message string will be "meow_mcp:action" -- parse after the colon.
**Warning signs:** No errors, but messages silently dropped.

### Pitfall 4: 8 MiB Message Size Limit
**What goes wrong:** Large viewport screenshots fail silently or crash.
**Why it happens:** Godot's debugger protocol has an 8 MiB max message size (hardcoded in remote_debugger_peer.cpp).
**How to avoid:** Base64-encode PNG (not raw pixels). A 1920x1080 PNG is typically 1-3 MB after compression. Add optional width/height resize. Log warning if encoded size > 6 MB.
**Warning signs:** Screenshot returns empty data, game disconnects after capture attempt.

### Pitfall 5: EngineDebugger Not Active in Non-Debug Runs
**What goes wrong:** Companion autoload starts but EngineDebugger.is_active() returns false.
**Why it happens:** Game was launched outside of editor (standalone export or via command line without --debug flag).
**How to avoid:** Always check `EngineDebugger.is_active()` before registering captures. Silently skip bridge initialization.
**Warning signs:** No "meow_mcp:ready" message from game side.

### Pitfall 6: GDCLASS Registration Required for Virtual Dispatch
**What goes wrong:** `_has_capture` and `_capture` overrides are never called despite being implemented.
**Why it happens:** EditorDebuggerPlugin subclass not registered with GDREGISTER_CLASS in register_types.cpp.
**How to avoid:** Add `GDREGISTER_CLASS(MeowDebuggerPlugin)` at MODULE_INITIALIZATION_LEVEL_EDITOR. Include `_bind_methods` even if empty.
**Warning signs:** Plugin registers successfully but debugger messages from game are never received.

### Pitfall 7: Autoload Order and Race Conditions
**What goes wrong:** Companion sends `meow_mcp:ready` before editor debugger plugin is set up to receive.
**Why it happens:** Autoload _ready() fires immediately on game start, but EditorDebuggerPlugin session may not be active yet.
**How to avoid:** The `_setup_session` callback fires when the game connects. `started` signal on EditorDebuggerSession confirms active connection. The game companion should send `ready` in _ready(), and the editor can safely receive it because session setup happens before game autoloads run.
**Warning signs:** Missing `ready` message in edge cases, inconsistent connection state.

### Pitfall 8: parse_input_event Timing with is_action_just_pressed
**What goes wrong:** Injected InputEventAction doesn't trigger `is_action_just_pressed()`.
**Why it happens:** Godot's input system expects press and release on different frames. Calling parse_input_event for both press and release in the same frame may not work correctly.
**How to avoid:** For input actions, only send press OR release per call (matching the MCP tool's `pressed` param). Let the AI control timing explicitly.
**Warning signs:** Game responds to `is_action_pressed()` but not `is_action_just_pressed()`.

## Code Examples

### Example 1: EditorDebuggerPlugin Virtual Methods (C++)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/editor_debugger_plugin.hpp
// and Godot docs: EditorDebuggerPlugin class reference

bool MeowDebuggerPlugin::_has_capture(const String &p_capture) const {
    return p_capture == "meow_mcp";
}

bool MeowDebuggerPlugin::_capture(const String &p_message,
                                    const Array &p_data,
                                    int32_t p_session_id) {
    // p_message is full string like "meow_mcp:ready"
    // Strip prefix to get action
    String action = p_message.substr(p_message.find(":") + 1);

    if (action == "ready") {
        active_session_id = p_session_id;
        UtilityFunctions::print("MCP Meow: Game bridge connected (session ",
                                 p_session_id, ")");
        return true;
    }
    if (action == "viewport_data") {
        // p_data[0] = base64 PNG string
        // p_data[1] = width (int)
        // p_data[2] = height (int)
        _store_viewport_response(p_data);
        return true;
    }
    if (action == "input_result") {
        // p_data[0] = success (bool)
        // p_data[1] = error message (string, empty on success)
        return true;
    }
    return false;
}

void MeowDebuggerPlugin::_setup_session(int32_t p_session_id) {
    Ref<EditorDebuggerSession> session = get_session(p_session_id);
    if (session.is_valid()) {
        // Connect to session signals for lifecycle tracking
        session->connect("started", callable_mp(this, &MeowDebuggerPlugin::_on_session_started).bind(p_session_id));
        session->connect("stopped", callable_mp(this, &MeowDebuggerPlugin::_on_session_stopped).bind(p_session_id));
    }
}
```

### Example 2: Companion Autoload Message Handling (GDScript)
```gdscript
# meow_mcp_bridge.gd - runs in the GAME process
extends Node

func _ready():
    if not EngineDebugger.is_active():
        queue_free()  # Not running from editor
        return
    EngineDebugger.register_message_capture("meow_mcp", _on_message)
    EngineDebugger.send_message("meow_mcp:ready", [])

func _exit_tree():
    if EngineDebugger.is_active():
        EngineDebugger.unregister_message_capture("meow_mcp")

func _on_message(message: String, data: Array) -> bool:
    # message has prefix stripped: "inject_key", "capture_viewport", etc.
    match message:
        "inject_key":
            return _inject_key(data)
        "inject_mouse_click":
            return _inject_mouse_click(data)
        "inject_mouse_move":
            return _inject_mouse_move(data)
        "inject_mouse_scroll":
            return _inject_mouse_scroll(data)
        "inject_action":
            return _inject_action(data)
        "capture_viewport":
            return _capture_viewport()
    return false
```

### Example 3: Input Injection (GDScript game-side)
```gdscript
func _inject_key(data: Array) -> bool:
    var keycode_str: String = data[0]  # e.g., "W", "space", "escape"
    var pressed: bool = data[1]

    var event = InputEventKey.new()
    event.keycode = OS.find_keycode_from_string(keycode_str)
    event.pressed = pressed
    event.physical_keycode = event.keycode
    Input.parse_input_event(event)
    return true

func _inject_mouse_click(data: Array) -> bool:
    var x: float = data[0]
    var y: float = data[1]
    var button_str: String = data[2]  # "left", "right", "middle"
    var pressed: bool = data[3]

    var button_index = {
        "left": MOUSE_BUTTON_LEFT,
        "right": MOUSE_BUTTON_RIGHT,
        "middle": MOUSE_BUTTON_MIDDLE
    }.get(button_str, MOUSE_BUTTON_LEFT)

    var event = InputEventMouseButton.new()
    event.position = Vector2(x, y)
    event.global_position = Vector2(x, y)
    event.button_index = button_index
    event.pressed = pressed
    Input.parse_input_event(event)
    return true

func _inject_action(data: Array) -> bool:
    var action_name: String = data[0]
    var pressed: bool = data[1]

    var event = InputEventAction.new()
    event.action = action_name
    event.pressed = pressed
    event.strength = 1.0 if pressed else 0.0
    Input.parse_input_event(event)
    return true
```

### Example 4: Viewport Capture (GDScript game-side)
```gdscript
func _capture_viewport() -> bool:
    var image = get_viewport().get_texture().get_image()
    if image == null or image.is_empty():
        EngineDebugger.send_message("meow_mcp:viewport_data",
            ["", 0, 0])  # Empty = error
        return true

    var png_data = image.save_png_to_buffer()
    var base64_str = Marshalls.raw_to_base64(png_data)
    EngineDebugger.send_message("meow_mcp:viewport_data",
        [base64_str, image.get_width(), image.get_height()])
    return true
```

### Example 5: MCPPlugin Integration
```cpp
// In MCPPlugin header - add member
Ref<MeowDebuggerPlugin> debugger_plugin;

// In MCPPlugin::_enter_tree()
debugger_plugin.instantiate();
add_debugger_plugin(debugger_plugin);
add_autoload_singleton("MeowMCPBridge",
    "res://addons/meow_godot_mcp/companion/meow_mcp_bridge.gd");

// In MCPPlugin::_exit_tree()
remove_autoload_singleton("MeowMCPBridge");
if (debugger_plugin.is_valid()) {
    remove_debugger_plugin(debugger_plugin);
    debugger_plugin.unref();
}

// Pass debugger_plugin pointer to MCPServer for tool dispatch
server->set_game_bridge(debugger_plugin.ptr());
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Custom TCP between editor and game | EditorDebuggerPlugin IPC | Godot 4.0 | No extra ports, automatic lifecycle |
| File-based polling for screenshots | Debugger message with Variant arrays | Godot 4.0 | Real-time, no disk I/O |
| OS-level input injection | Input.parse_input_event() | Godot 3.0+ | Cross-platform, within Godot input tree |

## Open Questions

1. **Async viewport capture response delivery**
   - What we know: Main thread handles both MCPServer::poll() and EditorDebuggerPlugin::_capture. Cannot block tool handler waiting for game response.
   - What's unclear: Best mechanism to deliver deferred MCP responses through MCPServer's existing IO thread architecture.
   - Recommendation: Extend MCPServer with a deferred response mechanism. When handle_request returns a special "pending" marker, MCPServer does not immediately queue a response. Instead, a callback (set on the game_bridge) queues the response when the game-side data arrives. The IO thread is notified via the existing response_cv. This requires storing the pending request's `id` to construct the response later.

2. **Key string parsing strategy**
   - What we know: GDScript has `OS.find_keycode_from_string()` which maps "W" -> KEY_W, "space" -> KEY_SPACE, etc. C++ side has the Key enum.
   - What's unclear: Whether OS.find_keycode_from_string handles all the key names AI clients would use (e.g., "ctrl+a", "shift+F5").
   - Recommendation: Let the companion GDScript handle key parsing via OS.find_keycode_from_string. For modifier keys (ctrl, shift, alt), parse them separately and set modifiers on the InputEventKey. The C++ tool handler just passes the string through to the game side.

3. **Mouse coordinate space**
   - What we know: InputEventMouse has set_position (viewport-local) and set_global_position (screen).
   - What's unclear: Whether AI clients will provide viewport-relative or window-relative coordinates.
   - Recommendation: Use viewport-relative coordinates. Set both position and global_position to the same value (viewport coords). Document this in the tool schema.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Python UAT (same as phases 5-9) + GoogleTest |
| Config file | tests/CMakeLists.txt (GoogleTest), uat scripts (Python) |
| Quick run command | `python tests/uat_phase10.py` |
| Full suite command | `cd tests/build && ctest && cd ../.. && python tests/uat_phase10.py` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BRDG-01 | Bridge auto-connects on game launch | UAT | `python tests/uat_phase10.py` (test 1-3) | No - Wave 0 |
| BRDG-02 | Key injection (press/release) | UAT | `python tests/uat_phase10.py` (test 4-5) | No - Wave 0 |
| BRDG-03 | Mouse injection (move/click/scroll) | UAT | `python tests/uat_phase10.py` (test 6-8) | No - Wave 0 |
| BRDG-04 | Action injection (press/release) | UAT | `python tests/uat_phase10.py` (test 9-10) | No - Wave 0 |
| BRDG-05 | Game viewport capture as ImageContent | UAT | `python tests/uat_phase10.py` (test 11-13) | No - Wave 0 |
| N/A | Tool registry has 38 tools | Unit | `cd tests/build && ctest -R test_tool_registry` | Exists (update needed) |

### Sampling Rate
- **Per task commit:** `cd tests/build && ctest -R test_tool_registry`
- **Per wave merge:** Full unit test suite + UAT
- **Phase gate:** Full suite green before verify

### Wave 0 Gaps
- [ ] `tests/uat_phase10.py` -- covers BRDG-01..05
- [ ] Update `tests/test_tool_registry.cpp` -- verify 38 tools (35 + 3 new)
- [ ] `project/addons/meow_godot_mcp/companion/meow_mcp_bridge.gd` -- companion script

### UAT Testing Strategy for Game Bridge
**Challenge:** UAT requires a RUNNING GAME to test bridge communication. The test script must:
1. Connect to MCP server (as usual)
2. Use `run_game` tool to launch the game
3. Wait for bridge connection (poll `get_game_bridge_status`)
4. Execute input injection and viewport capture tests
5. Use `stop_game` to clean up

This is more complex than previous UAT phases because it requires game lifecycle management within the test.

## Sources

### Primary (HIGH confidence)
- godot-cpp gen headers: `editor_debugger_plugin.hpp`, `editor_debugger_session.hpp`, `engine_debugger.hpp`, `input_event_key.hpp`, `input_event_mouse_button.hpp`, `input_event_mouse_motion.hpp`, `input_event_action.hpp`, `input.hpp`, `editor_plugin.hpp`, `global_constants.hpp` -- actual API signatures from the project's godot-cpp v10 build
- Godot source `remote_debugger_peer.cpp` -- 8 MiB message size limit, Variant serialization

### Secondary (MEDIUM confidence)
- [EditorDebuggerPlugin official docs](https://docs.godotengine.org/en/stable/classes/class_editordebuggerplugin.html) -- virtual method contracts, message prefix convention
- [EngineDebugger official docs](https://docs.godotengine.org/en/stable/classes/class_enginedebugger.html) -- register_message_capture, send_message
- [EditorDebuggerSession official docs](https://docs.godotengine.org/en/stable/classes/class_editordebuggersession.html) -- started/stopped signals, send_message, is_active
- [EditorPlugin.xml from Godot source](https://raw.githubusercontent.com/godotengine/godot/master/doc/classes/EditorPlugin.xml) -- add_debugger_plugin, add_autoload_singleton signatures
- [Godot docs issue #9571](https://github.com/godotengine/godot-docs/issues/9571) -- autoload best practices (_enable_plugin vs _enter_tree)
- [Godot proposals discussion #10994](https://github.com/godotengine/godot-proposals/discussions/10994) -- editor-game communication via debugger messages

### Tertiary (LOW confidence)
- [Godot forum: EditorDebuggerPlugin understanding](https://godotforums.org/d/37525-understanding-editordebuggerplugin-and-editordebuggersession) -- community examples
- [Godot issue #60416](https://github.com/godotengine/godot/issues/60416) -- parse_input_event timing with is_action_just_pressed

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All APIs verified from actual godot-cpp headers in the project
- Architecture: HIGH - EditorDebuggerPlugin pattern well-documented, message protocol clear from official docs and source
- Pitfalls: HIGH - Threading/deadlock issues identified from analyzing MCPServer code; message size limit verified from Godot source
- Async response pattern: MEDIUM - The deferred response mechanism needs design during planning; concept is sound but implementation details TBD

**Research date:** 2026-03-18
**Valid until:** 2026-04-18 (stable -- Godot debugger APIs unchanged since 4.0)
