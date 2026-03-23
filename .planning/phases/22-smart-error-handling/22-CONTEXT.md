# Phase 22: Smart Error Handling - Context

**Gathered:** 2026-03-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Transform all MCP tool error responses from dead-end messages into diagnostic-rich responses that enable AI self-correction. Every error carries `isError: true`, includes fuzzy match suggestions for node/class name errors, prerequisite guidance for state errors, and format examples for type errors.

</domain>

<decisions>
## Implementation Decisions

### Error Response Architecture
- Enrichment is a passive wrapper at the dispatch layer (`mcp_server.cpp` `result.contains("error")` check point) — no modifications to individual tool functions
- `suggested_tools` appended as natural language in the error text (e.g., "Use get_scene_tree to see available nodes.") — not a separate JSON field
- Zero overhead on success path — `enrich_error()` only invoked when `result.contains("error")` is true
- Existing 50 tools continue returning `{{"error", "..."}}` unchanged; enrichment is applied at dispatch layer

### Fuzzy Matching Strategy
- Levenshtein distance ≤2 for spelling error correction (e.g., Sprit2D → Sprite2D). Pure C++ implementation, no external dependencies
- Search scope: sibling nodes only (same parent as the error path) — avoids full-tree search performance cost
- Maximum 3 suggestions per error — sufficient for AI correction without wasting context tokens
- Both node paths and class names get fuzzy matching. Class names sourced from ClassDB

### Error Message Detail Level
- Target 2-4 sentences per error: what went wrong, why, and how to fix
- Property type hints for top 10 common types: Vector2, Vector3, Color, Rect2, Transform2D, NodePath, StringName, float, int, bool
- Script parse errors obtained via GDScript::reload() error capture — reuses existing attach_script pattern
- Error messages in English — consistent with tool descriptions, optimized for LLM comprehension

### Claude's Discretion
- Exact wording of error messages
- Internal organization of enrich_error() helper
- Whether to use a map or switch for error category routing

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `mcp_protocol.cpp:99` — `create_tool_result()` always sets `isError: false`; needs new `create_tool_error_result()` or modification
- `mcp_server.cpp:818` — `result.contains("error")` check is the natural interception point
- `variant_parser.cpp` — type parsing logic, can provide expected format strings for type error hints

### Established Patterns
- Tools return `nlohmann::json` with `{{"error", "message"}}` for errors, `{{"success", true}, ...}` for success
- Error strings are bare: "Node not found: X", "No scene open", "Game bridge not initialized", "Parent not found: X"
- 19 source files have error returns; ~50+ distinct error paths across all tools

### Integration Points
- `mcp_server.cpp` dispatch function (`handle_request`) — single point where tool results become MCP responses
- `mcp_protocol.h/cpp` — needs new `create_tool_error_result()` with `isError: true`
- `scene_mutation.cpp/h` — node lookup helper, natural place for fuzzy match utility

</code_context>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches based on research findings.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>
