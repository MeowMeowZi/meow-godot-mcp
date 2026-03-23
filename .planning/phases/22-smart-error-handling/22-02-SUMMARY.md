---
phase: 22-smart-error-handling
plan: 02
subsystem: error-handling
tags: [parameter-hints, script-syntax, error-enrichment, INVALID_PARAMS, gdscript]
dependency_graph:
  requires: [error_enrichment_module, create_tool_error_result, make_tool_response]
  provides: [enrich_missing_params, check_gdscript_syntax, make_params_error, TOOL_PARAM_HINTS]
  affects: [mcp_server, script_tools, error_enrichment]
tech_stack:
  added: []
  patterns: [parameter-hint-lookup, bracket-nesting-tracker, syntax-pre-check]
key_files:
  created: []
  modified:
    - src/error_enrichment.h
    - src/error_enrichment.cpp
    - src/mcp_server.cpp
    - src/script_tools.cpp
    - tests/test_error_enrichment.cpp
decisions:
  - "TOOL_PARAM_HINTS static map with 40 tool entries for comprehensive parameter format examples"
  - "make_params_error helper replaces 37 create_error_response INVALID_PARAMS calls in tools/call block"
  - "check_gdscript_syntax is pure C++ bracket/string nesting tracker, testable without Godot"
  - "Script syntax warnings are non-blocking: attach_script still succeeds but includes warning field"
  - "3 INVALID_PARAMS sites intentionally preserved: make_params_error definition, resources/read, pre-tool-name check"
metrics:
  duration: "11 min"
  completed: "2026-03-24"
  tasks: 2
  files: 5
  tests_added: 19
---

# Phase 22 Plan 02: Parameter Format Hints and Script Parse Error Detection Summary

TOOL_PARAM_HINTS map with 40 tool parameter format examples enriching all INVALID_PARAMS errors, plus pure C++ GDScript syntax checker detecting mismatched brackets, unterminated strings, and unclosed delimiters with line-level precision.

## Completed Tasks

| Task | Name | Commit | Key Changes |
|------|------|--------|-------------|
| 1 | Add parameter format enrichment to INVALID_PARAMS errors (TDD) | 92a91f6 | enrich_missing_params + TOOL_PARAM_HINTS (40 entries), make_params_error helper, 37 dispatch replacements, 9 unit tests |
| 2 | Add script parse error line capture and final build verification | 53c83bb | ScriptErrorInfo struct, check_gdscript_syntax, write_script/attach_script warnings, 10 ScriptSyntax tests |

## What Was Built

### Parameter Format Enrichment (ERR-04)

- **TOOL_PARAM_HINTS map**: 40 entries covering every tool that accepts parameters, with parameter names, types, and concrete example values
- **enrich_missing_params()**: Appends tool-specific parameter format hints to INVALID_PARAMS error messages; unknown tools get the original message unchanged
- **make_params_error()**: Static helper in mcp_server.cpp that wraps `enrich_missing_params` + `create_error_response` for clean dispatch integration
- **37 INVALID_PARAMS sites converted**: Every `create_error_response(id, mcp::INVALID_PARAMS, "Missing required param...")` call in the tools/call block now uses `make_params_error`

Example enriched error:
```
Missing required parameter: type Parameters: type (string, e.g. 'Sprite2D', 'CharacterBody2D'), parent_path (string, path to parent node, e.g. 'Player'), name (string, optional node name)
```

### GDScript Syntax Checker (ERR-08)

- **ScriptErrorInfo struct**: `has_error`, `line_number` (1-based), `line_content`, `error_description`
- **check_gdscript_syntax()**: Pure C++ function that detects:
  - Unmatched closing parenthesis/bracket/brace (reports exact line)
  - Unclosed parenthesis/bracket/brace at end of file (reports opening line)
  - Unterminated string literals (both `"` and `'` delimiters)
  - Unterminated multiline strings (triple-quote `"""`)
  - Correctly ignores brackets/parens inside strings and comments
  - Handles escape sequences (`\"`) in string literals
- **write_script integration**: After writing file, runs syntax check; adds `warning` field to JSON response if errors found
- **attach_script integration**: After `script->reload()`, if Error != OK, runs syntax check and adds warning with line info; script is still attached (non-blocking)
- **SCRIPT_ERROR enrichment enhanced**: Parse error and syntax error patterns now suggest read_script + edit_script instead of list_project_files

### Preserved INVALID_PARAMS Sites (intentional)

| Line | Location | Reason |
|------|----------|--------|
| 74 | make_params_error body | The function definition itself |
| 400 | resources/read | Not a tool call, no tool_name available |
| 437 | tools/call pre-check | Before tool_name is extracted from params |

## Test Results

- **test_error_enrichment**: 55 tests PASSED
  - 6 LevenshteinDistance (unchanged)
  - 15 CategorizeError (unchanged)
  - 8 EnrichError (unchanged)
  - 4 EnrichNodeNotFound (unchanged)
  - 3 EnrichUnknownClass (unchanged)
  - 9 MissingParams (NEW)
  - 10 ScriptSyntax (NEW)
- **All 8 test suites**: 231 tests PASSED
- **19 new tests total** across both tasks

## Deviations from Plan

None - plan executed exactly as written.

## Requirements Coverage

| Req ID | Description | Status |
|--------|-------------|--------|
| ERR-04 | Missing parameter errors include format examples and usage | Done - TOOL_PARAM_HINTS with 40 tools, all 37 INVALID_PARAMS sites enriched |
| ERR-08 | Script parse errors include line number and content | Done - check_gdscript_syntax in write_script and attach_script with warning field |

## Known Stubs

None - all functionality is fully wired.

## Self-Check: PASSED
