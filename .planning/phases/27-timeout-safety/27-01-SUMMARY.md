---
phase: 27-timeout-safety
plan: 01
subsystem: mcp-server
tags: [timeout, threading, condition-variable, stale-response, safety]

# Dependency graph
requires:
  - phase: 26-settings-persistence
    provides: "Stable MCP server with persisted port/tool settings"
provides:
  - "IO thread 30s timeout replacing unbounded wait"
  - "Game bridge 15s deferred request timeout for all 9 operations"
  - "Stale response discard via io_pending_request_id tracking"
affects: [mcp-server, game-bridge, runtime-tools]

# Tech tracking
tech-stack:
  added: []
  patterns: ["wait_for timeout pattern for IO thread response waiting", "deadline-based timeout in poll() for deferred game bridge requests", "request ID tracking for stale response discard"]

key-files:
  created: []
  modified: ["src/mcp_server.h", "src/mcp_server.cpp", "src/game_bridge.h", "src/game_bridge.cpp"]

key-decisions:
  - "Error code -32001 for IO thread timeout (custom code outside JSON-RPC reserved range)"
  - "15s deadline for game bridge deferred requests (covers viewport capture, click_node, eval, etc.)"
  - "Stale response discard by comparing response ID to io_pending_request_id under queue_mutex"
  - "expire_pending clears pending state BEFORE delivering error to prevent race with _capture arriving same frame"

patterns-established:
  - "IO thread wait_for(30s): bounded wait on response_cv prevents dead connections when main thread hangs"
  - "Deadline-based timeout in poll(): game_bridge->has_pending_timeout() checked every frame, expire_pending() delivers descriptive per-operation error"
  - "Request ID tracking: io_pending_request_id set on queue, cleared on send/timeout, checked in queue_deferred_response for stale discard"

requirements-completed: [TIMEOUT-01, TIMEOUT-02, TIMEOUT-03]

# Metrics
duration: 5min
completed: 2026-04-01
---

# Phase 27 Plan 01: Timeout Safety Summary

**30s IO thread timeout and 15s game bridge deferred timeout with stale response discard via request ID tracking**

## Performance

- **Duration:** 5 min
- **Started:** 2026-04-01T02:17:07Z
- **Completed:** 2026-04-01T02:22:25Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced unbounded `response_cv.wait()` with `wait_for(30s)` in IO thread, sending JSON-RPC error -32001 on timeout
- Added 15s deadline to all 9 deferred game bridge operations with per-operation timeout error messages
- Implemented stale response discard in `queue_deferred_response` using `io_pending_request_id` tracking

## Task Commits

Each task was committed atomically:

1. **Task 1: IO thread 30s timeout with request ID tracking and stale response discard** - `cd76dc7` (feat)
2. **Task 2: Game bridge 15s deferred request timeout** - `441fe85` (feat)

## Files Created/Modified
- `src/mcp_server.h` - Added `io_pending_request_id` field for request tracking
- `src/mcp_server.cpp` - 30s IO thread timeout via `wait_for`, stale response discard in `queue_deferred_response`, game bridge timeout check in `poll()`
- `src/game_bridge.h` - Added `pending_deadline` field, `has_pending_timeout()` and `expire_pending()` methods
- `src/game_bridge.cpp` - `expire_pending` implementation with per-type timeout messages, 15s deadline set in all 9 deferred request functions

## Decisions Made
- Used error code -32001 (custom, outside JSON-RPC reserved range -32000 to -32099) for IO thread timeout
- 15s timeout for game bridge operations balances responsiveness with allowing slow operations (viewport capture, test sequences)
- Stale response discard checks both null ID (IO thread already timed out) and ID mismatch (wrong request)
- `expire_pending()` clears `pending_type` to NONE before calling `deferred_callback` to prevent a race where `_capture` arrives on the same frame

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Known Stubs
None - all implementations are complete and wired.

## Next Phase Readiness
- Timeout safety in place for both IO thread and game bridge deferred requests
- MCP connections will no longer hang indefinitely when main thread is stuck or game bridge responses are lost
- Late responses after timeout are safely discarded, preventing response corruption across requests

---
*Phase: 27-timeout-safety*
*Completed: 2026-04-01*
