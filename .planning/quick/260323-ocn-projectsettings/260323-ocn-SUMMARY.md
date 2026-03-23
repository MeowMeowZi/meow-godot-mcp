---
phase: quick
plan: 260323-ocn
subsystem: server
tags: [tcp, port-management, project-settings, multi-instance]

requires:
  - phase: none
    provides: n/a
provides:
  - ProjectSettings-based port configuration (meow_mcp/server/port)
  - Auto-increment port on conflict for multi-editor support
  - Configure MCP command includes --port flag
affects: [mcp-server, mcp-plugin, bridge-configuration]

tech-stack:
  added: []
  patterns: [auto-increment port retry loop, ProjectSettings registration with property info]

key-files:
  created: []
  modified:
    - src/mcp_server.h
    - src/mcp_server.cpp
    - src/mcp_plugin.cpp

key-decisions:
  - "MCPServer::start() returns int (actual port) instead of void, enabling caller to know which port was bound"
  - "Auto-increment up to +10 ports on conflict, sufficient for typical multi-editor scenarios"
  - "Port re-read from ProjectSettings on toggle/restart to pick up user changes without editor restart"

patterns-established:
  - "Port retry pattern: for-loop with start(port+i) checking return > 0"
  - "ProjectSettings registration: set_setting + add_property_info + set_initial_value"

requirements-completed: []

duration: 2min
completed: 2026-03-23
---

# Quick Task 260323-ocn: ProjectSettings Port + Auto-Increment Summary

**Multi-port support via ProjectSettings with auto-increment on conflict for simultaneous editor instances**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-23T09:33:14Z
- **Completed:** 2026-03-23T09:34:57Z
- **Tasks:** 1
- **Files modified:** 3

## Accomplishments
- ProjectSettings `meow_mcp/server/port` registered with default 6800 (range 1024-65535, visible in editor)
- Auto-increment port on conflict: tries up to 10 consecutive ports when configured port is occupied
- MCPServer::start() now returns actual port used (int) or 0 on failure
- Configure MCP command includes `--port` flag matching actual bound port
- Warning logged when port auto-changed due to conflict
- Toggle/restart re-read port from ProjectSettings to pick up user changes

## Task Commits

Each task was committed atomically:

1. **Task 1: Auto-port with ProjectSettings and conflict detection** - `fe5bd82` (feat)

## Files Created/Modified
- `src/mcp_server.h` - Changed start() return type from void to int
- `src/mcp_server.cpp` - start() returns actual port on success, 0 on failure
- `src/mcp_plugin.cpp` - ProjectSettings registration, auto-increment retry loop in _enter_tree/_on_toggle/_on_restart, --port in configure command

## Decisions Made
- MCPServer::start() returns int instead of void to enable port conflict detection at call site
- Max 10 port attempts is sufficient for typical multi-editor scenarios (rarely more than a few concurrent)
- Port is re-read from ProjectSettings on every toggle/restart, so users can change it without restarting the editor

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Steps
- Blocker "Port management for multiple editor instances" in STATE.md is now resolved
- Bridge executable may need corresponding --port argument handling (already receives it via configure command)

---
*Quick task: 260323-ocn-projectsettings*
*Completed: 2026-03-23*
