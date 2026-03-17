---
phase: 03-script-project-management
plan: 02
subsystem: api
tags: [mcp-resources, project-tools, io-threading, queue-promise, godot-api, diraccess, projectsettings, resourceloader]

# Dependency graph
requires:
  - phase: 03-01
    provides: "Script tools module, tool dispatch chain with 9 tools"
  - phase: 01-04
    provides: "MCP server TCP polling, protocol layer, scene tools"
provides:
  - "3 project tools: list_project_files, get_project_settings, get_resource_info"
  - "MCP Resources protocol: resources/list, resources/read with 2 URIs"
  - "Resources capability in initialize response"
  - "IO thread + queue/promise pattern for MCPServer"
  - "12 tools total in tools/list"
affects: [03-03, 04-editor-ui, 05-runtime]

# Tech tracking
tech-stack:
  added: [std::thread, std::mutex, std::condition_variable, std::atomic, std::queue]
  patterns: [io-thread-queue-promise, pending-request-response-structs, process-message-io-inline-handling]

key-files:
  created:
    - src/project_tools.h
    - src/project_tools.cpp
  modified:
    - src/mcp_protocol.h
    - src/mcp_protocol.cpp
    - src/mcp_server.h
    - src/mcp_server.cpp
    - tests/test_protocol.cpp

key-decisions:
  - "IO thread handles TCP + JSON-RPC parse; main thread handles all Godot API dispatch via queue"
  - "Notifications and parse errors handled inline on IO thread (no queue round-trip needed)"
  - "IO thread uses condition_variable wait for synchronous request/response flow"
  - "ProjectSettings iteration uses get_property_list() with usage flag filtering"
  - "Resource inspection filters properties by STORAGE/EDITOR usage, skips INTERNAL"

patterns-established:
  - "PendingRequest/PendingResponse structs for typed thread-safe message passing"
  - "process_message_io returns bool: true=handled inline, false=queued for main thread"
  - "collect_files recursive helper with DirAccess for project file traversal"

requirements-completed: [PROJ-01, PROJ-02, PROJ-03, PROJ-04, MCP-04]

# Metrics
duration: 5min
completed: 2026-03-17
---

# Phase 3 Plan 2: Project Tools, MCP Resources, and IO Threading Summary

**3 project query tools (files/settings/resources), MCP Resources protocol with godot:// URIs, and IO thread queue/promise refactor for MCPServer**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-17T02:30:39Z
- **Completed:** 2026-03-17T02:35:57Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Project tools module with list_project_files (DirAccess recursive traversal), get_project_settings (ProjectSettings iteration), and get_resource_info (ResourceLoader + property inspection)
- MCP Resources protocol: resources/list returns godot://scene_tree and godot://project_files; resources/read serves scene tree JSON and file listing JSON
- Initialize response now advertises resources capability alongside tools; protocol version bumped to 0.2.0
- MCPServer refactored to two-thread architecture: IO thread handles TCP accept/read/write + JSON-RPC parsing, main thread executes Godot API calls via queue/promise pattern
- 12 tools total registered in tools/list (4 scene + 5 script + 3 project)
- 7 new unit tests added (96 total, all passing)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create project_tools module + register tools + add MCP Resources protocol** - `1137535` (feat)
2. **Task 2: Refactor MCPServer for IO thread + queue/promise pattern** - `9e46bc0` (refactor)

## Files Created/Modified
- `src/project_tools.h` - Project tool function declarations (list_project_files, get_project_settings, get_resource_info)
- `src/project_tools.cpp` - Project tool implementations using DirAccess, ProjectSettings, ResourceLoader
- `src/mcp_protocol.h` - Added create_resources_list_response and create_resource_read_response declarations
- `src/mcp_protocol.cpp` - 3 project tool schemas, resources capability, version 0.2.0, resource protocol builders
- `src/mcp_server.h` - IO thread members, PendingRequest/PendingResponse structs, mutex/queue/atomic/cv
- `src/mcp_server.cpp` - IO thread function, process_message_io, queue-based poll(), resources method handlers
- `tests/test_protocol.cpp` - 7 new tests: 3 tool registration, resources capability, 2 resource builders, updated counts

## Decisions Made
- IO thread handles TCP accept/read/write and JSON-RPC parsing; main thread handles all Godot API dispatch via queue -- this keeps all Godot scene tree access on the main thread for safety
- Notifications (initialized) and JSON parse errors handled inline on IO thread since they need no Godot API calls -- avoids unnecessary queue round-trips
- IO thread uses condition_variable::wait to block until main thread produces a response -- ensures synchronous JSON-RPC request/response flow over TCP
- ProjectSettings property iteration uses get_property_list() with usage flag filtering to skip internal/private properties
- Resource inspection filters properties by PROPERTY_USAGE_STORAGE or PROPERTY_USAGE_EDITOR, skips PROPERTY_USAGE_INTERNAL

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All 12 tools registered and dispatched (4 scene + 5 script + 3 project)
- MCP Resources protocol fully functional with 2 resource URIs
- IO thread architecture ready for concurrent request handling
- Phase 3 Plan 3 (EditorFileSystem scan integration, remaining tests) can proceed

## Self-Check: PASSED

All 7 created/modified files verified on disk. Both task commits (1137535, 9e46bc0) verified in git log.

---
*Phase: 03-script-project-management*
*Completed: 2026-03-17*
