# Phase 5: Runtime, Signals & Distribution - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can run/stop the game in debug mode, capture stdout/stderr log output for iterative debugging, query and manage signal connections between nodes, and the plugin ships as cross-platform pre-compiled binaries with CI/CD pipeline. Supports Godot 4.3 through 4.6 via godot-cpp compatibility layer.

Requirements: RNTM-01, RNTM-02, RNTM-03, RNTM-04, RNTM-05, RNTM-06, DIST-02, DIST-03

</domain>

<decisions>
## Implementation Decisions

### Game run/stop control (RNTM-01, RNTM-02)
- Single `run_game` tool with `mode` parameter: `"main"` | `"current"` | `"custom"`
- When mode is `"custom"`, accepts optional `scene_path` parameter (e.g., `"res://levels/test.tscn"`)
- Maps to Godot EditorInterface: `play_main_scene()`, `play_current_scene()`, `play_custom_scene(path)`
- When game is already running and `run_game` is called again: return success response with `"running": true` status info (not an error, not a restart)
- Separate `stop_game` tool to stop running game instance

### Debug output capture (RNTM-03)
- Batch pull model: `get_game_output` tool returns accumulated log lines
- AI calls `get_game_output` on-demand (fits MCP request-response model, compatible with stdio bridge)
- No real-time streaming — MCP spec is request-response, not subscription-based

### Signal management (RNTM-04, RNTM-05, RNTM-06)
- Tools: `get_node_signals`, `connect_signal`, `disconnect_signal`
- Query, create, and disconnect signal connections between nodes

### Cross-platform distribution (DIST-02, DIST-03)
- Pre-compiled binaries for Windows (x86_64), Linux (x86_64), macOS (universal)
- GitHub Actions CI/CD pipeline for automated builds
- Support Godot 4.3, 4.4, 4.5, 4.6 via godot-cpp compatibility
- User may self-host release distribution — keep packaging flexible

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

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` -- Full v1 requirements list, Phase 5 maps to RNTM-01..06, DIST-02, DIST-03
- `.planning/ROADMAP.md` -- Phase 5 success criteria, dependency chain, overall project progress

### Prior Phase Context
- `.planning/phases/1/1-CONTEXT.md` -- Phase 1 decisions: connection model (port 6800), bridge relay architecture, error handling patterns
- `.planning/phases/03-script-project-management/03-CONTEXT.md` -- Phase 3 decisions: IO thread + queue/promise threading model, tool dispatch patterns, file scope (res://)
- `.planning/phases/04-editor-integration/04-CONTEXT.md` -- Phase 4 decisions: ToolDef registry with version filtering, dock panel architecture, MCP Prompts protocol

### Existing Implementation
- `src/mcp_server.h` / `src/mcp_server.cpp` -- MCPServer class, two-thread architecture (IO + main), handle_request dispatch, poll() pattern
- `src/mcp_plugin.h` / `src/mcp_plugin.cpp` -- MCPPlugin (EditorPlugin), owns MCPServer + MCPDock, _enter_tree/_exit_tree lifecycle, _process poll loop
- `src/mcp_tool_registry.h` / `src/mcp_tool_registry.cpp` -- ToolDef struct with GodotVersion min_version, get_filtered_tools_json()
- `src/mcp_protocol.h` / `src/mcp_protocol.cpp` -- JSON-RPC protocol layer, MCP message builders, tool/resource/prompt response builders
- `SConstruct` -- SCons build: GDExtension shared library + standalone bridge executable, Windows/Linux cross-compile

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `MCPServer::handle_request()`: tool dispatch via if-else chain on tool name — new runtime/signal tools plug in here
- `ToolDef` registry in `mcp_tool_registry.h`: version-gated tool registration — runtime tools may need higher min_version for certain Godot APIs
- `mcp_protocol.h` builders: `create_tool_result()`, `create_error_response()` — reuse for all new tool responses
- `MCPPlugin::_process()`: main thread poll loop — game state polling hooks here
- `SConstruct`: bridge_env pattern for standalone executables — CI pipeline extends this build system

### Established Patterns
- Each tool domain has own header/cpp pair: `scene_tools`, `scene_mutation`, `script_tools`, `project_tools` — new: `runtime_tools`, `signal_tools`
- Tools are free functions returning `nlohmann::json` results
- Tool dispatch in `handle_request()` via if-else chain on tool name
- MCPServer is plain C++ class (not Godot Object) owned by MCPPlugin via raw pointer
- IO thread handles TCP/parse, main thread handles all Godot API calls (thread-safe queue/promise)
- `GODOT_MCP_MEOW_GODOT_ENABLED` define for dual-mode compilation (test vs runtime)

### Integration Points
- `handle_request()`: add `run_game`, `stop_game`, `get_game_output`, `get_node_signals`, `connect_signal`, `disconnect_signal` handlers
- `mcp_tool_registry.cpp`: register new ToolDef entries with appropriate min_version
- `MCPPlugin`: may need to capture EditorInterface reference for play_main_scene/play_current_scene/play_custom_scene
- Log capture: Godot's `add_custom_logger()` or EditorPlugin output hooks — requires research
- Signal API: `Object::get_signal_list()`, `Object::get_signal_connection_list()`, `Object::connect()`, `Object::disconnect()`

</code_context>

<specifics>
## Specific Ideas

- `run_game` tool mirrors editor F5/F6/F7 shortcuts — AI gets the same control as a human developer
- Debug output capture enables the core AI debugging workflow: run game → see errors → fix code → re-run
- Signal management completes the "AI can wire up game logic" story — create nodes, write scripts, connect signals
- User may self-host distribution rather than using standard GitHub Releases — keep packaging modular

</specifics>

<deferred>
## Deferred Ideas

- Real-time log streaming via MCP notifications — v1 uses batch pull, streaming if users request
- Game input injection (keyboard/mouse/Action events) — ADVR-01, v2 requirement
- Viewport screenshot capture — ADVR-02, v2 requirement
- Asset Library submission — evaluate after v1 stability proven
- Scheduled/automated testing across Godot versions — add when user base grows

</deferred>

---

*Phase: 05-runtime-signals-distribution*
*Context gathered: 2026-03-17*
