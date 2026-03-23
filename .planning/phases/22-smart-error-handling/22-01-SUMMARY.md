---
phase: 22-smart-error-handling
plan: 01
subsystem: error-handling
tags: [error-enrichment, fuzzy-matching, mcp-protocol, isError, dispatch-layer]
dependency_graph:
  requires: []
  provides: [error_enrichment_module, create_tool_error_result, make_tool_response, levenshtein_distance]
  affects: [mcp_server, mcp_protocol]
tech_stack:
  added: []
  patterns: [dispatch-layer-interception, error-category-routing, levenshtein-fuzzy-match]
key_files:
  created:
    - src/error_enrichment.h
    - src/error_enrichment.cpp
    - tests/test_error_enrichment.cpp
  modified:
    - src/mcp_protocol.h
    - src/mcp_protocol.cpp
    - src/mcp_server.cpp
    - tests/test_protocol.cpp
    - tests/CMakeLists.txt
decisions:
  - "String prefix matching for error categorization (not regex) per research anti-patterns"
  - "enrich_error_with_context uses Godot ClassDB for class fuzzy matching, cached on first use"
  - "make_tool_response helper replaces all 59 tool dispatch create_tool_result calls"
  - "Deferred response errors enriched in queue_deferred_response (single enrichment point)"
  - "Success-only paths (bridge-wait, restart_editor) keep create_tool_result unchanged"
metrics:
  duration: "11 min"
  completed: "2026-03-24"
  tasks: 2
  files: 8
  tests_added: 41
---

# Phase 22 Plan 01: Error Enrichment Infrastructure Summary

Error enrichment module with Levenshtein fuzzy matching, 9-category error classification, and dispatch-layer interception wrapping all 50+ tool error paths into isError:true diagnostic responses.

## Completed Tasks

| Task | Name | Commit | Key Changes |
|------|------|--------|-------------|
| 1 | Create error enrichment module and protocol error result function with unit tests | fa1e5d2 | New error_enrichment.h/cpp, create_tool_error_result in protocol, 36+5 unit tests |
| 2 | Integrate error enrichment into MCP server dispatch layer and deferred response path | 35c21e0 | make_tool_response helper, 59 dispatch replacements, deferred error enrichment |

## What Was Built

### Error Enrichment Module (src/error_enrichment.h/cpp)
- **ErrorCategory enum**: 9 categories (NODE_NOT_FOUND, NO_SCENE_OPEN, GAME_NOT_RUNNING, UNKNOWN_CLASS, TYPE_MISMATCH, SCRIPT_ERROR, RESOURCE_ERROR, DEFERRED_PENDING, GENERIC)
- **levenshtein_distance()**: Space-optimized O(min(m,n)) edit distance for fuzzy matching
- **categorize_error()**: String prefix matching to classify error messages into categories
- **enrich_error()**: Pure C++17 enrichment dispatcher, appends diagnostic text per category
- **enrich_node_not_found()**: Fuzzy match against sibling names (distance <= 2, max 3 suggestions)
- **enrich_unknown_class()**: Fuzzy match against known class names
- **enrich_error_with_context()**: Godot-dependent overload fetching live scene tree siblings and ClassDB class names
- **TYPE_FORMAT_HINTS**: Static map with format examples for 10 common Godot types (Vector2, Vector3, Color, etc.)

### Protocol Extension (src/mcp_protocol.h/cpp)
- **create_tool_error_result()**: New MCP response builder with isError:true per MCP spec 2025-03-26

### Dispatch Layer Integration (src/mcp_server.cpp)
- **make_tool_response()**: Static helper wrapping all tool results -- checks for "error" key, enriches, and routes to create_tool_error_result
- **59 tool dispatch calls** converted from create_tool_result to make_tool_response
- **queue_deferred_response()**: Enhanced to detect errors in deferred game bridge responses and apply enrichment
- **5 create_tool_result calls preserved**: bridge-wait success (x2), run_game already-running success, restart_editor success, and make_tool_response success branch

### Enrichment Categories and Guidance
| Category | Guidance Appended |
|----------|-------------------|
| NO_SCENE_OPEN | "Use open_scene to open an existing scene or create_scene to create a new one." |
| GAME_NOT_RUNNING | "Use run_game to start the game first (mode: 'main', 'current', or 'custom')." |
| NODE_NOT_FOUND | Fuzzy match suggestions + sibling list + "Use get_scene_tree" |
| UNKNOWN_CLASS | Fuzzy match from ClassDB + "Use get_scene_tree to see existing node types" |
| TYPE_MISMATCH | "Check the parameter value matches the expected type" |
| SCRIPT_ERROR | "Use list_project_files. Script paths must start with res://" |
| RESOURCE_ERROR | "Use list_project_files to see available resources" |
| DEFERRED_PENDING | "Wait for previous request. Use get_game_bridge_status" |
| GENERIC | "Use get_scene_tree to inspect the current scene state" |

## Test Results

- **test_error_enrichment**: 36 tests PASSED (Levenshtein: 6, categorization: 15, enrichment: 8, node-not-found: 4, unknown-class: 3)
- **test_protocol**: 56 tests PASSED (51 existing + 5 new ToolErrorResult tests)
- **Total new tests**: 41

## Deviations from Plan

None - plan executed exactly as written.

## Requirements Coverage

| Req ID | Description | Status |
|--------|-------------|--------|
| ERR-01 | Tool error responses use MCP isError:true | Done - create_tool_error_result + all dispatch paths |
| ERR-02 | Node-not-found errors include fuzzy match suggestions | Done - Levenshtein + enrich_node_not_found |
| ERR-03 | No-scene and game-not-running include next-step guidance | Done - category-specific enrichment |
| ERR-05 | Precondition errors include which tool to call first | Done - each category suggests recovery tools |
| ERR-06 | Property type errors include expected format | Done - TYPE_FORMAT_HINTS map with 10 types |
| ERR-07 | Error responses include suggested recovery tools | Done - natural language tool names in all categories |

## Known Stubs

None - all functionality is fully wired.

## Self-Check: PASSED
