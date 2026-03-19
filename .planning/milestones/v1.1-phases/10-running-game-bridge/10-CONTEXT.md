# Phase 10: Running Game Bridge - Context

**Gathered:** 2026-03-18
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can interact with a running game by injecting input and capturing its viewport. Covers EditorDebuggerPlugin-based IPC between editor and game, companion autoload injection, keyboard/mouse/action input injection, and game viewport screenshot capture. Does NOT cover game state inspection, variable modification, or real-time game event streaming.

Requirements: BRDG-01, BRDG-02, BRDG-03, BRDG-04, BRDG-05

</domain>

<decisions>
## Implementation Decisions

### IPC Architecture — EditorDebuggerPlugin
- Communication via **EditorDebuggerPlugin** — Godot's built-in debugging IPC, zero extra dependencies
- Data format: **Godot Variant arrays** — EditorDebuggerPlugin uses `send_message(name, data)` with Array, natively supports all Godot types
- Connection lifecycle: **Automatic** — EditorDebuggerPlugin connects on game launch, disconnects on game close, editor gets session_started/stopped callbacks
- Editor-side: Register EditorDebuggerPlugin subclass in MCPPlugin
- Game-side: Companion autoload registers as debugger peer, receives and executes commands

### Tool Architecture — 3 MCP Tools
- **inject_input** — Unified input injection with `type` parameter: "key", "mouse", "action"
  - type="key": keycode (string like "W", "space", "ctrl+a") + pressed (bool)
  - type="mouse": action ("move"/"click"/"scroll") + position ({x, y}) + button ("left"/"right"/"middle")
  - type="action": action_name (string) + pressed (bool)
- **capture_game_viewport** — Capture running game's viewport as base64 PNG ImageContent (reuses Phase 9 ImageContent infrastructure)
- **get_game_bridge_status** — Query bridge connection status (connected/disconnected + session info)

### Autoload Injection & Companion Script
- Companion autoload registered in **MCPPlugin::_enter_tree()** via ProjectSettings + add_autoload_singleton
- Companion is **GDScript** — listens for EditorDebuggerPlugin messages, executes input injection, captures viewport
- Companion location: **addons/meow_godot_mcp/companion/** directory — ships with GDExtension
- Plugin cleanup: **MCPPlugin::_exit_tree()** removes autoload registration automatically
- Companion script name: `meow_mcp_bridge.gd` (or similar, Claude's discretion)

### Code Organization
- New `game_bridge.h` / `game_bridge.cpp` — C++ EditorDebuggerPlugin integration + 3 tool functions
- New `addons/meow_godot_mcp/companion/meow_mcp_bridge.gd` — GDScript autoload for game side
- EditorDebuggerPlugin subclass may need separate .h/.cpp or be integrated into game_bridge

### Claude's Discretion
- EditorDebuggerPlugin subclass name and registration details
- Companion script exact message protocol (message names, data array structure)
- Keycode string parsing strategy (Godot Key enum mapping)
- Mouse coordinate handling (viewport-relative vs screen-relative)
- Game viewport capture implementation (screenshot request → response via debugger messages)
- Error handling for disconnected game, unsupported input types
- Whether get_game_bridge_status returns additional session details (PID, scene, etc.)

</decisions>

<canonical_refs>
## Canonical References

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` — Phase 10 maps to BRDG-01..05
- `.planning/ROADMAP.md` — Phase 10 success criteria

### Prior Phase Context
- `.planning/phases/09-editor-viewport-screenshots/09-CONTEXT.md` — ImageContent infrastructure (reuse for game viewport)
- `.planning/phases/05-runtime-signals-distribution/05-CONTEXT.md` — run_game/stop_game patterns

### Existing Implementation
- `src/mcp_server.cpp` — MCPServer: 35 tools, handle_request dispatch
- `src/mcp_protocol.cpp` — create_image_tool_result() builder (Phase 9)
- `src/mcp_plugin.cpp` — EditorPlugin lifecycle (_enter_tree/_exit_tree)
- `src/runtime_tools.cpp` — run_game/stop_game, EditorInterface play APIs

</canonical_refs>

<code_context>
## Existing Code Insights

### Key Godot APIs
- `EditorDebuggerPlugin` — base class for editor-side debugger integration
- `EngineDebugger` — game-side debugger API for registering message handlers
- `Input.parse_input_event()` — inject InputEvents into game's input system
- `InputEventKey`, `InputEventMouseButton`, `InputEventMouseMotion`, `InputEventAction` — event types
- `EditorPlugin::add_debugger_plugin()` — register EditorDebuggerPlugin
- `ProjectSettings::set_setting()` / `add_autoload_singleton()` — autoload management

### Reusable from Phase 9
- `create_image_tool_result()` — ImageContent protocol builder for game viewport screenshots
- PNG → base64 pipeline (save_png_to_buffer → raw_to_base64)

</code_context>

<deferred>
## Deferred Ideas

- Game state inspection (variable reading/writing) — complex, separate feature
- Real-time game event streaming — needs MCP notification support
- Gamepad input injection — specialized, not in requirements
- Touch input injection — mobile-specific
- Game audio capture — complex

</deferred>

---

*Phase: 10-running-game-bridge*
*Context gathered: 2026-03-18*
