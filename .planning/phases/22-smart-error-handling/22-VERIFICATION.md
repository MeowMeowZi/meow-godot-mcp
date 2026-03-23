---
phase: 22-smart-error-handling
verified: 2026-03-24T00:00:00Z
status: gaps_found
score: 6/8 must-haves verified
gaps:
  - truth: "GDExtension compiles without errors"
    status: failed
    reason: "error_enrichment.cpp uses wrong include path for class_db.hpp"
    artifacts:
      - path: "src/error_enrichment.cpp"
        issue: "Line 474 includes <godot_cpp/classes/class_db.hpp> but this header does not exist. Correct path used by all other src files is <godot_cpp/core/class_db.hpp>."
    missing:
      - "Fix include in src/error_enrichment.cpp line 474: change `#include <godot_cpp/classes/class_db.hpp>` to `#include <godot_cpp/core/class_db.hpp>`"
human_verification:
  - test: "Confirm enriched error messages surface correctly in Godot editor via MCP client"
    expected: "AI receives isError:true in MCP result and sees diagnostic text with tool suggestions when a tool call fails"
    why_human: "Requires a running Godot editor connected to an MCP client to observe live error responses"
  - test: "Confirm NODE_NOT_FOUND fuzzy match returns live sibling names from scene tree"
    expected: "enrich_error_with_context fetches actual child names from EditorInterface::get_edited_scene_root() and populates Did-you-mean suggestions"
    why_human: "Requires a live Godot scene open in the editor; cannot be verified with unit tests alone"
---

# Phase 22: Smart Error Handling Verification Report

**Phase Goal:** AI can self-correct from tool errors using diagnostic information, suggestions, and recovery guidance baked into every error response
**Verified:** 2026-03-24
**Status:** gaps_found (1 compilation blocker)
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                 | Status      | Evidence                                                                               |
|----|-----------------------------------------------------------------------|-------------|----------------------------------------------------------------------------------------|
| 1  | Every tool error response has isError: true in the MCP result JSON   | VERIFIED    | `create_tool_error_result()` in mcp_protocol.cpp line 126 sets `{"isError", true}`; all 60 tool dispatch sites route through `make_tool_response` helper which calls it on error |
| 2  | Node-not-found errors include fuzzy-matched similar names and sibling list | VERIFIED | `enrich_node_not_found()` in error_enrichment.cpp lines 182-221 runs Levenshtein (distance ≤ 2, top 3) and lists up to 10 siblings |
| 3  | No-scene-open errors tell AI to use open_scene or create_scene        | VERIFIED    | `enrich_no_scene_open()` line 148: "Use open_scene to open an existing scene or create_scene to create a new one." |
| 4  | Game-not-running errors tell AI to use run_game                       | VERIFIED    | `enrich_game_not_running()` line 152: "Use run_game to start the game first." |
| 5  | Missing parameter errors include format examples showing correct usage | VERIFIED   | `enrich_missing_params()` backed by TOOL_PARAM_HINTS map (40 tool entries); 38 INVALID_PARAMS sites use `make_params_error` helper |
| 6  | Script parse errors include offending line number and line content     | VERIFIED    | `check_gdscript_syntax()` returns ScriptErrorInfo with line_number and line_content; integrated into both write_script and attach_script in script_tools.cpp |
| 7  | Property type errors include expected format examples                  | VERIFIED    | TYPE_FORMAT_HINTS static map (10 types: Vector2, Vector3, Color, Rect2, Transform2D, NodePath, StringName, float, int, bool) in error_enrichment.cpp lines 91-102 |
| 8  | GDExtension compiles without errors                                    | FAILED      | `scons target=template_debug` fails: `error_enrichment.cpp(474): fatal error C1083: cannot open include file 'godot_cpp/classes/class_db.hpp': No such file or directory`. All other src files use `godot_cpp/core/class_db.hpp`. |

**Score:** 7/8 truths verified (1 compilation blocker)

### Required Artifacts

#### Plan 01 Artifacts

| Artifact                          | Expected                                                          | Status    | Details                                              |
|-----------------------------------|-------------------------------------------------------------------|-----------|------------------------------------------------------|
| `src/error_enrichment.h`          | enrich_error(), ErrorCategory enum, levenshtein_distance()        | VERIFIED  | All declared: enum class ErrorCategory (9 values), levenshtein_distance, categorize_error, enrich_error, enrich_node_not_found, enrich_unknown_class, enrich_missing_params, ScriptErrorInfo struct, check_gdscript_syntax, enrich_error_with_context (gated) |
| `src/error_enrichment.cpp`        | Error categorization, fuzzy matching, enrichment logic            | VERIFIED  | 536 lines (min_lines: 150 satisfied); all 9 categories implemented; TYPE_FORMAT_HINTS, TOOL_PARAM_HINTS maps present |
| `src/mcp_protocol.h`              | create_tool_error_result() declaration                            | VERIFIED  | Line 46: `nlohmann::json create_tool_error_result(const nlohmann::json& id, const std::string& error_text);` |
| `src/mcp_protocol.cpp`            | create_tool_error_result() with isError: true                     | VERIFIED  | Line 126: `{"isError", true}` inside create_tool_error_result |
| `src/mcp_server.cpp`              | make_tool_response helper, 30+ dispatch replacements              | VERIFIED  | 60 occurrences of `make_tool_response`; helper defined at lines 80-89; deferred response enrichment at lines 125-147 |
| `tests/test_error_enrichment.cpp` | Unit tests: Levenshtein, categorization, enrichment categories    | VERIFIED  | 55 TEST() definitions (min: 15 satisfied); suites: LevenshteinDistance(6), CategorizeError(15), EnrichError(8), EnrichNodeNotFound(4), EnrichUnknownClass(3), MissingParams(9), ScriptSyntax(10) |

#### Plan 02 Artifacts

| Artifact                          | Expected                                                     | Status   | Details                                                      |
|-----------------------------------|--------------------------------------------------------------|----------|--------------------------------------------------------------|
| `src/error_enrichment.cpp`        | enrich_missing_params() and TOOL_PARAM_HINTS (20+ entries)   | VERIFIED | 40 entries in TOOL_PARAM_HINTS; enrich_missing_params at line 326 |
| `src/mcp_server.cpp`              | INVALID_PARAMS enrichment via make_params_error              | VERIFIED | make_params_error at line 70; 38 occurrences of `make_params_error(id,` |
| `src/script_tools.cpp`            | Script parse error detection with line number                | VERIFIED | check_gdscript_syntax called at line 209 (write_script) and 326 (attach_script); warning field included |
| `tests/test_error_enrichment.cpp` | Tests for MissingParams and ScriptSyntax                     | VERIFIED | 9 MissingParams tests + 10 ScriptSyntax tests present        |

### Key Link Verification

| From                  | To                          | Via                                  | Status      | Details                                                     |
|-----------------------|-----------------------------|--------------------------------------|-------------|-------------------------------------------------------------|
| `src/mcp_server.cpp`  | `src/error_enrichment.h`    | `#include` + `enrich_error_with_context()` | VERIFIED | Line 4: `#include "error_enrichment.h"`; line 85: `enrich_error_with_context` called in make_tool_response |
| `src/mcp_server.cpp`  | `src/mcp_protocol.h`        | `create_tool_error_result()` calls   | VERIFIED    | Line 86: `mcp::create_tool_error_result(id, enriched)`      |
| `src/mcp_server.cpp`  | `make_tool_response` helper | All tool result returns route through helper | VERIFIED | 60 occurrences; old `create_tool_result(id, create_node(` pattern absent |
| `src/mcp_server.cpp`  | `src/error_enrichment.h`    | `enrich_missing_params()` via make_params_error | VERIFIED | Line 73: `enrich_missing_params(message, tool_name)` |
| `src/script_tools.cpp` | `check_gdscript_syntax`    | write_script and attach_script flows | VERIFIED    | Line 209 (write_script), line 326 (attach_script)           |
| `src/error_enrichment.cpp` | `godot_cpp/classes/class_db.hpp` | `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED` section | BROKEN | Header path is wrong: `classes/class_db.hpp` does not exist; should be `core/class_db.hpp` |

### Data-Flow Trace (Level 4)

Not applicable — this phase produces C++ library code (error enrichment engine), not components that render dynamic data. The data flow is: tool call → error result → make_tool_response → enrich_error_with_context → enriched text in MCP isError response.

### Behavioral Spot-Checks

| Behavior                       | Command                                                                        | Result                                | Status  |
|--------------------------------|--------------------------------------------------------------------------------|---------------------------------------|---------|
| All 231 unit tests pass        | `ctest -C Debug --output-on-failure` (in tests/build)                          | 100% tests passed, 0 failed out of 231 | PASS   |
| GDExtension compilation        | `scons target=template_debug`                                                  | C1083: cannot open `godot_cpp/classes/class_db.hpp` | FAIL |
| make_tool_response present     | `grep -c "make_tool_response" src/mcp_server.cpp`                              | 60                                    | PASS    |
| make_params_error present      | `grep -c "make_params_error(id," src/mcp_server.cpp`                           | 38                                    | PASS    |
| Old dispatch patterns gone     | `grep "create_tool_result(id, create_node("` in mcp_server.cpp                 | No output (pattern absent)            | PASS    |
| check_gdscript_syntax wired    | `grep -n "check_gdscript_syntax" src/script_tools.cpp`                         | Lines 209, 326                        | PASS    |

### Requirements Coverage

| Requirement | Source Plan | Description (from REQUIREMENTS.md)                                                        | Status      | Evidence                                                                              |
|-------------|-------------|-------------------------------------------------------------------------------------------|-------------|---------------------------------------------------------------------------------------|
| ERR-01      | 22-01       | Tool error responses use MCP `isError: true` flag (not plain text result)                 | SATISFIED   | create_tool_error_result with isError:true; all 60 dispatch sites use make_tool_response |
| ERR-02      | 22-01       | Node-not-found errors include fuzzy match suggestions (similar node names + sibling list) | SATISFIED   | enrich_node_not_found with Levenshtein ≤2, top-3 suggestions, 10 sibling names; 4 unit tests pass |
| ERR-03      | 22-01       | No-scene-open and game-not-running errors include concrete next-step guidance             | SATISFIED   | enrich_no_scene_open suggests open_scene/create_scene; enrich_game_not_running suggests run_game |
| ERR-04      | 22-02       | Missing required parameter errors include parameter format examples and usage description  | SATISFIED   | TOOL_PARAM_HINTS with 40 tools; make_params_error at 38 INVALID_PARAMS sites          |
| ERR-05      | 22-01       | Precondition failures include which tool to call first                                    | SATISFIED   | Every enrichment category includes specific recovery tool names in natural language   |
| ERR-06      | 22-01       | Property type mismatch errors include expected format and examples (e.g. Vector2(100,200)) | SATISFIED  | TYPE_FORMAT_HINTS map with 10 types: Vector2, Vector3, Color, Rect2, Transform2D, NodePath, StringName, float, int, bool |
| ERR-07      | 22-01       | Error responses include suggested recovery tools list                                     | SATISFIED   | All 9 enrichment categories output natural-language tool name suggestions             |
| ERR-08      | 22-02       | Script parse errors include specific line number and line content                         | SATISFIED   | ScriptErrorInfo struct + check_gdscript_syntax; wired into write_script (line 209) and attach_script (line 326) |

All 8 ERR requirements have implementation evidence. No orphaned requirements found.

### Anti-Patterns Found

| File                        | Line | Pattern                                                        | Severity | Impact                                                                      |
|-----------------------------|------|----------------------------------------------------------------|----------|-----------------------------------------------------------------------------|
| `src/error_enrichment.cpp`  | 474  | Wrong include: `godot_cpp/classes/class_db.hpp` (does not exist) | BLOCKER  | GDExtension fails to compile; `enrich_error_with_context()` for UNKNOWN_CLASS (ClassDB fuzzy matching) is unavailable at runtime — falls back is no-op since class list will never be populated |

No TODO/FIXME/PLACEHOLDER comments. No empty return stubs. No hardcoded-empty props flowing to render paths.

### Human Verification Required

#### 1. End-to-End Error Enrichment in Live Editor

**Test:** Open Godot editor with the plugin loaded, connect an MCP client (e.g., Claude), and call `create_node` on a non-existent parent path (e.g., `"FakePath/NotReal"`).
**Expected:** MCP response has `result.isError: true` and content text contains "Node not found" with fuzzy match suggestions and sibling list from the live scene tree.
**Why human:** Requires running Godot editor + MCP TCP connection; cannot be scripted in unit tests.

#### 2. NodeNotFound Fuzzy Match Uses Live Scene Tree

**Test:** Open a scene with a node named "Sprite2D", attempt `set_node_property` with node_path "Sprit2D" (typo).
**Expected:** Error message contains "Did you mean: 'Sprite2D'?" populated from EditorInterface::get_edited_scene_root().
**Why human:** enrich_error_with_context Godot-dependent path requires live editor scene.

### Gaps Summary

One gap blocks the phase goal being achievable in a deployable state:

**Compilation failure (BLOCKER):** `src/error_enrichment.cpp` line 474 includes `<godot_cpp/classes/class_db.hpp>` which does not exist in this project's godot-cpp installation. The correct header, used consistently by all other source files (`animation_tools.cpp`, `mcp_plugin.h`, `physics_tools.cpp`), is `<godot_cpp/core/class_db.hpp>`. This causes `scons target=template_debug` to terminate with a fatal C1083 error.

The fix is a single-line change. All unit tests (231/231) pass because the Godot-dependent section is behind `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED` which is not defined in the unit test build. The logic of the enrichment engine is fully correct and verified by tests — only the GDExtension build is broken.

Note: The REQUIREMENTS.md status table still shows all ERR requirements as "Pending" (not "Done") — this reflects the state of REQUIREMENTS.md before the phase ran, not a gap in implementation.

---

_Verified: 2026-03-24_
_Verifier: Claude (gsd-verifier)_
