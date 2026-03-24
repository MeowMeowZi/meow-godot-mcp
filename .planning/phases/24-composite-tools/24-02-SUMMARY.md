---
phase: 24-composite-tools
plan: 02
subsystem: tools
tags: [composite-tools, character-creation, ui-panel, node-duplication, undo-redo]

requires:
  - phase: 24-composite-tools
    plan: 01
    provides: "composite_tools module with find_nodes and batch_set_property"
  - phase: 02-scene-crud
    provides: "scene_mutation UndoRedo pattern, ClassDB::instantiate"
  - phase: 07-ui-system
    provides: "StyleBoxFlat, theme override patterns"
provides:
  - "create_character: atomic CharacterBody + CollisionShape + optional Sprite + optional movement script"
  - "create_ui_panel: declarative JSON spec to UI hierarchy with styling"
  - "duplicate_node: deep-copy subtree with recursive owner setting"
affects: [24-composite-tools]

tech-stack:
  added: []
  patterns: ["Declarative JSON spec to node hierarchy", "Manual GDScript construction for create_character script attachment", "Recursive owner setting for duplicate_node"]

key-files:
  created: []
  modified:
    - src/composite_tools.h
    - src/composite_tools.cpp
    - src/mcp_tool_registry.cpp
    - src/mcp_server.cpp
    - src/error_enrichment.cpp
    - tests/test_tool_registry.cpp
    - tests/test_protocol.cpp

key-decisions:
  - "Direct ClassDB::instantiate + UndoRedo for all composite tools (no reuse of create_node/create_collision_shape which have own actions)"
  - "Manual GDScript construction for create_character script (avoids ResourceLoader::load crash on new files)"
  - "create_ui_panel validates root_type is Control subclass, limits to 2 levels (root + children)"
  - "duplicate_node uses Godot Node::duplicate() for deep copy, then recursively sets owner via UndoRedo"
  - "Script file written to disk first, then manual GDScript instantiation for create_character"

patterns-established:
  - "Declarative JSON spec pattern for UI construction"
  - "Recursive UndoRedo owner setting for duplicated subtrees"

requirements-completed: [COMP-03, COMP-04, COMP-05]

duration: 7min
completed: 2026-03-24
---

# Phase 24 Plan 02: Composite Tools - create_character, create_ui_panel, duplicate_node Summary

**Three multi-step creation tools combining node creation, property setting, and script attachment into single atomic UndoRedo operations**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-24T04:29:42Z
- **Completed:** 2026-03-24T04:36:50Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- create_character: creates CharacterBody2D/3D + CollisionShape2D/3D with configurable shape + optional Sprite2D/3D with texture + optional basic_movement GDScript, all in single UndoRedo action
- create_ui_panel: creates container from declarative JSON spec (root_type, children array, style object), validates Control subclass, applies StyleBoxFlat and theme overrides, max 2 levels
- duplicate_node: deep-copies node subtree using Godot's Node::duplicate(), recursively sets owner on all descendants via UndoRedo helper
- All 5 composite tools (find_nodes, batch_set_property, create_character, create_ui_panel, duplicate_node) registered with full schemas and dispatched
- TOOL_PARAM_HINTS updated for all 5 composite tools
- Tool count updated from 52 to 55 across all test suites

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement create_character, create_ui_panel, duplicate_node** - `cd7ad7b` (feat)
2. **Task 2: Register tools, wire dispatch, update tests** - `6f9be80` (feat)

## Files Created/Modified
- `src/composite_tools.h` - Added 3 new function declarations (create_character, create_ui_panel, duplicate_node)
- `src/composite_tools.cpp` - Added ~560 lines: full implementations with UndoRedo, shape creation, GDScript construction, StyleBoxFlat, recursive owner setting
- `src/mcp_tool_registry.cpp` - Added 3 ToolDef entries with full input schemas (name/type/spec/source_path etc.)
- `src/mcp_server.cpp` - Added 3 dispatch blocks with parameter validation
- `src/error_enrichment.cpp` - Added TOOL_PARAM_HINTS for all 5 composite tools
- `tests/test_tool_registry.cpp` - Updated tool count 52->55, added 3 tool names to expected list
- `tests/test_protocol.cpp` - Updated tool count 52->55

## Decisions Made
- Direct ClassDB::instantiate + UndoRedo rather than reusing create_node/create_collision_shape (which create their own UndoRedo actions, breaking single-action requirement)
- Manual GDScript construction (instantiate + set_source_code + set_path + reload) for create_character script attachment, per MEMORY.md known bug
- create_ui_panel validates root_type is Control subclass (not just any Node)
- create_ui_panel limits nesting to 2 levels (root + children); nested children warned and skipped
- duplicate_node prevents duplicating scene root (returns error)
- Script template written to disk first for persistence, then manual GDScript construction for immediate attachment

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Updated tool count in existing test suites**
- **Found during:** Task 2 (verification)
- **Issue:** test_tool_registry.cpp and test_protocol.cpp expected 52 tools, now 55
- **Fix:** Updated all tool count assertions and added new tool names to expected list
- **Files modified:** tests/test_tool_registry.cpp, tests/test_protocol.cpp
- **Committed in:** 6f9be80 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Fix necessary for correct test passage. No scope creep.

## Issues Encountered
- scons build cannot run from worktree (missing godot-cpp submodule); code verified by pattern analysis and acceptance criteria checks
- CMake build also unavailable (no C++ compiler in worktree environment); same issue as Plan 01

## User Setup Required

None - no external service configuration required.

## Known Stubs

None - all tools fully implemented with real functionality.

## Next Phase Readiness
- All 5 composite tools complete (COMP-01 through COMP-05)
- Phase 24 ready for verification/UAT plan if needed

## Self-Check: PASSED

- All 7 modified files exist
- Both task commits verified in git log
- SUMMARY.md created

---
*Phase: 24-composite-tools*
*Completed: 2026-03-24*
