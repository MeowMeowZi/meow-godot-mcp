---
phase: 02-scene-crud
plan: 02
subsystem: scene
tags: [scene-mutation, undo-redo, create-node, delete-node, set-property, godot-cpp, EditorUndoRedoManager, ClassDB]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: "MCP server, protocol layer, TCPServer, tool dispatch pattern"
  - phase: 02-scene-crud/01
    provides: "parse_variant for string-to-Godot-type conversion"
provides:
  - "create_node MCP tool -- create any built-in node type with properties and UndoRedo"
  - "set_node_property MCP tool -- modify any node property with UndoRedo"
  - "delete_node MCP tool -- remove nodes with UndoRedo (undo restores)"
  - "MCPServer.set_undo_redo() -- UndoRedo plumbing from plugin to server"
affects: [02-03-PLAN, UAT, scene-crud-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: ["EditorUndoRedoManager action wrapping for all scene mutations", "ClassDB::instantiate + Object::cast_to<Node> for safe node creation", "Static ClassDB convenience methods (class_exists, is_parent_class, instantiate)"]

key-files:
  created:
    - src/scene_mutation.h
    - src/scene_mutation.cpp
    - tests/test_scene_mutation.cpp
  modified:
    - src/mcp_server.h
    - src/mcp_server.cpp
    - src/mcp_plugin.cpp
    - src/mcp_protocol.cpp
    - tests/test_protocol.cpp
    - tests/CMakeLists.txt
    - SConstruct

key-decisions:
  - "GODOT_MCP_MEOW_GODOT_ENABLED added to SConstruct CPPDEFINES rather than per-file #define -- ensures all source files consistently enable Godot-dependent code paths"
  - "Properties set AFTER add_child/set_owner within same UndoRedo action -- some properties only work correctly on nodes in the tree"
  - "ClassDB static convenience methods used (ClassDB::class_exists) rather than ClassDBSingleton::get_singleton() -- cleaner API"

patterns-established:
  - "Scene mutation functions accept EditorUndoRedoManager* as parameter -- decouples from plugin lifecycle"
  - "Tool dispatch pattern: extract args from JSON, validate required params, call implementation function, wrap result in create_tool_result"
  - "UndoRedo action naming convention: 'MCP: <verb> <subject>' (e.g., 'MCP: Create Sprite2D', 'MCP: Set position', 'MCP: Delete MyNode')"

requirements-completed: [SCNE-02, SCNE-03, SCNE-04, SCNE-05]

# Metrics
duration: 9min
completed: 2026-03-16
---

# Phase 2 Plan 02: Scene Mutation Tools Summary

**Three scene mutation MCP tools (create_node, set_node_property, delete_node) with full EditorUndoRedoManager integration and ClassDB-based node instantiation**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-16T10:05:50Z
- **Completed:** 2026-03-16T10:15:16Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments
- Implemented create_node with ClassDB validation (class_exists + is_parent_class), safe Variant-to-Node* extraction, initial property setting, and UndoRedo wrapping
- Implemented set_node_property with parse_variant integration for automatic string-to-Godot-type conversion and UndoRedo wrapping
- Implemented delete_node with scene root protection, parent-based removal, and undo_reference for redo support
- Registered all three tools in MCP protocol with full JSON schemas and dispatched through server
- All 70 tests pass (11 new: 3 protocol tool registration + 8 scene mutation contract tests)
- GDExtension builds successfully

## Task Commits

Each task was committed atomically:

1. **Task 1: Create scene_mutation module + wire UndoRedo** - `7ca673d` (feat)
2. **Task 2: Register tools in MCP protocol + update tests** - `747fe4c` (test)

## Files Created/Modified
- `src/scene_mutation.h` - Function declarations for create_node, set_node_property, delete_node
- `src/scene_mutation.cpp` - Full implementations with EditorUndoRedoManager, ClassDB validation, parse_variant integration
- `src/mcp_server.h` - Added set_undo_redo() setter and EditorUndoRedoManager* member
- `src/mcp_server.cpp` - Added scene_mutation.h include, set_undo_redo method, tool dispatch for 3 new tools
- `src/mcp_plugin.cpp` - Wired get_undo_redo() to server in _enter_tree
- `src/mcp_protocol.cpp` - Registered create_node, set_node_property, delete_node with JSON schemas
- `tests/test_protocol.cpp` - Updated tool count to 4, added 3 schema validation tests
- `tests/test_scene_mutation.cpp` - 8 contract tests for argument/response JSON formats
- `tests/CMakeLists.txt` - Added test_scene_mutation target
- `SConstruct` - Added GODOT_MCP_MEOW_GODOT_ENABLED to CPPDEFINES for all GDExtension sources

## Decisions Made
- Added GODOT_MCP_MEOW_GODOT_ENABLED to SConstruct CPPDEFINES so variant_parser.cpp compiles parse_variant for the GDExtension build (was previously only activated per-file)
- Used ClassDB static convenience methods rather than ClassDBSingleton::get_singleton() for cleaner code
- Properties are set AFTER add_child/set_owner within the same UndoRedo action because some properties only work on nodes in the tree

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added GODOT_MCP_MEOW_GODOT_ENABLED to SConstruct CPPDEFINES**
- **Found during:** Task 1 (GDExtension build verification)
- **Issue:** Linker error -- variant_parser.cpp did not compile parse_variant() because GODOT_MCP_MEOW_GODOT_ENABLED was not defined during GDExtension compilation. The plan assumed per-file #define was sufficient, but variant_parser.cpp needs the define at compile time too.
- **Fix:** Added `env.Append(CPPDEFINES=["GODOT_MCP_MEOW_GODOT_ENABLED"])` to SConstruct for all GDExtension sources
- **Files modified:** SConstruct
- **Verification:** GDExtension build succeeds (scons exits 0)
- **Committed in:** 7ca673d (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Essential fix for linking. The CPPDEFINES approach is cleaner than per-file defines.

## Issues Encountered
None beyond the auto-fixed deviation above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All three scene mutation tools are callable via MCP protocol
- Plan 03 (UAT integration) can test create_node, set_node_property, delete_node end-to-end in the Godot editor
- The UndoRedo plumbing from plugin to server is complete and tested
- tools/list returns 4 tools total (get_scene_tree + 3 new)

## Self-Check: PASSED

- All 10 created/modified files exist on disk
- Both task commits (7ca673d, 747fe4c) found in git log
- GDExtension build passes (scons exits 0)
- All 70 tests pass (ctest exits 0)

---
*Phase: 02-scene-crud*
*Completed: 2026-03-16*
