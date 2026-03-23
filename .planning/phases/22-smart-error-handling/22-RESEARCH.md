# Phase 22: Smart Error Handling - Research

**Researched:** 2026-03-24
**Domain:** MCP protocol error handling, C++ string fuzzy matching, Godot ClassDB introspection
**Confidence:** HIGH

## Summary

This phase transforms the MCP server's error handling from bare single-line error strings into diagnostic-rich responses that enable AI self-correction. The codebase currently has ~50+ distinct error paths across 19 source files, all returning `{{"error", "..."}}` JSON objects through `mcp::create_tool_result()` which hardcodes `isError: false`. The MCP spec (2025-03-26) explicitly distinguishes protocol errors (JSON-RPC error objects) from tool execution errors (result with `isError: true` in the content), and the current implementation incorrectly sends tool-level errors with `isError: false`.

The CONTEXT.md locks a centralized dispatch-layer enrichment architecture: an `enrich_error()` function at the `handle_request` exit point that appends diagnostic context (fuzzy suggestions, precondition guidance, type format hints) as natural language in the error text string. No individual tool functions need modification -- all enrichment happens at the dispatch layer. The fuzzy matching strategy uses Levenshtein distance (threshold 2, sibling-only scope, max 3 suggestions) implemented in pure C++ with no external dependencies.

**Primary recommendation:** Create a new `create_tool_error_result()` protocol function with `isError: true`, add an `enrich_error()` dispatcher in `mcp_server.cpp` that classifies errors by pattern (node-not-found, no-scene, type-mismatch, etc.) and appends contextual diagnostics, then route all tool error returns through it.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Enrichment is a passive wrapper at the dispatch layer (`mcp_server.cpp` `result.contains("error")` check point) -- no modifications to individual tool functions
- `suggested_tools` appended as natural language in the error text (e.g., "Use get_scene_tree to see available nodes.") -- not a separate JSON field
- Zero overhead on success path -- `enrich_error()` only invoked when `result.contains("error")` is true
- Existing 50 tools continue returning `{{"error", "..."}}` unchanged; enrichment is applied at dispatch layer
- Levenshtein distance <=2 for spelling error correction. Pure C++ implementation, no external dependencies
- Search scope: sibling nodes only (same parent as the error path) -- avoids full-tree search performance cost
- Maximum 3 suggestions per error
- Both node paths and class names get fuzzy matching. Class names sourced from ClassDB
- Target 2-4 sentences per error: what went wrong, why, and how to fix
- Property type hints for top 10 common types: Vector2, Vector3, Color, Rect2, Transform2D, NodePath, StringName, float, int, bool
- Script parse errors obtained via GDScript::reload() error capture -- reuses existing attach_script pattern
- Error messages in English -- consistent with tool descriptions, optimized for LLM comprehension

### Claude's Discretion
- Exact wording of error messages
- Internal organization of enrich_error() helper
- Whether to use a map or switch for error category routing

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| ERR-01 | Tool error responses use MCP `isError: true` flag | New `create_tool_error_result()` in mcp_protocol.cpp; route errors through it at dispatch |
| ERR-02 | Node-not-found errors include fuzzy match suggestions + parent's children list | Levenshtein C++ implementation + Godot `Node::get_child_count()`/`get_child()` for siblings |
| ERR-03 | "No scene open" and "game not running" errors include next-step guidance | Pattern matching on error string prefix; append tool-call suggestions |
| ERR-04 | Missing parameter errors include format examples and usage | Enrichment references tool schema from `mcp_tool_registry` |
| ERR-05 | Precondition errors include which tool to call first | Category routing map: "No scene open" -> "open_scene/create_scene", "Game bridge not initialized" -> "run_game" |
| ERR-06 | Property type mismatch errors include expected format and example | `variant_parser.cpp` type hints + static format string table for top 10 types |
| ERR-07 | Error responses include suggested recovery tools list | Natural language appended per error category (locked decision) |
| ERR-08 | Script parse errors include offending line number and content | GDScript::reload() error capture in attach_script flow; parse error output for line info |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| nlohmann/json | 3.12.0 | JSON handling for enriched error responses | Already in project, zero new deps |
| godot-cpp | v10+ (Godot 4.3+) | ClassDB introspection, Node tree traversal | Already in project |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<algorithm>` | C++17 stdlib | `std::min`, `std::sort` for Levenshtein + suggestion ranking | Fuzzy match implementation |
| `<vector>` | C++17 stdlib | Dynamic arrays for suggestions, sibling names | All enrichment paths |
| `<string>` | C++17 stdlib | String manipulation for error message construction | All enrichment paths |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-rolled Levenshtein | Damerau-Levenshtein | Handles transpositions but adds complexity; LD<=2 sufficient for typo correction |
| Sibling-only search | Full tree search | Better coverage but O(n) over entire tree; sibling-only is O(children_count) and covers 90%+ of cases |
| Natural language suggested_tools | Separate JSON field | Separate field is cleaner for machine parsing, but user locked natural language approach |

## Architecture Patterns

### Recommended Project Structure
```
src/
  error_enrichment.h      # New: enrich_error() declaration + error category enum
  error_enrichment.cpp     # New: enrichment logic, fuzzy match, category routing
  mcp_protocol.h           # Modified: add create_tool_error_result()
  mcp_protocol.cpp         # Modified: implement create_tool_error_result()
  mcp_server.cpp           # Modified: route error results through enrich + error_result
tests/
  test_error_enrichment.cpp # New: unit tests for Levenshtein, error categorization, enrichment
  test_protocol.cpp         # Modified: tests for create_tool_error_result()
```

### Pattern 1: Dispatch-Layer Error Interception
**What:** All tool results pass through a single checkpoint. If `result.contains("error")`, call `enrich_error()` before creating the MCP response.
**When to use:** Every tool call response in `handle_request()`.
**Example:**
```cpp
// In mcp_server.cpp handle_request(), after each tool returns result:
// Current pattern (50+ locations):
//   return mcp::create_tool_result(id, some_tool_function(...));
//
// New pattern at the END of handle_request, single checkpoint:
// The tool result JSON is checked; if it has "error", enrich and use error result.

// Conceptual flow (the actual interception point):
nlohmann::json result = some_tool_function(...);
if (result.contains("error")) {
    std::string enriched = enrich_error(result["error"].get<std::string>(), tool_name);
    return mcp::create_tool_error_result(id, enriched);
}
return mcp::create_tool_result(id, result);
```

### Pattern 2: Error Category Routing
**What:** Classify error strings by prefix/pattern into categories, each with its own enrichment strategy.
**When to use:** Inside `enrich_error()`.
**Example:**
```cpp
enum class ErrorCategory {
    NODE_NOT_FOUND,      // "Node not found:", "Parent not found:", "Source node not found:"
    NO_SCENE_OPEN,       // "No scene open"
    GAME_NOT_RUNNING,    // "Game bridge not initialized", "Game is not currently running"
    UNKNOWN_CLASS,       // "Unknown class:"
    TYPE_MISMATCH,       // (detected from variant_parser context)
    MISSING_PARAM,       // (handled at dispatch layer before tool call)
    SCRIPT_ERROR,        // "File not found:", "Script file not found:"
    GENERIC              // fallback: pass through with suggested_tools only
};

ErrorCategory categorize_error(const std::string& error_msg, const std::string& tool_name);
```

### Pattern 3: Levenshtein Fuzzy Match
**What:** Pure C++ edit distance, O(m*n) with single-row optimization.
**When to use:** NODE_NOT_FOUND and UNKNOWN_CLASS categories.
**Example:**
```cpp
// Space-optimized Levenshtein: O(min(m,n)) space
int levenshtein_distance(const std::string& a, const std::string& b) {
    if (a.size() < b.size()) return levenshtein_distance(b, a);
    std::vector<int> prev(b.size() + 1), curr(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= a.size(); ++i) {
        curr[0] = static_cast<int>(i);
        for (size_t j = 1; j <= b.size(); ++j) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            curr[j] = std::min({curr[j-1] + 1, prev[j] + 1, prev[j-1] + cost});
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}
```

### Anti-Patterns to Avoid
- **Modifying individual tool functions:** Locked decision says enrichment happens at dispatch layer only. Do NOT change the 50+ tool return statements.
- **Adding a JSON `suggested_tools` field:** User locked decision specifies natural language in error text.
- **Full tree search for fuzzy matching:** Locked to sibling-only scope for performance.
- **Regex-based error classification:** String prefix matching is simpler, faster, and sufficient for the ~15 known error prefix patterns.
- **Caching ClassDB results per request:** The enrichment only fires on errors (rare path); caching adds complexity with no measurable benefit.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Class name validation | Custom class name database | `ClassDB::get_inheriters_from_class("Node")` | Returns all 300+ Node subclasses from live engine; always current |
| Property type information | Manual type tables | `ClassDB::class_get_property_list()` | Returns property metadata including type for any class |
| Sibling node enumeration | Custom tree walker | `Node::get_child_count()` + `Node::get_child(i)` | Standard Godot API, direct access |
| Parent class checking | Manual inheritance tables | `ClassDB::is_parent_class()` | Already used in codebase |

**Key insight:** Godot's ClassDB is a comprehensive runtime reflection system. For class name fuzzy matching, `get_inheriters_from_class("Node")` gives all valid Node types. For property type hints, `class_get_property_list()` gives type metadata. Use the engine's own introspection rather than maintaining static tables.

## Common Pitfalls

### Pitfall 1: Inconsistent Error JSON Structure Across Tools
**What goes wrong:** Some tools return `{{"error", "msg"}}` while others return `{{"success", false}, {"error", "msg"}}`. The dispatch layer check `result.contains("error")` catches both, but the enrichment must extract the error string correctly from both formats.
**Why it happens:** The codebase evolved over 21 phases. Earlier tools (scene_mutation) use `{{"error", "..."}}`. Later tools (signal_tools, runtime_tools) use `{{"success", false}, {"error", "..."}}`.
**How to avoid:** The `enrich_error()` function should always extract via `result["error"].get<std::string>()` which works for both formats. The result structure itself is NOT modified -- only the error string is enriched.
**Warning signs:** Test with both error formats during development.

### Pitfall 2: Deferred Response Error Path
**What goes wrong:** Some tools (game_bridge) use deferred responses -- the error comes back through `queue_deferred_response()` and `create_tool_result()` is called inside `game_bridge.cpp`, NOT in `handle_request()`. These errors bypass the dispatch-layer enrichment.
**Why it happens:** The deferred response pattern was designed for async operations (viewport capture, game property queries). When these fail, the error is wrapped in `create_tool_result()` inside the bridge callback.
**How to avoid:** Two options: (1) enrich errors in `game_bridge.cpp` callback before calling `create_tool_result()`, or (2) add an enrichment wrapper in `queue_deferred_response()`. Option 2 is better as it maintains the "single enrichment point" principle.
**Warning signs:** Test with game bridge not connected, then verify error has enrichment.

### Pitfall 3: GDScript Reload Error Capture
**What goes wrong:** `GDScript::reload()` in the attach_script flow currently ignores errors. The method returns an `Error` enum but does NOT provide the parse error message through the godot-cpp API. Getting the actual error text (line number, error description) requires checking Godot's output.
**Why it happens:** godot-cpp wraps `reload()` as returning only `Error` enum. The detailed parse error is pushed to Godot's error handler, not returned to the caller.
**How to avoid:** After `reload()`, check the return value. If non-OK, capture the error via `UtilityFunctions::push_error()` / print buffer, or re-read the script source and do basic syntax validation (check for unterminated strings, mismatched brackets). Alternatively, use the `GDScript` class method that reports errors.
**Warning signs:** `reload()` returning `OK` even for syntactically invalid scripts (because reload may succeed partially).

### Pitfall 4: ClassDB::get_inheriters_from_class Performance
**What goes wrong:** Calling `get_inheriters_from_class("Node")` returns 300+ class names. Running Levenshtein against all of them for every "Unknown class" error adds ~microseconds of computation (negligible), but the `PackedStringArray` to `std::vector<std::string>` conversion on every error call is wasteful.
**Why it happens:** ClassDB is queried at runtime each time.
**How to avoid:** Cache the Node subclass list on first use (it never changes during a Godot session). A simple `static std::vector<std::string>` initialized on first `enrich_error()` call is sufficient.
**Warning signs:** Profiling shows repeated ClassDB queries in error-heavy scenarios.

### Pitfall 5: Error String Mutation Breaking Existing Tests
**What goes wrong:** UAT tests (uat_phase3.py through uat_phase18.py) check for specific error strings like `"Node not found: "`. If enrichment changes the error text in the content JSON, these tests break.
**Why it happens:** The enrichment adds sentences AFTER the original error message. But the error is now wrapped in `create_tool_error_result` (isError: true) instead of `create_tool_result` (isError: false).
**How to avoid:** The enrichment APPENDS to the error string (original message + newline + enrichment). Existing tests that check `"error"` key in the parsed content text should still find the original prefix. However, the `isError` field change from false to true may affect UAT test assertions if they check that field.
**Warning signs:** Run existing UAT suite after changes and look for assertion failures on `isError`.

## Code Examples

### Example 1: create_tool_error_result Implementation
```cpp
// Source: Pattern derived from existing create_tool_result in mcp_protocol.cpp:99
// New function with isError: true per MCP spec 2025-03-26
nlohmann::json create_tool_error_result(const nlohmann::json& id, const std::string& error_text) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", {
                {
                    {"type", "text"},
                    {"text", error_text}
                }
            }},
            {"isError", true}
        }}
    };
}
```

### Example 2: Error Interception at Dispatch Layer
```cpp
// Source: mcp_server.cpp handle_request(), conceptual modification
// Instead of modifying 50+ return statements, wrap at the tool_name dispatch exit:
if (tool_name == "create_node") {
    // ... extract args ...
    auto result = create_node(type, parent_path, node_name, properties, undo_redo);
    if (result.contains("error")) {
        std::string enriched = enrich_error(result["error"].get<std::string>(), tool_name);
        return mcp::create_tool_error_result(id, enriched);
    }
    return mcp::create_tool_result(id, result);
}
// NOTE: This pattern repeats for each tool. A helper macro or lambda could reduce boilerplate.
```

### Example 3: Node-Not-Found Enrichment with Fuzzy Matching
```cpp
// Source: research synthesis of error patterns and Godot API
std::string enrich_node_not_found(const std::string& error_msg, const std::string& tool_name) {
    // Extract the path from "Node not found: Player/Sprit2D"
    std::string path = error_msg.substr(16); // after "Node not found: "

    // Split path to get parent and target name
    auto slash_pos = path.rfind('/');
    std::string parent_path = (slash_pos != std::string::npos) ? path.substr(0, slash_pos) : "";
    std::string target_name = (slash_pos != std::string::npos) ? path.substr(slash_pos + 1) : path;

    // Get sibling names from parent
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    Node* parent = parent_path.empty() ? scene_root : scene_root->get_node_or_null(NodePath(parent_path.c_str()));

    std::string result = error_msg;
    if (parent) {
        // Collect siblings and find fuzzy matches
        std::vector<std::pair<int, std::string>> matches;
        for (int i = 0; i < parent->get_child_count(); i++) {
            std::string child_name = String(parent->get_child(i)->get_name()).utf8().get_data();
            int dist = levenshtein_distance(target_name, child_name);
            if (dist <= 2) {
                matches.push_back({dist, child_name});
            }
        }
        std::sort(matches.begin(), matches.end());

        if (!matches.empty()) {
            result += " Did you mean: ";
            for (size_t i = 0; i < std::min(matches.size(), size_t(3)); i++) {
                if (i > 0) result += ", ";
                result += "'" + matches[i].second + "'";
            }
            result += "?";
        }

        // List available children
        result += " Available children of '" + parent_path + "': ";
        for (int i = 0; i < std::min(parent->get_child_count(), 10); i++) {
            if (i > 0) result += ", ";
            result += String(parent->get_child(i)->get_name()).utf8().get_data();
        }
        if (parent->get_child_count() > 10) result += "... (" + std::to_string(parent->get_child_count()) + " total)";
    }
    result += " Use get_scene_tree to see the full node hierarchy.";
    return result;
}
```

### Example 4: Type Format Hint Table
```cpp
// Source: research synthesis from variant_parser.cpp type handling
static const std::unordered_map<std::string, std::string> TYPE_FORMAT_HINTS = {
    {"Vector2",     "Vector2(x, y) e.g. Vector2(100, 200)"},
    {"Vector3",     "Vector3(x, y, z) e.g. Vector3(0, 1, 0)"},
    {"Color",       "Color(r, g, b, a) with 0-1 floats, or #RRGGBB hex e.g. Color(1, 0, 0, 1) or #ff0000"},
    {"Rect2",       "Rect2(x, y, width, height) e.g. Rect2(0, 0, 100, 50)"},
    {"Transform2D", "Transform2D(rotation, position) e.g. Transform2D(0, Vector2(100, 200))"},
    {"NodePath",    "Relative path from scene root e.g. Player/Sprite2D"},
    {"StringName",  "Simple string e.g. idle"},
    {"float",       "Decimal number e.g. 3.14"},
    {"int",         "Integer number e.g. 42"},
    {"bool",        "true or false"},
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `isError` always false | `isError: true` for tool execution errors | MCP spec 2025-03-26 | AI clients can distinguish errors from empty results |
| Bare error strings | Diagnostic-rich error responses | Industry trend 2025 | AI can self-correct without human intervention |
| Protocol-level errors for tool failures | Application-level `isError` in result | MCP spec distinction | Protocol errors = infrastructure; `isError` = tool logic |

**Deprecated/outdated:**
- Using JSON-RPC `error` objects for tool execution failures (these are for protocol errors only per MCP spec)

## Open Questions

1. **GDScript reload() error text capture**
   - What we know: `GDScript::reload()` returns `Error` enum. The parse error text goes to Godot's error log, not to the caller. The existing `attach_script` code calls `reload()` and ignores the return value.
   - What's unclear: Whether godot-cpp provides any way to capture the specific parse error message (line number, description). The C++ engine code uses `GDScriptParser` which stores errors, but this class may not be exposed via godot-cpp.
   - Recommendation: Try checking `reload()` return value first. If it returns non-OK, re-read the script source and at minimum report "Script has parse errors". For line-specific info, experiment with `GDScript::get_script_method_list()` or explore if `_get_script_error()` exists. If unavailable, do a basic bracket/indent check in C++ as a fallback to identify the problematic line region. Mark as LOW confidence until tested.

2. **Deferred response enrichment path**
   - What we know: 7 tool paths use deferred responses through `game_bridge.cpp`. Errors from these paths bypass the dispatch-layer `handle_request()` check.
   - What's unclear: Whether enrichment should happen in `game_bridge.cpp` or in `queue_deferred_response()`.
   - Recommendation: Add enrichment in `queue_deferred_response()` by parsing the tool result JSON before queuing. This maintains the single-point principle. The `game_bridge` errors are simpler (mostly "Game bridge not initialized", "Another deferred request pending") and need only precondition guidance, not fuzzy matching.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 (unit) + Python TCP UAT (integration) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd tests/build && cmake --build . --config Debug && ctest -C Debug --output-on-failure` |
| Full suite command | `cd tests/build && cmake --build . --config Debug && ctest -C Debug --output-on-failure && python tests/uat_phase22.py` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ERR-01 | `isError: true` in error responses | unit | `ctest -R test_protocol -C Debug --output-on-failure` | Needs update |
| ERR-02 | Fuzzy match suggestions for node not found | unit + UAT | `ctest -R test_error_enrichment -C Debug --output-on-failure` | Wave 0 |
| ERR-03 | Next-step guidance for no-scene/no-game | unit | `ctest -R test_error_enrichment -C Debug --output-on-failure` | Wave 0 |
| ERR-04 | Parameter format examples | unit | `ctest -R test_error_enrichment -C Debug --output-on-failure` | Wave 0 |
| ERR-05 | Precondition tool suggestions | unit | `ctest -R test_error_enrichment -C Debug --output-on-failure` | Wave 0 |
| ERR-06 | Type format hints | unit | `ctest -R test_error_enrichment -C Debug --output-on-failure` | Wave 0 |
| ERR-07 | Suggested tools in error text | unit + UAT | `ctest -R test_error_enrichment -C Debug --output-on-failure` | Wave 0 |
| ERR-08 | Script parse error line info | UAT | `python tests/uat_phase22.py` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && cmake --build . --config Debug && ctest -C Debug --output-on-failure`
- **Per wave merge:** Full unit suite + UAT
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_error_enrichment.cpp` -- covers ERR-02 through ERR-07 (pure C++ tests for Levenshtein, categorization, enrichment text)
- [ ] `tests/test_protocol.cpp` update -- covers ERR-01 (new `create_tool_error_result` test)
- [ ] `tests/CMakeLists.txt` update -- add `test_error_enrichment` executable
- [ ] `tests/uat_phase22.py` -- covers ERR-01 through ERR-08 (live integration tests via TCP)

## Error Catalog (Codebase Audit)

Comprehensive catalog of all error patterns found across 19 source files, classified by enrichment category:

### Category: NODE_NOT_FOUND
Prefix patterns: `"Node not found: "`, `"Parent not found: "`, `"Source node not found: "`, `"Target node not found: "`, `"Node not found at path: "`
- Files: scene_mutation.cpp (x3), scene_tools.cpp (x1), script_tools.cpp (x3), signal_tools.cpp (x4), ui_tools.cpp (x2), animation_tools.cpp (x3), scene_file_tools.cpp (x1), tilemap_tools.cpp (x1)
- Enrichment: Fuzzy match + sibling list + `suggested_tools: get_scene_tree`
- Count: ~18 occurrences

### Category: NO_SCENE_OPEN
Exact match: `"No scene open"`
- Files: scene_mutation.cpp (x3), animation_tools.cpp (x1), ui_tools.cpp (x2), scene_file_tools.cpp (x1), tilemap_tools.cpp (x1), physics_tools.cpp (x1), script_tools.cpp (x2)
- Enrichment: "No scene is currently open in the editor. Use open_scene to open an existing scene or create_scene to create a new one."
- Count: ~11 occurrences

### Category: GAME_NOT_RUNNING
Patterns: `"Game bridge not initialized"`, `"No game running or bridge not connected"`, `"Game is not currently running"`
- Files: mcp_server.cpp (x9), game_bridge.cpp (x8), runtime_tools.cpp (x1)
- Enrichment: "The game is not running. Use run_game with mode 'main', 'current', or 'custom' to start the game first."
- Count: ~18 occurrences

### Category: UNKNOWN_CLASS
Patterns: `"Unknown class: "`, `" is not a Node type"`
- Files: scene_mutation.cpp (x2), scene_file_tools.cpp (x2)
- Enrichment: Fuzzy match against ClassDB Node subclasses + `suggested_tools: get_scene_tree (to see existing node types)`
- Count: ~4 occurrences

### Category: MISSING_PARAMETER
Handled at dispatch layer via `mcp::create_error_response()` (JSON-RPC protocol error)
- Files: mcp_server.cpp (x20+)
- Note: These are JSON-RPC errors, NOT tool results. They use `INVALID_PARAMS` error code. Per MCP spec, these are protocol-level errors. Consider whether to also enrich these, or focus only on tool-level errors.
- Recommendation: ALSO enrich these with format examples since they are the most common AI errors.

### Category: TYPE_MISMATCH
Patterns: Various (no consistent prefix), detected contextually
- Files: ui_tools.cpp (`"Unknown preset: "`, `"Unknown alignment: "`, `"Unknown size flag: "`), animation_tools.cpp (`"Unknown track type: "`, `"Unknown loop_mode: "`, `"Unknown action: "`), game_bridge.cpp (`"Unknown input type: "`, `"Unknown mouse_action: "`)
- Enrichment: Include valid values (many already do: "Valid: none, linear, pingpong")
- Count: ~10 occurrences

### Category: SCRIPT_ERROR
Patterns: `"File not found: "`, `"Script file not found: "`, `"Path must start with res://"`, `"File already exists: "`
- Files: script_tools.cpp (x8)
- Enrichment: Format guidance, path resolution help
- Count: ~8 occurrences

### Category: RESOURCE_ERROR
Patterns: `"Resource not found: "`, `"Failed to load resource: "`
- Files: project_tools.cpp (x3)
- Enrichment: "Use list_project_files to see available resources."
- Count: ~3 occurrences

### Category: DEFERRED_PENDING
Pattern: `"Another deferred request is already pending"`
- Files: game_bridge.cpp (x7)
- Enrichment: "Wait for the previous request to complete before sending another."
- Count: ~7 occurrences

### Category: GENERIC
All other errors not matching above patterns.
- Enrichment: Append `suggested_tools` only based on tool_name context.
- Count: ~10 miscellaneous errors

**Total error paths cataloged: ~89**

## Project Constraints (from CLAUDE.md)

- C++17, godot-cpp v10+ -- all new code must comply
- nlohmann/json for JSON handling -- no alternative JSON libraries
- GDScript follows Godot conventions -- error messages reference GDScript conventions
- MCP tools preferred for scene interaction -- error guidance should reference MCP tool names
- SCons build system for GDExtension, CMake for unit tests

## Sources

### Primary (HIGH confidence)
- [MCP Spec 2025-03-26 Tools](https://modelcontextprotocol.io/specification/2025-03-26/server/tools) - Official isError specification, error handling distinction
- godot-cpp `class_db_singleton.hpp` (local) - Confirmed `get_class_list()`, `get_inheriters_from_class()`, `class_get_property_list()`, `class_exists()`, `is_parent_class()` all available
- Codebase audit of 19 source files - Complete error catalog with 89+ error paths classified

### Secondary (MEDIUM confidence)
- [Levenshtein distance algorithm implementations](https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance) - Standard algorithm, well-documented
- [Godot ClassDB API docs](https://docs.godotengine.org/en/4.3/classes/class_classdb.html) - ClassDB method reference

### Tertiary (LOW confidence)
- GDScript::reload() error capture mechanism - Need runtime testing to confirm what error info is accessible via godot-cpp

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - No new dependencies, all existing project libraries
- Architecture: HIGH - Dispatch-layer pattern validated against 89+ error paths, single interception point confirmed at mcp_server.cpp handle_request
- Error categorization: HIGH - Complete audit of all 19 source files with error returns
- Fuzzy matching: HIGH - Standard Levenshtein algorithm, ClassDB APIs confirmed available
- Script parse error (ERR-08): LOW - GDScript::reload() error text capture mechanism unverified via godot-cpp
- Deferred response path: MEDIUM - Identified the gap, solution proposed but not validated

**Research date:** 2026-03-24
**Valid until:** 2026-04-24 (stable domain, no fast-moving dependencies)
