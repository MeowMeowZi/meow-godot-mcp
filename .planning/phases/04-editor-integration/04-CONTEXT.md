# Phase 4: Editor Integration - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

The plugin provides a polished editor experience with a dock panel showing connection status and server controls, version-aware tool filtering, and MCP prompt templates for common Godot workflows.

Requirements: EDIT-01, EDIT-02, EDIT-03, EDIT-04

</domain>

<decisions>
## Implementation Decisions

### Dock panel placement and architecture
- Right-side dock (alongside Inspector), using `add_control_to_dock(DOCK_SLOT_RIGHT_BL, ...)`
- Pure C++ UI built with godot-cpp (VBoxContainer, Label, Button, etc.)
- Separate MCPDock class (pure C++ wrapper, NOT registered to ClassDB) — manages a VBoxContainer instance internally
- MCPPlugin creates and owns MCPDock, passes its root Control to `add_control_to_dock()`
- Follows same pattern as MCPServer: plain C++ class owned by MCPPlugin via raw pointer

### Status display
- Basic status info: connection state (Connected/Disconnected/Waiting), listening port, Godot version, available tool count
- No request log/history — keep it simple for v1
- No client name display (would require extracting from MCP initialize handshake — defer)

### Control buttons
- Single toggle button switching between Start/Stop (button text changes with state) + separate Restart button
- 2 buttons total

### Status update mechanism
- Polling via timer: accumulate delta in `_process()`, check MCPServer state every 0.5-1 second
- Only update UI labels/buttons when state actually changes (dirty check)
- Event-driven considered but rejected: MCPServer is a plain C++ class with IO thread — callbacks from IO thread to UI are thread-unsafe, and atomic flag polling is functionally equivalent

### Version detection and tool filtering
- Framework approach: build ToolDef struct with name, min_version, schema — filter tools at initialization
- Current phase: all tools set to min_version 4.3 (no actual filtering happens yet)
- Phase 5 Runtime tools may introduce real version gates — framework ready for that
- Version detected at runtime via `Engine.get_version_info()` (returns Dictionary with major/minor/patch/status)
- Detected version displayed in Dock panel

### MCP Prompt templates
- Parameterized templates with arguments (e.g., `create_player_controller(movement_type="2d_platformer")`)
- 3-4 core workflow templates: create player controller, set up scene structure, debug physics, create UI interface
- Hardcoded in C++ (same pattern as tool schemas in `mcp_protocol.cpp`)
- Requires adding `prompts` capability to MCP initialize response
- Implements `prompts/list` and `prompts/get` MCP methods

### Claude's Discretion
- Exact UI layout spacing, font sizes, colors for dock panel
- Specific prompt template content and parameter names
- ToolDef data structure details and where to store the registry
- How to expose MCPServer state to MCPDock (direct pointer, getter methods, etc.)
- Timer interval precision (0.5s vs 1s)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### MCP Protocol
- `.planning/REQUIREMENTS.md` -- Full v1 requirements list, Phase 4 maps to EDIT-01..04
- `.planning/ROADMAP.md` -- Phase 4 success criteria and dependency chain

### Prior Phase Context
- `.planning/phases/1/1-CONTEXT.md` -- Phase 1 decisions: connection model, bridge behavior, port 6800 default
- `.planning/phases/03-script-project-management/03-CONTEXT.md` -- Phase 3 decisions: tool patterns, IO thread architecture, MCP Resources protocol

### Existing Implementation
- `src/mcp_plugin.h` / `src/mcp_plugin.cpp` -- MCPPlugin class, `_enter_tree()` auto-start, `_process()` poll loop — dock panel hooks here
- `src/mcp_server.h` / `src/mcp_server.cpp` -- MCPServer class with `start()`/`stop()`/`is_running()`, two-thread architecture, tool dispatch
- `src/mcp_protocol.h` / `src/mcp_protocol.cpp` -- Tool schemas in `create_tools_list_response()`, capability declarations in `create_initialize_response()` — needs prompts capability + tool registry refactor
- `project/addons/godot_mcp_meow/plugin.gd` -- Thin 2-line GDScript wrapper (`@tool extends MCPPlugin`)
- `project/addons/godot_mcp_meow/plugin.cfg` -- Plugin metadata

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `MCPServer::is_running()`: already exposes server running state — MCPDock can query this
- `MCPServer::start(port)` / `MCPServer::stop()`: lifecycle methods ready for dock button wiring
- `mcp_protocol.cpp::create_tools_list_response()`: tool schema registration pattern — refactor to ToolDef array for version filtering
- `mcp_protocol.cpp::create_initialize_response()`: capability declaration — add `prompts` capability
- `mcp_protocol.cpp`: resource protocol builders pattern — reuse for prompts/list and prompts/get builders

### Established Patterns
- MCPServer is plain C++ class owned by MCPPlugin via raw pointer — MCPDock follows same pattern
- Each tool domain has own header/cpp pair — new files: `mcp_dock.h/cpp`, `mcp_prompts.h/cpp`
- Tool schemas are static JSON in `mcp_protocol.cpp` — version-gated registry replaces this
- Protocol handlers dispatched in `handle_request()` via if-else chain — add prompts/* methods
- EditorPlugin `add_control_to_dock()` requires a Godot Control node — MCPDock provides its internal VBoxContainer

### Integration Points
- MCPPlugin `_enter_tree()`: create MCPDock, call `add_control_to_dock()`
- MCPPlugin `_exit_tree()`: `remove_control_from_docks()`, delete MCPDock
- MCPPlugin `_process()`: call MCPDock update (timer-based state polling)
- `handle_request()`: add `prompts/list` and `prompts/get` method handlers
- `create_initialize_response()`: add `prompts` to capabilities object
- `create_tools_list_response()`: refactor from hardcoded JSON to ToolDef registry filtered by version

</code_context>

<specifics>
## Specific Ideas

- MCPDock follows the exact same ownership pattern as MCPServer: plain C++ class, raw pointer in MCPPlugin, created in `_enter_tree`, deleted in `_exit_tree`
- Version detection framework is forward-looking — Phase 5 Runtime tools (game run/stop, signal management) are the first likely candidates for version-gated features
- Prompt templates should give AI enough context to execute multi-step workflows without additional guidance — "create a 2D platformer player controller" should result in node creation + script attachment + property setup

</specifics>

<deferred>
## Deferred Ideas

- Request log/history display in dock panel — add if users want debugging visibility
- Client name display from MCP initialize handshake — requires MCPServer to extract and expose clientInfo
- User-configurable/extensible prompt templates via external JSON — hardcoded is sufficient for v1
- Port configuration UI in dock panel — currently hardcoded 6800, configurable via Project Settings in future

</deferred>

---

*Phase: 04-editor-integration*
*Context gathered: 2026-03-17*
