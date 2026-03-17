---
phase: 04-editor-integration
plan: 01
subsystem: ui
tags: [godot-cpp, dock-panel, version-filtering, tool-registry, editor-plugin]

# Dependency graph
requires:
  - phase: 03-script-project-management
    provides: "12 MCP tools with schemas in mcp_protocol.cpp, MCPServer with IO thread"
provides:
  - "MCPDock class with VBoxContainer UI for editor dock panel"
  - "ToolDef registry with version-filtered tool listing"
  - "MCPServer client_connected atomic flag and has_client() getter"
  - "MCPServer godot_version member with set_godot_version() setter"
  - "MCPPlugin with dock lifecycle, button callbacks, and status polling"
  - "Version detection via Engine::get_version_info()"
affects: [04-02, phase-5]

# Tech tracking
tech-stack:
  added: []
  patterns: ["ToolDef registry pattern for version-gated tool filtering", "MCPDock plain C++ wrapper with VBoxContainer hierarchy", "Status polling via _process() delta accumulator with dirty check"]

key-files:
  created:
    - "src/mcp_dock.h"
    - "src/mcp_dock.cpp"
    - "src/mcp_tool_registry.h"
    - "src/mcp_tool_registry.cpp"
    - "tests/test_tool_registry.cpp"
  modified:
    - "src/mcp_protocol.h"
    - "src/mcp_protocol.cpp"
    - "src/mcp_server.h"
    - "src/mcp_server.cpp"
    - "src/mcp_plugin.h"
    - "src/mcp_plugin.cpp"
    - "tests/CMakeLists.txt"

key-decisions:
  - "ToolDef registry in separate mcp_tool_registry.h/cpp for testability (no Godot headers)"
  - "Backward-compatible create_tools_list_response overload using GodotVersion{99,99,99}"
  - "MCPDock is plain C++ class (not Godot Object) with button signals routed through MCPPlugin"
  - "Status polling at 1.0s interval with dirty check on running/connected/tool_count"

patterns-established:
  - "ToolDef struct with min_version for version-gated tool registration"
  - "MCPDock ownership: plain C++ class owned by MCPPlugin via raw pointer (same as MCPServer)"
  - "Button signal routing: callable_mp(plugin, &MCPPlugin::method) for non-Object UI classes"

requirements-completed: [EDIT-01, EDIT-02, EDIT-03]

# Metrics
duration: 8min
completed: 2026-03-17
---

# Phase 4 Plan 01: Dock Panel UI with Version-Aware Tool Registry Summary

**MCPDock editor panel with three-state status display, Start/Stop/Restart controls, and ToolDef registry filtering 12 tools by detected Godot version**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-17T09:35:06Z
- **Completed:** 2026-03-17T09:43:22Z
- **Tasks:** 2
- **Files modified:** 12

## Accomplishments
- ToolDef registry with all 12 tools, GodotVersion comparison, and version-filtered listing (21 new unit tests)
- MCPDock UI panel with status/port/version/tools labels and Start/Stop/Restart buttons
- MCPPlugin integration: version detection, dock lifecycle, button signal wiring, 1s status polling
- MCPServer: atomic client_connected flag and godot_version member for version-filtered tools/list
- Full backward compatibility: all 117 unit tests pass (96 existing + 21 new)

## Task Commits

Each task was committed atomically:

1. **Task 1: ToolDef registry + MCPServer client_connected** (TDD)
   - `8298033` (test: add failing tests for tool registry) - RED phase
   - `0168a32` (feat: implement registry, version filtering, client_connected) - GREEN phase

2. **Task 2: MCPDock UI + MCPPlugin integration** - `1a1fe18` (feat: dock panel with server controls and status polling)

## Files Created/Modified
- `src/mcp_tool_registry.h` - GodotVersion struct, ToolDef struct, registry API declarations
- `src/mcp_tool_registry.cpp` - All 12 tool definitions with schemas and version filtering
- `src/mcp_dock.h` - MCPDock class with VBoxContainer UI and dirty-check update methods
- `src/mcp_dock.cpp` - Dock construction (labels, buttons), status/button update logic
- `src/mcp_protocol.h` - Added GodotVersion forward declaration and version-filtered overload
- `src/mcp_protocol.cpp` - Refactored to use ToolDef registry (backward-compatible)
- `src/mcp_server.h` - Added client_connected atomic, has_client(), godot_version, set_godot_version()
- `src/mcp_server.cpp` - client_connected updates in IO thread, version-filtered tools/list
- `src/mcp_plugin.h` - Added MCPDock pointer, status_timer, version fields, button callbacks
- `src/mcp_plugin.cpp` - Version detection, dock lifecycle, signal wiring, status polling, callbacks
- `tests/test_tool_registry.cpp` - 21 tests: version comparison, registry, filtering, schemas
- `tests/CMakeLists.txt` - Added test_tool_registry executable, updated test_protocol dependencies

## Decisions Made
- ToolDef registry in separate `mcp_tool_registry.h/cpp` (not in mcp_protocol) for clean separation of data vs protocol logic, and to keep it Godot-free for unit testing
- Backward-compatible `create_tools_list_response(id)` overload delegates to version-filtered overload with GodotVersion{99,99,99}
- MCPDock as plain C++ class (not ClassDB registered) following MCPServer ownership pattern
- Button signals connected to MCPPlugin via `callable_mp()` since MCPDock lacks `get_instance_id()`
- Status polling at 1.0s interval (within CONTEXT.md 0.5-1s range) with dirty check on three state fields

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- MCPDock visible in editor when plugin loads (manual verification in Plan 04-02 UAT)
- ToolDef registry ready for Phase 5 to add version-gated runtime tools
- MCPServer version plumbing ready for prompts protocol (Plan 04-02)
- All unit tests pass, GDExtension builds cleanly

## Self-Check: PASSED

All 12 created/modified files verified present. All 3 task commits (8298033, 0168a32, 1a1fe18) verified in git log.

---
*Phase: 04-editor-integration*
*Completed: 2026-03-17*
