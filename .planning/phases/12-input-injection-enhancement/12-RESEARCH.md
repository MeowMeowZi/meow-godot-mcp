# Phase 12: Input Injection Enhancement - Research

**Researched:** 2026-03-20
**Domain:** Godot input injection, runtime node querying via EditorDebuggerPlugin
**Confidence:** HIGH

## Summary

Phase 12 enhances the existing input injection system (Phase 10) with three capabilities: (1) automatic press+release cycling for click actions, (2) a `click_node` tool that clicks UI nodes by scene tree path, and (3) a `get_node_rect` tool that returns a node's screen coordinates and size. All three features build on the existing EditorDebuggerPlugin message channel between the editor (C++ GDExtension) and the running game (GDScript companion autoload).

The implementation is straightforward because the architecture is already proven. The existing `inject_input` tool sends fire-and-forget messages via `send_to_game()`, and the existing viewport capture uses a deferred response pattern (pending state + callback). The new `click_node` and `get_node_rect` tools need the deferred pattern since they require the game process to resolve node paths, compute coordinates, and return results. The 50ms delay for click cycling is best implemented in GDScript using `await get_tree().create_timer(0.05).timeout` since it naturally integrates with the Godot main loop.

**Primary recommendation:** Implement the 50ms click delay on the GDScript (game) side using `await create_timer()`. Use the deferred response pattern (same as viewport capture) for `click_node` and `get_node_rect`. Generalize the pending state tracking from single-purpose (viewport only) to multi-purpose (viewport, click_node, get_node_rect).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- mouse_action=click automatically sends press+release (no opt-in required)
- 50ms delay between press and release events
- Backward compatible: explicit pressed=true/false preserves single-fire behavior
- click_node is an independent MCP tool with only node_path parameter
- Node paths use scene tree relative paths (e.g., "BackpackUI/BtnSearch")
- click_node uses async deferred mode: game resolves path, gets coordinates, injects click, returns result
- get_node_rect returns viewport coordinates (compatible with inject_input position)
- get_node_rect returns position + size + global_position (full Rect2 + global pos)
- Node not found / not visible / not Control returns error with reason
- click_node returns actual clicked coordinates (for debugging/verification)

### Claude's Discretion
- 50ms delay implementation method (C++ timer vs game-side delay)
- click_node deferred response: reuse viewport capture mechanism or new independent mechanism
- Unit test coverage scope

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INPT-01 | click action auto-includes press+release complete cycle, single call completes click | Backward-compatible detection via `args.contains("pressed")` check in C++; 50ms delay via GDScript `await create_timer(0.05).timeout` on game side |
| INPT-02 | New `click_node` tool, click running game UI node by node path | Deferred response pattern (generalized from viewport capture); GDScript resolves path via `get_tree().current_scene.get_node()`, uses `Control.get_global_rect()` center for click coordinates |
| INPT-03 | New `get_node_rect` tool, get running node screen coordinates and size | Deferred response pattern; GDScript uses `Control.get_global_rect()` for Rect2, `Control.get_global_position()` for global position; validates `is_visible_in_tree()` and `is Control` |
</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ (4.3) | GDExtension C++ bindings | Project foundation |
| nlohmann/json | 3.12.0 | JSON handling in C++ | Already used throughout |
| GDScript | Godot 4.3 | Game-side companion script | Already used for meow_mcp_bridge.gd |

### Godot APIs Used
| API | Where | Purpose |
|-----|-------|---------|
| `Control.get_global_rect()` | meow_mcp_bridge.gd | Returns `Rect2` with position+size in viewport coordinates |
| `Control.get_global_position()` | meow_mcp_bridge.gd | Returns `Vector2` global position |
| `CanvasItem.is_visible_in_tree()` | meow_mcp_bridge.gd | Check if node and all ancestors are visible |
| `SceneTree.current_scene` | meow_mcp_bridge.gd | Get root of the running scene for path resolution |
| `Node.get_node()` | meow_mcp_bridge.gd | Resolve relative node path from current_scene |
| `await get_tree().create_timer(0.05).timeout` | meow_mcp_bridge.gd | 50ms delay between press and release |
| `Input.parse_input_event()` | meow_mcp_bridge.gd | Inject mouse press/release events (already used) |

**No new dependencies required. All implementation uses existing libraries and Godot APIs.**

## Architecture Patterns

### Modified Files
```
src/
  game_bridge.h          # Add click_node_tool(), get_node_rect_tool() declarations; generalize pending state
  game_bridge.cpp        # Add new tool methods; modify inject_input_tool() for auto-click; handle new message types
  mcp_tool_registry.cpp  # Register click_node and get_node_rect tool definitions
  mcp_server.cpp         # Add dispatch for click_node and get_node_rect

project/addons/meow_godot_mcp/companion/
  meow_mcp_bridge.gd     # Add message handlers; modify _inject_mouse_click for auto-cycle; add node lookup functions
```

### Pattern 1: Backward-Compatible Click Auto-Cycling (INPT-01)

**What:** When `mouse_action=click` and `pressed` is NOT explicitly provided in args, automatically send press then release with 50ms delay.
**When to use:** Modifying existing inject_input behavior.

**C++ side (game_bridge.cpp) changes:**
```cpp
// In inject_input_tool(), mouse_action == "click" branch:
bool explicit_pressed = args.contains("pressed") && args["pressed"].is_boolean();

if (!explicit_pressed) {
    // Auto-cycle mode: tell game to do press+delay+release
    Array data;
    data.push_back(x);
    data.push_back(y);
    data.push_back(String(button.c_str()));
    data.push_back(true);  // auto_cycle flag
    send_to_game("meow_mcp:inject_mouse_click", data);

    return {{"success", true}, {"type", "mouse"}, {"mouse_action", "click"},
            {"position", {{"x", x}, {"y", y}}}, {"button", button}, {"auto_cycle", true}};
} else {
    // Existing single-fire behavior
    // ... (unchanged)
}
```

**GDScript side (meow_mcp_bridge.gd) changes:**
```gdscript
func _inject_mouse_click(data: Array) -> bool:
    var x: float = data[0]
    var y: float = data[1]
    var button_str: String = data[2]
    var auto_cycle: bool = data[3] if data.size() > 3 else false

    var button_map = { "left": MOUSE_BUTTON_LEFT, "right": MOUSE_BUTTON_RIGHT, "middle": MOUSE_BUTTON_MIDDLE }
    var btn = button_map.get(button_str, MOUSE_BUTTON_LEFT)

    if auto_cycle:
        # Press
        var press_event = InputEventMouseButton.new()
        press_event.position = Vector2(x, y)
        press_event.global_position = Vector2(x, y)
        press_event.button_index = btn
        press_event.pressed = true
        Input.parse_input_event(press_event)

        # 50ms delay
        await get_tree().create_timer(0.05).timeout

        # Release
        var release_event = InputEventMouseButton.new()
        release_event.position = Vector2(x, y)
        release_event.global_position = Vector2(x, y)
        release_event.button_index = btn
        release_event.pressed = false
        Input.parse_input_event(release_event)

        EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
    else:
        # Original single-fire behavior (data[3] is pressed bool)
        var pressed: bool = data[3]
        var event = InputEventMouseButton.new()
        event.position = Vector2(x, y)
        event.global_position = Vector2(x, y)
        event.button_index = btn
        event.pressed = pressed
        Input.parse_input_event(event)
        EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
    return true
```

### Pattern 2: Generalized Deferred Response (INPT-02, INPT-03)

**What:** Extend the existing single-purpose deferred response (viewport capture) to support multiple concurrent deferred operations.
**When to use:** click_node and get_node_rect both need round-trip communication with the game process.

**Current state:** game_bridge.h has single `pending_capture_id`, `has_pending_capture` fields.

**Recommended approach:** Use a map of pending request type to pending state, OR use a simple enum/string tag on the pending state. Since these operations don't overlap (MCP processes one request at a time on the IO thread wait pattern), a tagged single-slot approach is simplest:

```cpp
// game_bridge.h
enum class PendingType { NONE, VIEWPORT_CAPTURE, CLICK_NODE, GET_NODE_RECT };

PendingType pending_type = PendingType::NONE;
nlohmann::json pending_id;     // MCP request id
int pending_width = 0;         // For viewport capture
int pending_height = 0;        // For viewport capture
```

**Why single-slot works:** The MCPServer processes requests sequentially on the IO thread (it waits for response_cv before processing the next message). So there can only be one pending deferred request at a time.

### Pattern 3: Node Path Resolution in Game Process (INPT-02, INPT-03)

**What:** Resolve scene-tree-relative node paths in the running game and return results via debugger messages.
**When to use:** click_node and get_node_rect need to find nodes in the running game's scene tree.

```gdscript
func _resolve_node(node_path: String) -> Node:
    var scene_root = get_tree().current_scene
    if scene_root == null:
        return null
    if node_path.is_empty():
        return scene_root
    return scene_root.get_node_or_null(node_path)
```

**Path format:** Relative to `current_scene` root (e.g., "BackpackUI/BtnSearch"). This matches what `get_scene_tree` tool returns to AI, ensuring consistency.

### Pattern 4: click_node Deferred Flow

**Editor (C++) side:**
1. `click_node_tool(args)` validates node_path, checks game connected
2. Sets `pending_type = PendingType::CLICK_NODE`, stores `pending_id`
3. Sends `"meow_mcp:click_node"` message with node_path
4. Returns `{{"__deferred", true}}`

**Game (GDScript) side:**
1. Receives `click_node` message with node_path
2. Resolves node via `_resolve_node(node_path)`
3. Validates: exists? is Control? is_visible_in_tree()?
4. Gets center of `get_global_rect()` as click position
5. Injects press event at center position
6. `await get_tree().create_timer(0.05).timeout`
7. Injects release event at same position
8. Sends `"meow_mcp:click_node_result"` with success/coordinates/error

**Editor (C++) side receives:**
1. `_capture()` handles `"click_node_result"` action
2. Builds JSON response with clicked coordinates
3. Calls `deferred_callback(response)` which queues response for IO thread

### Pattern 5: get_node_rect Deferred Flow

Same as click_node but simpler (no input injection):

**Game (GDScript) side:**
1. Resolve node, validate Control + visible
2. Get `get_global_rect()` -> position.x, position.y, size.x, size.y
3. Get `get_global_position()` -> global_position.x, global_position.y
4. Send `"meow_mcp:node_rect_result"` with all values

**Response JSON format:**
```json
{
    "node_path": "BackpackUI/BtnSearch",
    "position": {"x": 100, "y": 50},
    "size": {"width": 200, "height": 40},
    "global_position": {"x": 100, "y": 50},
    "center": {"x": 200, "y": 70},
    "visible": true
}
```

### Anti-Patterns to Avoid
- **Blocking the C++ main thread:** Never use `sleep` or busy-wait in C++ for the 50ms delay. The delay MUST be on the GDScript side using `await create_timer()`.
- **Hardcoded node paths with /root/:** Node paths should be relative to `current_scene`, NOT absolute paths starting with `/root/`. The AI works with relative paths from `get_scene_tree`.
- **Multiple pending states:** Do NOT create separate pending fields for each deferred type. The single-slot tagged approach is sufficient because MCP requests are serial.
- **Returning success before game confirms:** click_node must use deferred response to return the actual click result, not fire-and-forget. The AI needs to know if the click succeeded.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Delay between press/release | C++ sleep or timer thread | GDScript `await create_timer(0.05).timeout` | Integrates with Godot main loop, non-blocking |
| Node path resolution | Custom path parser | `Node.get_node_or_null(path)` on `current_scene` | Handles all Godot path syntax correctly |
| Rect computation | Manual position+size math | `Control.get_global_rect()` | Accounts for anchors, offsets, parent transforms |
| Visibility check | Manual parent traversal | `CanvasItem.is_visible_in_tree()` | Checks entire ancestor chain automatically |

**Key insight:** The GDScript side should do ALL node resolution and coordinate computation. The C++ side only handles message passing and MCP protocol framing.

## Common Pitfalls

### Pitfall 1: Auto-Cycle Breaking Backward Compatibility
**What goes wrong:** Existing AI workflows that pass `pressed: true` for mouse click suddenly get double-fire behavior.
**Why it happens:** Not checking whether `pressed` was explicitly provided.
**How to avoid:** Use `args.contains("pressed")` in C++ to distinguish explicit from implicit. Only auto-cycle when `pressed` is NOT explicitly set.
**Warning signs:** UAT tests for existing inject_input with `pressed=true/false` start failing.

### Pitfall 2: GDScript await in Message Handler Return
**What goes wrong:** `_inject_mouse_click` with `await` changes its return behavior -- it becomes a coroutine, and the `return true` happens after the await, not immediately.
**Why it happens:** GDScript functions with `await` become coroutines.
**How to avoid:** The `_on_message` dispatch still returns `true` for the message match. The key is that `_inject_mouse_click` is called (coroutine starts), and the return value from the coroutine is not critical for the message capture system -- EngineDebugger just needs to know the message was recognized.
**Warning signs:** Messages not being recognized as captured. Test by verifying the `_on_message` return path still works correctly.

### Pitfall 3: Node Not in Tree During Resolution
**What goes wrong:** `get_node_or_null()` returns null for nodes that exist but are not yet in the tree.
**Why it happens:** Deferred node additions, scenes being loaded.
**How to avoid:** Always null-check the result and return a clear "not found" error.
**Warning signs:** Intermittent null results for nodes that should exist.

### Pitfall 4: get_global_rect() Returns Zero Rect
**What goes wrong:** `get_global_rect()` returns `Rect2(0, 0, 0, 0)` if called too early in _ready or before layout pass.
**Why it happens:** Control layout is computed during the frame's layout pass, not immediately.
**How to avoid:** Since these tools are called during gameplay (not _ready), this should not be an issue. But if it is, use `await get_tree().process_frame` before querying.
**Warning signs:** Zero-size rects for nodes that are visually present.

### Pitfall 5: Non-Control Nodes with click_node
**What goes wrong:** AI tries to click a Node2D or Sprite2D, which don't have `get_global_rect()`.
**Why it happens:** Only Control nodes have rect-based layout.
**How to avoid:** Check `node is Control` before calling `get_global_rect()`. Return clear error: "Node is not a Control node (type: Sprite2D)".
**Warning signs:** Method not found errors in game output.

### Pitfall 6: Deferred Response Leaked on Game Disconnect
**What goes wrong:** Game stops while a click_node or get_node_rect request is pending, leaving the MCP client hanging.
**Why it happens:** `_on_session_stopped` only handles viewport capture pending state.
**How to avoid:** Generalize `_on_session_stopped` to clean up ANY pending deferred request, not just viewport capture.
**Warning signs:** MCP client timeout waiting for response after game crash/stop.

## Code Examples

### Tool Registration (mcp_tool_registry.cpp)

```cpp
// click_node tool definition
{
    "click_node",
    "Click a UI Control node in the running game by its scene tree path. "
    "Resolves the node, computes its center position, and injects a complete "
    "press+release mouse click. Returns the actual clicked coordinates. "
    "Requires a game to be running with the MCP bridge connected.",
    {
        {"type", "object"},
        {"properties", {
            {"node_path", {
                {"type", "string"},
                {"description", "Path to the Control node relative to the scene root "
                               "(e.g., 'BackpackUI/BtnSearch'). Must be a Control node."}
            }}
        }},
        {"required", {"node_path"}}
    },
    {4, 3, 0}
},

// get_node_rect tool definition
{
    "get_node_rect",
    "Get the screen rectangle (position and size) of a Control node in the "
    "running game. Returns viewport coordinates compatible with inject_input position. "
    "Requires a game to be running with the MCP bridge connected.",
    {
        {"type", "object"},
        {"properties", {
            {"node_path", {
                {"type", "string"},
                {"description", "Path to the Control node relative to the scene root "
                               "(e.g., 'BackpackUI/BtnSearch'). Must be a Control node."}
            }}
        }},
        {"required", {"node_path"}}
    },
    {4, 3, 0}
},
```

### GDScript Node Resolution and Rect Query

```gdscript
# Source: Godot 4.3 API - Control.get_global_rect(), CanvasItem.is_visible_in_tree()
func _handle_get_node_rect(data: Array) -> bool:
    var node_path: String = data[0]
    var node = _resolve_node(node_path)

    if node == null:
        EngineDebugger.send_message("meow_mcp:node_rect_result",
            [false, "Node not found: " + node_path, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
        return true

    if not (node is Control):
        EngineDebugger.send_message("meow_mcp:node_rect_result",
            [false, "Node is not a Control (type: " + node.get_class() + ")",
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
        return true

    if not node.is_visible_in_tree():
        EngineDebugger.send_message("meow_mcp:node_rect_result",
            [false, "Node is not visible in tree", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
        return true

    var rect: Rect2 = node.get_global_rect()
    var global_pos: Vector2 = node.get_global_position()
    EngineDebugger.send_message("meow_mcp:node_rect_result",
        [true, "", rect.position.x, rect.position.y, rect.size.x, rect.size.y,
         global_pos.x, global_pos.y])
    return true
```

### GDScript click_node Handler

```gdscript
# Source: Godot 4.3 API - Input.parse_input_event(), SceneTreeTimer
func _handle_click_node(data: Array) -> bool:
    var node_path: String = data[0]
    var node = _resolve_node(node_path)

    if node == null:
        EngineDebugger.send_message("meow_mcp:click_node_result",
            [false, "Node not found: " + node_path, 0.0, 0.0])
        return true

    if not (node is Control):
        EngineDebugger.send_message("meow_mcp:click_node_result",
            [false, "Node is not a Control (type: " + node.get_class() + ")", 0.0, 0.0])
        return true

    if not node.is_visible_in_tree():
        EngineDebugger.send_message("meow_mcp:click_node_result",
            [false, "Node is not visible in tree", 0.0, 0.0])
        return true

    var rect: Rect2 = node.get_global_rect()
    var center_x: float = rect.position.x + rect.size.x / 2.0
    var center_y: float = rect.position.y + rect.size.y / 2.0

    # Press
    var press_event = InputEventMouseButton.new()
    press_event.position = Vector2(center_x, center_y)
    press_event.global_position = Vector2(center_x, center_y)
    press_event.button_index = MOUSE_BUTTON_LEFT
    press_event.pressed = true
    Input.parse_input_event(press_event)

    # 50ms delay
    await get_tree().create_timer(0.05).timeout

    # Release
    var release_event = InputEventMouseButton.new()
    release_event.position = Vector2(center_x, center_y)
    release_event.global_position = Vector2(center_x, center_y)
    release_event.button_index = MOUSE_BUTTON_LEFT
    release_event.pressed = false
    Input.parse_input_event(release_event)

    EngineDebugger.send_message("meow_mcp:click_node_result",
        [true, "", center_x, center_y])
    return true
```

### C++ Deferred Response Handler (game_bridge.cpp _capture)

```cpp
// In _capture(), add new action handlers:
if (action == "click_node_result") {
    if (pending_type == PendingType::CLICK_NODE && deferred_callback) {
        bool success = p_data[0];
        String error_msg = p_data[1];
        double cx = p_data[2];
        double cy = p_data[3];

        nlohmann::json result;
        if (success) {
            result = {{"success", true},
                     {"clicked_position", {{"x", cx}, {"y", cy}}}};
        } else {
            result = {{"error", std::string(error_msg.utf8().get_data())}};
        }

        auto response = mcp::create_tool_result(pending_id, result);
        deferred_callback(response);
        pending_type = PendingType::NONE;
    }
    return true;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Two calls for click (press+release) | Single call auto-cycles | This phase | AI needs one call instead of two for clicking |
| Screenshot-based coordinate guessing | `get_node_rect` returns exact viewport coords | This phase | Deterministic, pixel-perfect clicking |
| Manual coordinate clicking | `click_node` by path | This phase | AI doesn't need to know coordinates at all |

**Deprecated/outdated:**
- None -- this is new functionality building on proven Phase 10 patterns

## Open Questions

1. **Message data format: Array vs Dictionary**
   - What we know: Existing messages use positional Array arguments (e.g., `[x, y, button_str, pressed]`)
   - What's unclear: Whether to continue with positional arrays or switch to dictionaries for readability in new messages
   - Recommendation: Continue with positional Arrays for consistency with existing code. The companion GDScript already uses this pattern, and switching midway would create inconsistency.

2. **Error reporting granularity for click_node**
   - What we know: User wants "not found / not visible / not Control" distinction
   - What's unclear: Whether to add more error types (e.g., "node has zero size", "node is disabled")
   - Recommendation: Start with the three error types specified. Can extend later.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Python UAT script (uat_phase12.py) |
| Config file | none -- standalone script using TCP JSON-RPC |
| Quick run command | `python tests/uat_phase12.py` |
| Full suite command | `python tests/uat_phase12.py` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| INPT-01 | click auto press+release (no explicit pressed) | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-01 | explicit pressed=true preserves single-fire | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-01 | explicit pressed=false preserves single-fire | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-02 | click_node succeeds on valid Control node | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-02 | click_node returns clicked coordinates | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-02 | click_node error for non-existent node | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-02 | click_node error for non-Control node | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-03 | get_node_rect returns position+size+global_pos | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-03 | get_node_rect error for non-existent node | integration | `python tests/uat_phase12.py` | No -- Wave 0 |
| INPT-03 | get_node_rect error for not-visible node | integration | `python tests/uat_phase12.py` | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** N/A (integration tests require running game)
- **Per wave merge:** `python tests/uat_phase12.py` (requires editor + game running)
- **Phase gate:** Full UAT green before verify-work

### Wave 0 Gaps
- [ ] `tests/uat_phase12.py` -- UAT script covering all INPT requirements
- [ ] Test scene with known UI layout for deterministic coordinate assertions
- [ ] Tool count update in existing phase 10 UAT (38 -> 40 tools)

## Sources

### Primary (HIGH confidence)
- `src/game_bridge.h` / `src/game_bridge.cpp` -- Existing deferred response pattern, inject_input implementation
- `project/addons/meow_godot_mcp/companion/meow_mcp_bridge.gd` -- Existing GDScript message handlers
- `src/mcp_server.cpp` -- Tool dispatch pattern, deferred response handling
- `src/mcp_tool_registry.cpp` -- Tool registration pattern
- [Godot Control class docs](https://docs.godotengine.org/en/stable/classes/class_control.html) -- get_global_rect(), get_global_position()
- [Godot CanvasItem docs](https://docs.godotengine.org/en/stable/classes/class_canvasitem.html) -- is_visible_in_tree()
- [Godot InputEventMouseButton docs](https://docs.godotengine.org/en/stable/classes/class_inputeventmousebutton.html) -- press/release event properties
- [Godot EngineDebugger docs](https://docs.godotengine.org/en/stable/classes/class_enginedebugger.html) -- message capture/send protocol

### Secondary (MEDIUM confidence)
- [Godot SceneTreeTimer docs](https://docs.godotengine.org/en/stable/classes/class_scenetreetimer.html) -- create_timer() for 50ms delay
- [Godot Forum: get_global_rect timing](https://forum.godotengine.org/t/when-can-i-measure-the-rect-of-a-control-node/100975) -- Layout pass timing considerations

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all existing project libraries, no new deps
- Architecture: HIGH -- directly extends proven Phase 10 patterns (deferred response, message protocol)
- Pitfalls: HIGH -- based on direct code reading of existing implementation plus Godot API documentation
- Implementation approach: HIGH -- 50ms delay on GDScript side is idiomatic and non-blocking

**Research date:** 2026-03-20
**Valid until:** 2026-04-20 (stable -- no fast-moving dependencies)
