---
phase: 15-integration-testing-toolkit
plan: 01
subsystem: testing
tags: [mcp, integration-testing, test-sequence, assertions, prompt-template]

# Dependency graph
requires:
  - phase: 12-input-injection-enhancement
    provides: "click_node, get_node_rect deferred tools"
  - phase: 13-runtime-state-query
    provides: "get_game_node_property, eval_in_game, get_game_scene_tree deferred tools"
  - phase: 14-game-output-enhancement
    provides: "get_game_output buffered log capture"
provides:
  - "run_test_sequence MCP tool for batch test execution with assertions"
  - "test_game_ui prompt template (4 variants: button_click, form_validation, navigation, state_verification)"
  - "RUN_TEST_SEQUENCE pending type and async state machine pattern"
affects: [15-02-integration-testing-uat]

# Tech tracking
tech-stack:
  added: []
  patterns: ["async state machine for sequential deferred tool orchestration", "assertion evaluation with equals/contains/not_empty operators"]

key-files:
  created: []
  modified:
    - src/game_bridge.h
    - src/game_bridge.cpp
    - src/mcp_tool_registry.cpp
    - src/mcp_server.cpp
    - src/mcp_prompts.cpp

key-decisions:
  - "Async state machine approach instead of promise/future (avoids main thread deadlock)"
  - "Deferred capture handlers route to _advance_test_sequence when pending_type == RUN_TEST_SEQUENCE"
  - "wait action uses OS::delay_usec to block main thread briefly (acceptable for test tool)"
  - "Bypass individual tool methods and send directly to game via send_to_game for sub-steps"

patterns-established:
  - "State machine orchestration: RUN_TEST_SEQUENCE pending type manages multi-step deferred workflows"
  - "Assertion evaluation: property-based checking with equals/contains/not_empty/exists operators"

requirements-completed: [TEST-01, TEST-02, TEST-03]

# Metrics
duration: 5min
completed: 2026-03-20
---

# Phase 15 Plan 01: Integration Testing Toolkit Summary

**run_test_sequence batch test tool with async state machine orchestration and test_game_ui prompt template with 4 workflow variants**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-19T20:55:23Z
- **Completed:** 2026-03-19T21:00:23Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Implemented run_test_sequence MCP tool that executes sequential test steps against the running game
- Async state machine (RUN_TEST_SEQUENCE pending type) orchestrates deferred tools without deadlocking
- Assertion system supports equals, contains, not_empty operators with structured pass/fail reporting
- Added test_game_ui prompt template with 4 variants covering common UI testing workflows

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement run_test_sequence tool (registry + dispatch + game_bridge)** - `c4c8150` (feat)
2. **Task 2: Add test_game_ui prompt template** - `e828e16` (feat)

## Files Created/Modified
- `src/game_bridge.h` - Added RUN_TEST_SEQUENCE pending type, test sequence state members, method declarations
- `src/game_bridge.cpp` - Implemented run_test_sequence_tool, _execute_test_step, _advance_test_sequence, _evaluate_assertion; modified 5 capture handlers to route to state machine
- `src/mcp_tool_registry.cpp` - Added run_test_sequence tool definition with steps array schema
- `src/mcp_server.cpp` - Added dispatch for run_test_sequence with deferred response handling
- `src/mcp_prompts.cpp` - Added test_game_ui prompt template with button_click, form_validation, navigation, state_verification variants

## Decisions Made
- Async state machine approach instead of promise/future to avoid main thread deadlock (deferred callback and run_test_sequence both execute on main thread)
- Bypass individual tool methods (click_node_tool etc.) for sub-steps; send directly to game via send_to_game since pending_type checks would reject nested calls
- wait action uses OS::delay_usec for simplicity (blocks main thread briefly, acceptable for typical 100-500ms test waits)
- Assertion evaluation uses string comparison for all operators (values are string-serialized from Godot var_to_str)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- run_test_sequence tool ready for UAT testing in 15-02
- test_game_ui prompt template registered and queryable via prompts/list and prompts/get
- Total tools: 33 (32 + run_test_sequence), Total prompts: 7 (6 + test_game_ui)

---
*Phase: 15-integration-testing-toolkit*
*Completed: 2026-03-20*
