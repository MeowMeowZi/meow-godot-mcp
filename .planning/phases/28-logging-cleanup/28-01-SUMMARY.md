---
phase: 28-logging-cleanup
plan: 01
subsystem: dx
tags: [logging, push_error, error-enrichment, tool-hints, cleanup]

# Dependency graph
requires:
  - phase: 22-error-enrichment
    provides: TOOL_PARAM_HINTS map and error enrichment system
provides:
  - push_error() for TCP server and game input errors (Debugger > Errors visibility)
  - Cleaned TOOL_PARAM_HINTS with exact 30-tool 1:1 registry match
affects: [error-enrichment, mcp-server, game-bridge]

# Tech tracking
tech-stack:
  added: []
  patterns: [push_error for actual errors vs print for informational messages]

key-files:
  created: []
  modified: [src/mcp_server.cpp, src/game_bridge.cpp, src/error_enrichment.cpp]

key-decisions:
  - "push_error() for error conditions, print() for informational -- consistent pattern"
  - "TOOL_PARAM_HINTS sorted alphabetically for maintainability"

patterns-established:
  - "Error severity: push_error() for failures users need to act on, print() for status/info"

requirements-completed: [LOG-01, CLEAN-01]

# Metrics
duration: 15min
completed: 2026-04-01
---

# Phase 28 Plan 01: Logging Cleanup Summary

**push_error() replacing printerr() for TCP/input errors, and TOOL_PARAM_HINTS cleaned from 50 to 30 entries matching current registry**

## Performance

- **Duration:** 15 min
- **Started:** 2026-04-01T02:33:09Z
- **Completed:** 2026-04-01T02:47:44Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Replaced 2 printerr() calls with push_error() so plugin errors appear in both Output and Debugger > Errors panels
- Cleaned TOOL_PARAM_HINTS: removed 28 stale entries from 59-tool era, added 6 missing entries for current tools
- Alphabetically sorted all 30 TOOL_PARAM_HINTS entries for maintainability
- Build compiles cleanly with no errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace printerr() with push_error() for actual errors (LOG-01)** - `eabaad6` (fix)
2. **Task 2: Clean TOOL_PARAM_HINTS to match current 30-tool registry (CLEAN-01)** - `a4006fd` (chore)

## Files Created/Modified
- `src/mcp_server.cpp` - printerr() -> push_error() for TCP server listen failure
- `src/game_bridge.cpp` - printerr() -> push_error() for game input injection error
- `src/error_enrichment.cpp` - TOOL_PARAM_HINTS cleaned to 30 entries, alphabetically sorted

## Decisions Made
- push_error() for error conditions, print() for informational -- consistent severity pattern established
- TOOL_PARAM_HINTS sorted alphabetically for easier maintenance and review

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Known Stubs
None - all changes are complete and functional.

## Next Phase Readiness
- Logging cleanup complete for v1.6 milestone
- All 3 files compile cleanly
- TOOL_PARAM_HINTS now in sync with mcp_tool_registry.cpp

## Self-Check: PASSED

- All 3 source files exist and were modified
- Both task commits (eabaad6, a4006fd) verified in git log
- SUMMARY.md created at expected path

---
*Phase: 28-logging-cleanup*
*Completed: 2026-04-01*
