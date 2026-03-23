# Stack Research: v1.5 AI Workflow Enhancement

**Project:** Godot MCP Meow
**Researched:** 2026-03-23
**Confidence:** HIGH
**Scope:** Stack additions for composite tools, enriched MCP Resources, smart error handling, and expanded prompt templates. Existing stack (C++17, godot-cpp v10+, nlohmann/json 3.12.0, SCons, stdio bridge + TCP relay, GoogleTest) is validated and unchanged.

## Executive Summary

v1.5 requires **zero new external libraries**. All four feature areas are implementable by extending existing C++ code patterns with the same dependencies already in use. The critical insight is that these features are *architectural enhancements* to the MCP protocol layer and tool dispatch system, not new Godot API integrations.

- **Composite tools** reuse existing atomic tool functions (create_node, set_node_property, etc.) composed into higher-level orchestration functions.
- **Enriched Resources** extend the existing `resources/list` and `resources/read` handlers with new URI schemes and `resources/templates/list` support, using simple string matching -- no RFC 6570 library needed.
- **Smart error handling** wraps existing tool return values with structured diagnostic JSON, modifying `create_tool_result` and tool functions to include `isError: true` + diagnostic context.
- **Expanded prompts** add more entries to the existing `PromptDef` vector in `mcp_prompts.cpp` using the same pattern.

The only protocol-layer change is declaring `resources` capability in `create_initialize_response()` and handling `resources/templates/list`.

---

## Recommended Stack

### Core Technologies (Unchanged)

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| C++17 | -- | Language standard | Unchanged, already in use |
| godot-cpp | v10+ (Godot 4.3+) | GDExtension bindings | Unchanged, already in use |
| nlohmann/json | 3.12.0 | JSON handling | Unchanged, already latest (released 2025-04-11) |
| SCons | -- | Build system | Unchanged |
| GoogleTest | -- | Unit testing | Unchanged |

### New godot-cpp Headers Required

No new godot-cpp headers are needed for v1.5. All features build on top of classes already included by existing tool modules. The composite tools call existing functions from `scene_mutation.h`, `script_tools.h`, `ui_tools.h`, `physics_tools.h`, etc.

### Supporting Libraries

**None new.** This is explicitly a strength -- v1.5 maintains the zero-external-dependency differentiator.

| Library Considered | Why NOT Added | Alternative |
|-------------------|---------------|-------------|
| URI template library (RFC 6570) | Our `godot://` URI scheme uses trivial `{variable}` patterns. A full RFC 6570 implementation would add ~1000 LOC dependency for features we don't need (expression operators, reserved expansion). | Hand-rolled string matching: `godot://node/{node_path}` parsed via `std::string::find()` + `substr()`. ~30 LOC. |
| Error catalog framework | Over-engineering. We have ~50 tools with predictable failure modes. | Static error catalog as `std::unordered_map<std::string, ErrorInfo>` keyed by error code. ~200 LOC. |
| Template engine (for prompts) | Prompts are simple text with `{argument}` substitution. | Existing lambda-based `PromptDef::generate` pattern already handles this. |

---

## Detailed Technical Decisions Per Feature Area

### 1. Composite Tools

**Decision: Internal composition via C++ function calls, NOT a new dispatch layer.**

Composite tools like `create_character` or `create_ui_panel` will be implemented as regular C++ functions that internally call the same underlying functions used by atomic tools (e.g., `create_node()`, `set_node_property()`, `attach_script()`, `create_collision_shape()`).

**Why internal composition (not tool-calling-tool):**
- No JSON serialization/deserialization overhead between steps
- Atomic UndoRedo -- wrap the entire composite operation in a single `undo_redo->create_action()` so Ctrl+Z undoes the whole thing
- Error handling at each step with short-circuit on failure
- Same thread safety guarantees as existing tools (all run on main thread)

**Implementation pattern:**
```cpp
// New file: src/composite_tools.h / .cpp
nlohmann::json create_character_body(
    const std::string& type,           // "2d" or "3d"
    const std::string& parent_path,
    const std::string& name,
    const nlohmann::json& options,     // sprite, collision_shape, script, etc.
    EditorUndoRedoManager* undo_redo
);
```

Each composite tool:
1. Opens a single UndoRedo action
2. Calls existing functions sequentially
3. Returns a result JSON with `created_nodes[]` array listing all created paths
4. On any step failure, returns error with `step_failed` and `partial_results` for AI recovery

**Registration:** Standard `ToolDef` entries in `mcp_tool_registry.cpp` with `min_version: {4, 3, 0}`. Dispatch in `mcp_server.cpp::handle_request()` same as all other tools.

**Tool count impact:** ~5-8 new composite tools. Total will grow from 50 to ~55-58.

### 2. Enriched MCP Resources

**Decision: Extend existing inline `resources/list` and `resources/read` handlers. Add `resources/templates/list` method handler. Declare `resources` capability in initialize response.**

**Current state (v1.4):**
- 2 static resources: `godot://scene_tree`, `godot://project_files`
- Resources capability NOT declared in initialize response (resources work but are not advertised)
- No resource templates, no subscriptions

**v1.5 additions:**

| Resource URI | Type | Content | Purpose |
|-------------|------|---------|---------|
| `godot://scene_tree` | Static | Scene tree JSON (existing) | Unchanged |
| `godot://project_files` | Static | File listing (existing) | Unchanged |
| `godot://scene_scripts` | Static | All scripts attached to nodes in current scene + their content | AI gets full code context without per-file read_script calls |
| `godot://signal_connections` | Static | All signal connections in current scene | AI understands event flow without per-node get_node_signals calls |
| `godot://node/{node_path}` | Template | Full node details: properties, signals, script, children | On-demand deep inspection of any node |
| `godot://script/{script_path}` | Template | Script content + metadata | On-demand script access via resources protocol |

**Resource templates implementation (MCP spec 2025-03-26 `resources/templates/list`):**

```cpp
// In mcp_server.cpp handle_request():
if (method == "resources/templates/list") {
    nlohmann::json templates = nlohmann::json::array();
    templates.push_back({
        {"uriTemplate", "godot://node/{node_path}"},
        {"name", "Node Details"},
        {"description", "Full details for a specific node: all properties, signals, script, children"},
        {"mimeType", "application/json"}
    });
    templates.push_back({
        {"uriTemplate", "godot://script/{script_path}"},
        {"name", "Script Content"},
        {"description", "GDScript source code and metadata"},
        {"mimeType", "text/plain"}
    });
    return create_resource_templates_list_response(id, templates);
}
```

**URI template matching (no library needed):**
```cpp
// Simple prefix matching for godot://node/{path} and godot://script/{path}
if (uri.rfind("godot://node/", 0) == 0) {
    std::string node_path = uri.substr(13); // after "godot://node/"
    // ... resolve and return node details
}
```

**Protocol layer changes:**
1. Add `resources` capability to `create_initialize_response()`:
   ```json
   "capabilities": {
       "tools": {"listChanged": false},
       "resources": {},
       "prompts": {"listChanged": false}
   }
   ```
2. Add `create_resource_templates_list_response()` to `mcp_protocol.h/.cpp`
3. Add handler for `resources/templates/list` in `mcp_server.cpp`

**NOT implementing (too complex for v1.5, low client support):**
- Resource subscriptions (`resources/subscribe`) -- requires change notification infrastructure
- `resources/list` pagination -- resource count is small (<10)

### 3. Smart Error Handling

**Decision: Structured error responses using MCP `isError: true` tool results with diagnostic fields. NOT protocol-level JSON-RPC errors for tool execution failures.**

**Why `isError: true` tool results instead of JSON-RPC errors:**
- JSON-RPC errors are caught by MCP clients and surfaced as notifications, then discarded
- Tool results with `isError: true` are injected back into the LLM context window
- The AI sees the error message and can self-correct
- This is the MCP-recommended pattern (spec 2025-03-26, section on Error Handling)

**Current error handling (v1.4):**
```json
{"error": "Parent not found: BadPath"}
```
Simple string error in the tool result JSON. `isError` is always `false`. The AI gets the error text but no diagnostic context.

**v1.5 error handling:**
```json
{
  "error": "Parent not found: BadPath",
  "error_code": "NODE_NOT_FOUND",
  "diagnosis": "The node path 'BadPath' does not exist in the current scene tree.",
  "suggestions": [
    "Use get_scene_tree to see available node paths",
    "Check if the parent node was recently renamed or deleted",
    "Use '' or '.' for the scene root node"
  ],
  "context": {
    "scene_root": "Main",
    "available_children": ["Player", "World", "UI"]
  }
}
```

And the MCP response wrapper uses `isError: true`:
```json
{
  "result": {
    "content": [{"type": "text", "text": "<above JSON>"}],
    "isError": true
  }
}
```

**Implementation approach:**

A. **Error catalog** (`src/error_catalog.h`):
```cpp
struct ErrorInfo {
    std::string code;          // "NODE_NOT_FOUND", "INVALID_TYPE", etc.
    std::string diagnosis;     // Human-readable explanation
    std::vector<std::string> suggestions; // Recovery actions
};

// Maps error patterns to diagnostic info
const ErrorInfo& get_error_info(const std::string& error_code);
```

B. **Modified `create_tool_result`** (`mcp_protocol.h`):
```cpp
// Existing (unchanged):
nlohmann::json create_tool_result(const nlohmann::json& id, const nlohmann::json& content_data);

// New overload for error results:
nlohmann::json create_tool_error_result(const nlohmann::json& id, const nlohmann::json& error_data);
```

The `create_tool_error_result` function produces:
```cpp
return {
    {"jsonrpc", "2.0"},
    {"id", id},
    {"result", {
        {"content", {{{"type", "text"}, {"text", error_data.dump()}}}},
        {"isError", true}
    }}
};
```

C. **Helper for enriching errors** in tool functions:
```cpp
// In each tool module, wrap error returns:
nlohmann::json enrich_error(const std::string& error_code,
                             const std::string& raw_error,
                             const nlohmann::json& context = {});
```

**Error codes catalog (~15-20 codes covering all 50+ tools):**

| Error Code | Applies To | Diagnosis Pattern |
|------------|-----------|-------------------|
| `NODE_NOT_FOUND` | Any tool taking node_path | "Node 'X' not found. Available: [...]" |
| `NO_SCENE_OPEN` | All scene tools | "No scene is open. Use create_scene or open_scene first." |
| `INVALID_NODE_TYPE` | create_node | "Unknown class 'X'. Did you mean 'Y'?" (fuzzy match) |
| `NOT_A_NODE_TYPE` | create_node | "'Resource' is not a Node subclass." |
| `SCRIPT_NOT_FOUND` | read_script, attach_script | "File 'X' not found. Check the path starts with res://" |
| `SCRIPT_ALREADY_EXISTS` | write_script | "File already exists. Use edit_script to modify." |
| `INVALID_PROPERTY` | set_node_property | "Property 'X' not found on Y. Available: [...]" |
| `INVALID_VALUE_FORMAT` | set_node_property | "Cannot parse 'X' as Vector2. Expected format: 'Vector2(x,y)'" |
| `UNDO_REDO_UNAVAILABLE` | Mutating tools | "UndoRedo manager not available." |
| `GAME_NOT_RUNNING` | Runtime tools | "Game is not running. Use run_game first." |
| `BRIDGE_NOT_CONNECTED` | Bridge tools | "Game bridge not connected. Run game with wait_for_bridge: true." |
| `ANIMATION_NOT_FOUND` | Animation tools | "Animation 'X' not found on player 'Y'." |
| `SHAPE_TYPE_UNKNOWN` | create_collision_shape | "Unknown shape_type 'X'. Valid: [...]" |
| `COMPOSITE_STEP_FAILED` | Composite tools | "Step N failed: <detail>. Completed steps: [...]" |

### 4. Expanded Prompt Templates

**Decision: Add more entries to the existing `PromptDef` vector. Same architecture, same lambda-based generation pattern.**

**Current state (v1.4):** 7 prompts using the `PromptDef` struct with `std::function<nlohmann::json(const nlohmann::json&)> generate` lambda.

**v1.5 additions (~5-8 new prompts) focusing on workflow orchestration:**

| Prompt Name | Purpose | Key Arguments |
|-------------|---------|---------------|
| `create_2d_game` | End-to-end workflow for creating a basic 2D game | `game_type`: platformer/top_down/puzzle |
| `create_3d_game` | End-to-end workflow for creating a basic 3D game | `game_type`: fps/third_person/exploration |
| `debug_game_issue` | Systematic debugging workflow using runtime tools | `issue_type`: crash/visual_bug/input_not_working/performance |
| `refactor_scene` | Workflow for restructuring an existing scene tree | `strategy`: extract_subscene/flatten/regroup |
| `optimize_scene` | Performance optimization checklist and actions | `focus`: rendering/physics/scripting |
| `add_feature` | Step-by-step workflow for adding a common game feature | `feature`: save_load/inventory/dialog/particles |

**Prompt design principles for v1.5 (based on MCP prompts best practices):**
1. Embed resource references in prompt messages (link to `godot://scene_tree` etc.)
2. Include explicit tool call sequences with parameter templates
3. Add verification steps ("after creating X, use get_scene_tree to confirm")
4. Include error recovery guidance ("if create_node fails with NODE_NOT_FOUND, use get_scene_tree first")

**No new files needed.** All prompts go in `mcp_prompts.cpp` in the existing `get_prompt_defs()` vector.

---

## New Source Files

| File | Purpose | LOC Estimate |
|------|---------|-------------|
| `src/composite_tools.h` / `.cpp` | Composite tool implementations (create_character, create_ui_panel, etc.) | ~500-800 |
| `src/error_catalog.h` / `.cpp` | Error code definitions, diagnostic info, suggestion templates | ~200-300 |

**Modified existing files:**

| File | Changes |
|------|---------|
| `src/mcp_protocol.h` / `.cpp` | Add `create_tool_error_result()`, `create_resource_templates_list_response()`. Modify `create_initialize_response()` to declare resources capability. |
| `src/mcp_server.cpp` | Add `resources/templates/list` handler. Add new resource URIs to `resources/list` and `resources/read`. Add composite tool dispatch. Wire error enrichment into existing tool dispatch. |
| `src/mcp_tool_registry.cpp` | Add ~5-8 composite tool ToolDef entries. |
| `src/mcp_prompts.cpp` | Add ~5-8 new PromptDef entries. |
| Multiple tool files (scene_mutation, script_tools, etc.) | Modify error returns to use `enrich_error()` helper. |

---

## Build System Impact

**SCons changes:** None. New .cpp files in `src/` are auto-detected by `Glob("src/*.cpp")`.

**CMake changes (tests):** Add test files for composite tools and error catalog to `CMakeLists.txt`.

**Compilation impact:** ~2 new .cpp files, ~700-1100 lines. Build time increase: negligible (~2-3s).

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Composite tools | Internal C++ composition | Tool-calling-tool (JSON dispatch) | JSON overhead, no atomic UndoRedo, complexity |
| Composite tools | Single UndoRedo action | One UndoRedo per sub-step | Users expect Ctrl+Z to undo the whole "create character" |
| URI template matching | Simple string prefix matching | RFC 6570 library (e.g., Tinkoff/uri-template) | Only 2 template patterns, all Level 1 (simple {var}). Library adds ~1000 LOC dependency for zero benefit. |
| Error handling | `isError: true` tool results | JSON-RPC error codes | JSON-RPC errors are discarded by clients, never reach the LLM context window |
| Error handling | Static error catalog | ML-based error classification | Over-engineering. 50 tools with ~15 error categories is tractable with a static map. |
| Error diagnostics | Include available nodes/properties in error | Only include error message | Context helps AI self-correct. Cost is one extra tree traversal on error path only. |
| Resource templates | `godot://` custom scheme | `file://` scheme | Custom scheme is cleaner for non-filesystem resources, per MCP spec guidance |
| Prompt architecture | Same PromptDef lambdas | External prompt files (.md/.txt) | Compiled-in prompts have zero I/O, are version-consistent, and match existing pattern |
| Prompt content | Tool-sequence workflows | General game dev advice | Prompts should guide specific MCP tool usage, not duplicate Godot docs |

---

## What NOT to Add

| Don't Add | Why Not | What To Do Instead |
|-----------|---------|-------------------|
| New external C++ libraries | Zero-dependency is the core differentiator | Implement all features with existing nlohmann/json + godot-cpp + STL |
| Resource subscriptions | Low client support (Claude Desktop doesn't use them), high implementation complexity | Start with static resources + templates. Add subscriptions in a future version if demand exists. |
| Resource pagination | <10 resources total | Return all in one response |
| Structured output schemas (MCP `outputSchema` / `structuredContent`) | Spec 2025-06-18 feature, we target 2025-03-26. Not supported by most clients yet. | Stick with TextContent JSON strings. Consider for future spec upgrade. |
| Tool annotations (`readOnlyHint`, `destructiveHint`) | Spec 2025-06-18 feature | Consider for future spec upgrade |
| `listChanged` notifications for tools/resources | Requires notification infrastructure, tools/resources are static at runtime | Declare `listChanged: false` |
| Mega-tool pattern (better-godot-mcp style "18 tools with action parameter") | Reduces discoverability for AI. AI works better with explicit tool names than action-within-tool dispatch. Our 50+ named tools perform better with Claude than 18 mega-tools. | Keep granular atomic tools + add targeted composite tools for genuinely multi-step operations |

---

## Version Compatibility

| Component | Compatible With | Notes |
|-----------|-----------------|-------|
| nlohmann/json 3.12.0 | C++17, godot-cpp v10+ | Already latest release, no update needed |
| MCP spec 2025-03-26 | Claude Desktop, Cursor, VS Code MCP | Resource templates added in this spec version |
| `resources/templates/list` | Claude Desktop (confirmed) | Supported in spec 2025-03-26, widely adopted |
| `isError: true` tool results | All MCP clients | Part of core spec since inception |
| godot-cpp v10 | Godot 4.3+ | Unchanged minimum requirement |

---

## Installation

No new packages or dependencies. Existing build commands:

```bash
# Core build (unchanged):
scons platform=<platform> target=template_debug

# Bridge build (unchanged):
scons bridge

# Tests (unchanged):
cd tests && cmake -B build && cmake --build build
ctest --test-dir build
```

---

## Sources

### MCP Specification (2025-03-26) -- HIGH Confidence
- [MCP Resources Specification](https://spec.modelcontextprotocol.io/specification/2025-03-26/server/resources/) -- resource templates, URI templates, capabilities
- [MCP Tools Specification (2025-06-18)](https://modelcontextprotocol.io/specification/2025-06-18/server/tools) -- isError, tool result structure, error handling patterns
- [MCP Prompts for Automation Blog](http://blog.modelcontextprotocol.io/posts/2025-07-29-prompts-for-automation/) -- prompt design patterns, resource-prompt integration

### MCP Error Handling Best Practices -- MEDIUM Confidence
- [Better MCP tool/call Error Responses (Alpic AI)](https://dev.to/alpic/better-mcp-toolscall-error-responses-help-your-ai-recover-gracefully-15c7) -- isError vs protocol errors, three error patterns
- [MCP Server Best Practices (MCPcat)](https://mcpcat.io/blog/mcp-server-best-practices/) -- tool composition, batch patterns
- [54 Patterns for Better MCP Tools (Arcade)](https://arcade.dev/blog/mcp-tool-patterns) -- tool design patterns, composition

### Competitive Landscape -- MEDIUM Confidence
- [better-godot-mcp (18 composite mega-tools)](https://github.com/n24q02m/better-godot-mcp) -- composite tool design reference, Node.js implementation
- [Godot MCP Pro](https://gdaimcp.com/) -- 162 tools, commercial reference

### nlohmann/json -- HIGH Confidence
- [nlohmann/json GitHub Releases](https://github.com/nlohmann/json/releases) -- confirmed 3.12.0 is latest (2025-04-11)

### Codebase Analysis (Local) -- HIGH Confidence
- `src/mcp_server.cpp` -- current tool dispatch, resource handling, error patterns
- `src/mcp_protocol.h/.cpp` -- current protocol builders, initialize response
- `src/mcp_prompts.cpp` -- current prompt architecture (PromptDef + lambdas)
- `src/mcp_tool_registry.h/.cpp` -- current ToolDef registration pattern
- `src/scene_mutation.cpp` -- error return pattern, UndoRedo integration
- `src/physics_tools.cpp` -- composite-like pattern (create CollisionShape + Shape resource in one call)

---
*Stack research for: v1.5 AI Workflow Enhancement*
*Researched: 2026-03-23*
