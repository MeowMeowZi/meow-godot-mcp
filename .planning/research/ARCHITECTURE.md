# Architecture Patterns: v1.5 AI Workflow Enhancement Integration

**Domain:** Godot GDExtension MCP Server Plugin -- v1.5 AI Workflow Enhancement
**Researched:** 2026-03-23
**Scope:** How composite tools, enriched MCP Resources, smart error handling, and expanded prompt templates integrate with the existing v1.0-v1.4 architecture

## Existing Architecture Summary (v1.4)

```
AI Client <--stdio--> Bridge (~50KB) <--TCP--> GDExtension (MCPServer)
                                                    |
                                              MCPPlugin (EditorPlugin)
                                                    |
                                              MCPServer (plain C++)
                                               /       \
                                    IO thread    Main thread (poll)
                                   (TCP r/w)     (Godot API calls)
                                                    |
                                          +--- handle_request() ---+
                                          |    giant if/else chain |
                                          |    ~1060 lines         |
                                          +------------------------+
                                                    |
                                            Tool modules (free functions)
                                            - scene_tools.h/.cpp
                                            - scene_mutation.h/.cpp
                                            - script_tools.h/.cpp
                                            - project_tools.h/.cpp
                                            - runtime_tools.h/.cpp
                                            - signal_tools.h/.cpp
                                            - scene_file_tools.h/.cpp
                                            - ui_tools.h/.cpp
                                            - animation_tools.h/.cpp
                                            - viewport_tools.h/.cpp
                                            - tilemap_tools.h/.cpp
                                            - physics_tools.h/.cpp

                                            Registry (Godot-free, unit-testable)
                                            - mcp_tool_registry.h/.cpp (ToolDef array)
                                            - mcp_protocol.h/.cpp (~180 LOC)
                                            - mcp_prompts.h/.cpp (PromptDef array)
                                            - variant_parser.h/.cpp (string -> Godot types)

                                            Game Bridge (runtime tools)
                                            - game_bridge.h/.cpp (EditorDebuggerPlugin)
```

**Key architectural patterns in existing code:**

1. **Tool modules are free functions** returning `nlohmann::json`. Signature: `json tool_func(args..., UndoRedo*)`. No classes, no inheritance. Each module has a `.h` (Godot-free interface when possible, `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED` for Godot-dependent functions) and a `.cpp`.

2. **Tool dispatch is a monolithic if/else chain** in `MCPServer::handle_request()`. Each tool name maps to argument extraction + function call. Currently ~660 lines of dispatch code for 50 tools.

3. **Error handling is ad-hoc**: tool functions return `{"error": "message"}` on failure, `{"success": true, ...}` on success. `mcp_server.cpp` returns `mcp::create_error_response()` for missing/invalid params. No structured diagnostics, no fix suggestions, no error categorization.

4. **MCP Resources are hardcoded**: exactly two resources (`godot://scene_tree`, `godot://project_files`), handled inline in `handle_request()` with no resource template support. No `resources/templates/list` handler exists.

5. **MCP Prompts are static generators**: `PromptDef` struct with name/description/arguments/generate lambda. `get_prompt_messages()` does argument substitution and returns message arrays. Currently 7 prompts.

6. **Threading model**: IO thread handles TCP+JSON-RPC parsing, queues `PendingRequest` to main thread. Main thread `poll()` dequeues, calls `handle_request()`, queues `PendingResponse` back. Some requests use `__deferred` marker for async operations (viewport capture, bridge wait).

7. **Registry is Godot-free**: `mcp_tool_registry.h`, `mcp_protocol.h`, `mcp_prompts.h` use pure C++17 + nlohmann/json. This enables GoogleTest unit testing without godot-cpp linkage.

## Feature 1: Composite Tools

### What It Is

Multi-step operations bundled into a single MCP tool call. Examples:
- `create_character`: Creates CharacterBody2D + CollisionShape2D + Sprite2D + script in one call
- `create_ui_panel`: Creates PanelContainer + layout + label + buttons + styling
- `create_tilemap_level`: Creates TileMapLayer + TileSet + paints initial tiles

### Integration Points

**New files (CREATE):**
- `src/composite_tools.h` -- function declarations
- `src/composite_tools.cpp` -- implementations

**Modified files (MODIFY):**
- `src/mcp_tool_registry.cpp` -- add ToolDef entries for each composite tool
- `src/mcp_server.cpp` -- add dispatch branches in `handle_request()`

### Architecture Decision: Composition Strategy

Composite tools should call existing tool functions directly (internal function calls), NOT go through the MCP dispatch layer. Rationale:

```cpp
// GOOD: Direct internal composition
nlohmann::json create_character(const std::string& char_type, ..., EditorUndoRedoManager* undo_redo) {
    // Step 1: Create root node
    auto root_result = create_node("CharacterBody2D", parent_path, name, {}, undo_redo);
    if (root_result.contains("error")) return root_result;

    // Step 2: Create collision shape
    auto col_result = create_collision_shape(root_result["path"], "capsule", {...}, "", undo_redo);
    if (col_result.contains("error")) {
        // Partial failure: undo step 1
        delete_node(root_result["path"], undo_redo);
        return {{"error", "Failed to create collision shape: " + col_result["error"].get<std::string>()}};
    }

    // Step 3: Create sprite, write script, attach script...
    return {
        {"success", true},
        {"nodes_created", nlohmann::json::array({...})},
        {"steps_completed", 5}
    };
}
```

```cpp
// BAD: Re-dispatching through MCP (unnecessary overhead, breaks error reporting)
nlohmann::json create_character(...) {
    auto result1 = handle_request("tools/call", id, {{"name", "create_node"}, ...});
    // Now you have a JSON-RPC response wrapper to unwrap... messy.
}
```

**Why direct calls:** Composite tools ARE internal orchestrators. They need:
- Direct access to `undo_redo` for transactional grouping (wrap all steps in one UndoRedo action)
- Ability to pass `Node*` pointers between steps (avoid redundant lookups)
- Clean error propagation without JSON-RPC wrapper noise
- The ability to undo partial work on failure

### UndoRedo Grouping

Critical: each composite tool must use a SINGLE UndoRedo action so Ctrl+Z undoes the entire composite operation, not individual steps. Current tool functions each create their own action. For composites, we need to either:

**Option A (recommended):** Create a new UndoRedo action in the composite function, call lower-level helpers that accept the existing action instead of creating new ones. This requires extracting the "meat" of create_node/set_node_property into helpers that DON'T create their own action.

```cpp
// In composite_tools.cpp:
nlohmann::json create_character(..., EditorUndoRedoManager* undo_redo) {
    undo_redo->create_action("MCP: Create Character");

    // Lower-level helpers that add to existing action
    auto root = create_node_raw(type, parent, name, props, undo_redo); // no create_action inside
    auto col = add_collision_shape_raw(..., undo_redo);
    auto script = create_and_attach_script_raw(..., undo_redo);

    undo_redo->commit_action();
    return {{"success", true}, ...};
}
```

**Option B (pragmatic fallback):** Call existing functions as-is. Each sub-step creates its own UndoRedo action. Ctrl+Z undoes one step at a time. Less elegant but zero refactoring of existing code.

**Recommendation:** Start with Option B for speed. Refactor to Option A in a later polish phase if UX testing reveals that multi-step undo is confusing. The existing functions work and are battle-tested; introducing `_raw` variants is refactoring risk.

### Return Format for Composites

Composite tools should return a structured summary:

```json
{
    "success": true,
    "nodes_created": [
        {"path": "Player", "type": "CharacterBody2D"},
        {"path": "Player/CollisionShape2D", "type": "CollisionShape2D"},
        {"path": "Player/Sprite2D", "type": "Sprite2D"}
    ],
    "script_created": "res://scripts/player.gd",
    "steps_completed": 5,
    "steps_total": 5
}
```

On partial failure:

```json
{
    "error": "Failed at step 3/5: Could not create script",
    "partial_results": {
        "nodes_created": [
            {"path": "Player", "type": "CharacterBody2D"},
            {"path": "Player/CollisionShape2D", "type": "CollisionShape2D"}
        ],
        "steps_completed": 2,
        "steps_total": 5,
        "failed_step": "write_script"
    }
}
```

### Impact on Dispatch Chain

Each composite tool adds one more `if (tool_name == "...")` branch in `handle_request()`. With 50 existing tools and ~5-8 composite tools, the chain grows to ~58 branches. This is manageable but the function is already ~1060 lines. See "Architecture Concern: Dispatch Growth" below.

## Feature 2: Enriched MCP Resources

### What It Is

Expand from 2 static resources to a richer set that gives AI full context about the project:

| Current Resource | Enriched Resources |
|---|---|
| `godot://scene_tree` | Keep as-is |
| `godot://project_files` | Keep as-is |
| (none) | `godot://node/{path}` -- detailed node info (all properties, signals, connections) |
| (none) | `godot://script/{path}` -- script source + metadata |
| (none) | `godot://signal_map` -- full signal connection graph |
| (none) | `godot://scene_scripts` -- all scripts in current scene |

### Integration Points

**New files (CREATE):**
- `src/resource_providers.h` -- resource provider function declarations
- `src/resource_providers.cpp` -- implementations

**Modified files (MODIFY):**
- `src/mcp_server.cpp` -- extend `resources/list` and `resources/read` handlers
- `src/mcp_protocol.h/.cpp` -- add `create_resource_templates_list_response()` builder

### Static vs Template Resources

The MCP spec (2025-03-26) supports **resource templates** via `resources/templates/list`. Parameterized resources like `godot://node/{path}` and `godot://script/{path}` should be exposed as templates:

```json
{
    "resourceTemplates": [
        {
            "uriTemplate": "godot://node/{node_path}",
            "name": "Node Details",
            "description": "Detailed information about a specific node: all properties, signals, connections, children",
            "mimeType": "application/json"
        },
        {
            "uriTemplate": "godot://script/{script_path}",
            "name": "Script Content",
            "description": "GDScript source code and metadata for a script file",
            "mimeType": "application/json"
        }
    ]
}
```

Non-parameterized enriched resources go in `resources/list`:

```json
{
    "resources": [
        {"uri": "godot://scene_tree", "name": "Scene Tree", ...},
        {"uri": "godot://project_files", "name": "Project Files", ...},
        {"uri": "godot://signal_map", "name": "Signal Connection Map", ...},
        {"uri": "godot://scene_scripts", "name": "Scene Scripts", ...}
    ]
}
```

### Resource Implementation Pattern

Each enriched resource is a free function returning `nlohmann::json`, following the tool module pattern:

```cpp
// resource_providers.h
#include <nlohmann/json.hpp>
#include <string>

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
nlohmann::json get_node_details(const std::string& node_path);
nlohmann::json get_script_content(const std::string& script_path);
nlohmann::json get_signal_map();
nlohmann::json get_scene_scripts();
#endif
```

### Resource Read Dispatch

The `resources/read` handler in `mcp_server.cpp` currently does exact URI matching. For template resources, it needs prefix matching + parameter extraction:

```cpp
// In handle_request, resources/read section:
if (uri.rfind("godot://node/", 0) == 0) {
    std::string node_path = uri.substr(13); // len("godot://node/") = 13
    auto details = get_node_details(node_path);
    // ... wrap in resource_read_response
}
if (uri.rfind("godot://script/", 0) == 0) {
    std::string script_path = uri.substr(15);
    // ...
}
```

### Resources Capability Declaration

The `create_initialize_response()` in `mcp_protocol.cpp` must be updated to declare the `resources` capability:

```cpp
// Currently missing from capabilities:
{"capabilities", {
    {"tools", {{"listChanged", false}}},
    {"prompts", {{"listChanged", false}}},
    {"resources", {}}  // ADD THIS -- no subscribe or listChanged needed for now
}}
```

### What `get_node_details` Returns

More detail than `get_scene_tree` provides for a single node. Includes ALL properties, not just transform/visible/script:

```json
{
    "name": "Player",
    "type": "CharacterBody2D",
    "path": "Player",
    "properties": {
        "position": "Vector2(100, 200)",
        "rotation": 0.0,
        "scale": "Vector2(1, 1)",
        "velocity": "Vector2(0, 0)",
        "floor_max_angle": 0.785398,
        "collision_layer": 1,
        "collision_mask": 1
    },
    "signals": {
        "defined": ["ready", "tree_entered", "tree_exiting", ...],
        "connections": [
            {"signal": "body_entered", "target": "HitBox", "method": "_on_body_entered"}
        ]
    },
    "script": {
        "path": "res://scripts/player.gd",
        "source_preview": "extends CharacterBody2D\n\nconst SPEED = 300.0\n..."
    },
    "children": ["CollisionShape2D", "Sprite2D", "AnimationPlayer"],
    "groups": ["players"]
}
```

### What `get_signal_map` Returns

Full signal connection graph for the current scene:

```json
{
    "connections": [
        {
            "source": "Player/HurtBox",
            "signal": "area_entered",
            "target": "Player",
            "method": "_on_hurt_box_area_entered",
            "flags": 0
        }
    ],
    "node_count": 15,
    "connection_count": 3
}
```

This reuses logic from `signal_tools.cpp::get_node_signals()` but iterates all nodes.

## Feature 3: Smart Error Handling

### What It Is

Transform bare `{"error": "Node not found: BadPath"}` into diagnostic-rich responses:

```json
{
    "error": "Node not found: BadPath",
    "diagnostics": {
        "category": "node_not_found",
        "searched_path": "BadPath",
        "scene_root": "Main",
        "available_nodes": ["Player", "World", "UI"],
        "did_you_mean": "Player"
    },
    "suggestions": [
        "Check if the node path is relative to the scene root (not absolute)",
        "Use get_scene_tree to see all available node paths",
        "Node paths are case-sensitive in Godot"
    ]
}
```

### Integration Strategy: Wrapper, Not Rewrite

Do NOT modify every existing tool function. Instead, create an error enrichment layer that wraps tool results:

**New files (CREATE):**
- `src/error_diagnostics.h` -- error enrichment function declarations
- `src/error_diagnostics.cpp` -- implementations

**Modified files (MODIFY):**
- `src/mcp_server.cpp` -- wrap tool results through enrichment before returning

### Error Enrichment Pipeline

```cpp
// error_diagnostics.h

// Enrich a tool result if it contains an error
// Returns the original result unchanged if no error
nlohmann::json enrich_error(const nlohmann::json& result,
                            const std::string& tool_name,
                            const nlohmann::json& args);
```

Applied in `mcp_server.cpp` dispatch:

```cpp
// Current pattern (unchanged):
if (tool_name == "create_node") {
    // ... extract args ...
    auto result = create_node(type, parent_path, node_name, properties, undo_redo);
    return mcp::create_tool_result(id, enrich_error(result, "create_node", args));
    //                                  ^^^^^^^^^^^^ NEW: wrap result
}
```

This is minimally invasive. Each dispatch branch adds one `enrich_error()` call wrapping the tool result. No tool function changes needed.

### Error Categories and Diagnostics

```cpp
// error_diagnostics.cpp

struct ErrorPattern {
    std::string category;        // "node_not_found", "invalid_type", "no_scene", etc.
    std::string pattern;         // regex or prefix to match in error message
    std::function<nlohmann::json(const nlohmann::json& result, const std::string& tool, const nlohmann::json& args)> diagnose;
};
```

Key error categories to handle:

| Category | Error Message Pattern | Diagnostics |
|---|---|---|
| `node_not_found` | "Node not found: X" | List available nodes, suggest closest match |
| `no_scene` | "No scene open" | Tell AI to use create_scene or open_scene first |
| `invalid_type` | "Unknown class: X" | List similar class names, check 2D/3D suffix |
| `parent_not_found` | "Parent not found: X" | List available parent paths |
| `script_not_found` | "File not found" | Check res:// prefix, list available scripts |
| `property_invalid` | Property-related errors | List valid properties for the node type |
| `bridge_not_connected` | "Game bridge not initialized" | Tell AI to run_game with wait_for_bridge first |

### "Did You Mean" Implementation

For `node_not_found` and `parent_not_found`, compute Levenshtein distance against actual scene tree paths:

```cpp
// Simple Levenshtein distance for suggestion generation
// Only compute against shallow scene tree (depth 2-3) to keep it fast
std::string find_closest_node(const std::string& attempted_path);
```

This requires calling `get_scene_tree()` within the diagnostics layer -- acceptable because errors are the slow path and scene tree queries are fast.

### Godot-Free vs Godot-Dependent Diagnostics

Some diagnostics need Godot API access (listing nodes, checking classes). The `enrich_error` function lives in Godot-dependent code (`#ifdef MEOW_GODOT_MCP_GODOT_ENABLED`). This is fine because it runs on the main thread inside `handle_request()`.

For unit-testable error pattern matching, the pattern registry itself can be Godot-free:

```cpp
// error_diagnostics.h (Godot-free part)
struct ErrorCategory {
    std::string category;
    std::vector<std::string> suggestions;
};

// Match an error message to a category (pure C++, unit-testable)
ErrorCategory categorize_error(const std::string& error_message, const std::string& tool_name);

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
// Full enrichment with Godot API access (node listing, class checking)
nlohmann::json enrich_error(const nlohmann::json& result, const std::string& tool_name, const nlohmann::json& args);
#endif
```

### Performance Consideration

Error enrichment does extra work (scene tree traversal, string matching). This is acceptable because:
1. Errors are the minority case (success path has zero overhead)
2. `get_scene_tree()` is already fast (used by tools/call every day)
3. Levenshtein on ~50-200 node names is microseconds

## Feature 4: Expanded Prompt Templates

### What It Is

More workflow-oriented prompts that guide AI through multi-step game development tasks. Currently 7 prompts; target ~12-15.

### Integration Points

**Modified files (MODIFY):**
- `src/mcp_prompts.cpp` -- add new PromptDef entries to the `defs` vector

No new files needed. No structural changes. The existing `PromptDef` struct and dispatch are sufficient.

### New Prompt Categories

| Prompt Name | Description | Arguments |
|---|---|---|
| `debug_scene` | Debug scene tree issues (missing nodes, wrong types) | `symptom` (string) |
| `optimize_performance` | Performance optimization workflow | `area` (rendering/physics/scripting) |
| `setup_collision_layers` | Configure collision layer/mask strategy | `game_type` |
| `create_state_machine` | Create a state machine pattern | `entity_type` (player/enemy/npc) |
| `setup_autoloads` | Configure autoload singletons | `features` (save/audio/events) |
| `test_game_feature` | Test a game feature end-to-end | `feature_description` |
| `debug_error` | Diagnose a specific error message | `error_text` |
| `refactor_scene` | Refactor scene structure | `issue` (deep_nesting/large_script/...) |

### Prompt Design Pattern

New prompts should reference specific MCP tools and show exact parameter formats. This is the pattern established by `build_ui_layout`:

```
Step 1: [What to do]
  Tool: [tool_name]
  Parameters: { ... }
  Result: [Expected outcome]
```

Prompts that reference composite tools should be written AFTER composite tools are implemented, to ensure tool names and parameters are accurate.

### Context-Aware Prompts (Future Consideration)

Currently, prompts are static text templates. A future enhancement could make prompts context-aware by reading the current scene tree and tailoring guidance. This would require:
- Passing scene context into `get_prompt_messages()`
- Changing the PromptDef generate signature to accept additional context

This is out of scope for v1.5 but worth noting as a v2.0 direction.

## Architecture Concern: Dispatch Growth

The `MCPServer::handle_request()` function is already 1060 lines with 50 tool branches. Adding composite tools will push it further. Two mitigation options:

### Option A: Tool Dispatch Table (Recommended for v1.5)

Replace the if/else chain with a function pointer map:

```cpp
// In mcp_server.h or a new mcp_dispatch.h:
using ToolHandler = std::function<nlohmann::json(const nlohmann::json& id,
                                                  const nlohmann::json& args,
                                                  MCPServer* server)>;

// In mcp_server.cpp:
static const std::unordered_map<std::string, ToolHandler>& get_tool_handlers() {
    static const std::unordered_map<std::string, ToolHandler> handlers = {
        {"get_scene_tree", [](auto& id, auto& args, auto* s) {
            int max_depth = get_int(args, "max_depth", -1);
            bool include_properties = get_bool(args, "include_properties", true);
            std::string root_path = get_string(args, "root_path");
            return mcp::create_tool_result(id, get_scene_tree(max_depth, include_properties, root_path));
        }},
        {"create_node", [](auto& id, auto& args, auto* s) {
            // ...
        }},
        // ... all 50+ tools
    };
    return handlers;
}
```

**Pros:** O(1) lookup vs O(n) if/else, cleaner code, easier to add new tools.
**Cons:** Requires passing MCPServer context (undo_redo, game_bridge) through the handler. Each handler needs access to server state.

**However, this is a refactoring task, not a feature task.** Recommendation: do NOT refactor the dispatch in v1.5. The if/else chain works, is easy to understand, and adding 5-8 more branches is not a crisis. Flag this as tech debt for v2.0.

### Option B: Keep If/Else, Add Comments (Pragmatic for v1.5)

Keep the current pattern. Add section comments for tool groups:

```cpp
// === Composite Tools ===
if (tool_name == "create_character") { ... }
if (tool_name == "create_ui_panel") { ... }
```

**Recommendation:** Option B for v1.5. The dispatch table refactor is a separate concern and should not gate v1.5 features.

## Component Boundaries (v1.5)

### New Components

```
src/
  composite_tools.h/.cpp      -- NEW: multi-step tool orchestrators
  resource_providers.h/.cpp   -- NEW: enriched resource data providers
  error_diagnostics.h/.cpp    -- NEW: error categorization + enrichment
```

### Modified Components

```
src/
  mcp_server.cpp              -- MODIFY: add composite dispatch, enrich errors,
                                  extend resources/read, add resources/templates/list
  mcp_tool_registry.cpp       -- MODIFY: add ToolDef entries for composite tools
  mcp_protocol.h/.cpp         -- MODIFY: add resources capability, template list builder
  mcp_prompts.cpp             -- MODIFY: add new prompt templates
```

### Untouched Components (validate no changes needed)

```
src/
  mcp_server.h                -- No new public API needed
  mcp_dock.h/.cpp             -- No UI changes for v1.5
  mcp_plugin.h/.cpp           -- No lifecycle changes
  game_bridge.h/.cpp          -- No new bridge capabilities
  register_types.h/.cpp       -- No new Godot classes
  variant_parser.h/.cpp       -- No new type conversions
  scene_tools.h/.cpp          -- Existing, called by composite_tools
  scene_mutation.h/.cpp       -- Existing, called by composite_tools
  script_tools.h/.cpp         -- Existing, called by composite_tools
  signal_tools.h/.cpp         -- Existing, called by resource_providers
  ui_tools.h/.cpp             -- Existing, called by composite_tools
  physics_tools.h/.cpp        -- Existing, called by composite_tools
  tilemap_tools.h/.cpp        -- Existing, not directly relevant
  animation_tools.h/.cpp      -- Existing, not directly relevant
  viewport_tools.h/.cpp       -- Existing, not directly relevant
  scene_file_tools.h/.cpp     -- Existing, not directly relevant
  project_tools.h/.cpp        -- Existing, called by resource_providers
  runtime_tools.h/.cpp        -- Existing, not directly relevant
```

### Data Flow for Each Feature

**Composite Tools data flow:**
```
MCP Client -> tools/call "create_character"
  -> MCPServer::handle_request() dispatch
    -> composite_tools::create_character()
      -> scene_mutation::create_node()      (step 1)
      -> physics_tools::create_collision_shape()  (step 2)
      -> script_tools::write_script()       (step 3)
      -> script_tools::attach_script()      (step 4)
    -> enrich_error() (if error)
  -> mcp::create_tool_result()
-> MCP Client
```

**Enriched Resources data flow:**
```
MCP Client -> resources/read { uri: "godot://node/Player" }
  -> MCPServer::handle_request()
    -> URI prefix match "godot://node/"
    -> resource_providers::get_node_details("Player")
      -> scene_tools::get_scene_tree() (reuse)
      -> signal_tools::get_node_signals() (reuse)
      -> script_tools::read_script() (reuse)
    -> mcp::create_resource_read_response()
-> MCP Client
```

**Smart Error data flow:**
```
MCP Client -> tools/call "set_node_property" { node_path: "Playe" }
  -> MCPServer::handle_request() dispatch
    -> scene_mutation::set_node_property()
    <- {"error": "Node not found: Playe"}
    -> error_diagnostics::enrich_error(result, "set_node_property", args)
      -> categorize_error() -> "node_not_found"
      -> get_scene_tree() to find available nodes
      -> levenshtein("Playe", ["Player", "World", ...]) -> "Player"
    <- {"error": "...", "diagnostics": {...}, "suggestions": [...]}
  -> mcp::create_tool_result()
-> MCP Client
```

## Build Order (Dependency-Based)

### Phase 1: Error Diagnostics (no dependencies, enables testing other features)
1. Create `error_diagnostics.h/.cpp` with `categorize_error()` (Godot-free, unit-testable)
2. Add `enrich_error()` with Godot-dependent diagnostics
3. Wire into `mcp_server.cpp` dispatch (wrap existing tool results)
4. Add GoogleTest unit tests for `categorize_error()`

**Why first:** Error enrichment is a passive wrapper. Adding it early means all subsequent tools (including composites) automatically get smart errors. Zero risk to existing functionality.

### Phase 2: Enriched Resources (depends on existing tool functions only)
1. Create `resource_providers.h/.cpp`
2. Add `resources/templates/list` handler to `mcp_server.cpp`
3. Add static resources (`signal_map`, `scene_scripts`) to `resources/list`
4. Add template resources (`node/{path}`, `script/{path}`) to `resources/read`
5. Update `create_initialize_response()` to declare resources capability

**Why second:** Resources are read-only queries that reuse existing tool module functions. No mutation, minimal risk. Validates that resource providers work before composite tools need them.

### Phase 3: Composite Tools (depends on error diagnostics being in place)
1. Create `composite_tools.h/.cpp`
2. Implement 2-3 composite tools (start with `create_character`, `create_ui_panel`)
3. Add ToolDef entries to `mcp_tool_registry.cpp`
4. Add dispatch branches to `mcp_server.cpp`
5. Error enrichment automatically applies (from Phase 1)
6. Add UAT tests

**Why third:** Composite tools are the most complex feature. They call multiple existing functions and benefit from error diagnostics being in place. Most likely to surface edge cases.

### Phase 4: Expanded Prompts (depends on composite tools existing)
1. Add new PromptDef entries to `mcp_prompts.cpp`
2. Reference composite tool names and parameters in prompt text
3. Add prompts for debugging workflows that leverage smart errors

**Why last:** Prompts reference tool names and parameters. They must be written after composite tools have settled names. Also the lowest-risk feature -- purely additive text content.

## Testing Strategy

### Unit Tests (GoogleTest, no Godot)
- `test_error_diagnostics.cpp` -- error categorization, suggestion generation, Levenshtein
- `test_tool_registry.cpp` -- existing, extend to verify composite tool ToolDef entries
- `test_protocol.cpp` -- existing, extend for resource template response format

### UAT Tests (Python, live Godot)
- `tests/uat_composite_tools.py` -- create_character, verify nodes exist, undo
- `tests/uat_enriched_resources.py` -- resources/list, resources/read, template resources
- `tests/uat_smart_errors.py` -- trigger known errors, verify diagnostics present

### Manual Testing Checklist
- [ ] Composite tool creates all expected nodes
- [ ] Ctrl+Z undoes composite operation (all or stepwise, document which)
- [ ] Resource templates list includes new entries
- [ ] Reading `godot://node/Player` returns full property set
- [ ] Error for misspelled node path includes "did you mean"
- [ ] New prompts display correctly in AI client

## Anti-Patterns to Avoid

### Anti-Pattern 1: Over-Engineering Composite Tool Framework
**What:** Creating a generic "pipeline" or "recipe" system for defining composite tools via data.
**Why bad:** Each composite tool has unique logic (different node types, different validation, different script templates). A generic framework adds complexity without reducing code.
**Instead:** Each composite tool is a plain function. Repetition is OK -- these are orchestration functions, not library code.

### Anti-Pattern 2: Breaking the Godot-Free Boundary
**What:** Adding Godot headers to `mcp_tool_registry.h` or `mcp_protocol.h` for new features.
**Why bad:** Breaks unit test compilation. The Godot-free boundary is a core architecture invariant.
**Instead:** Keep registry/protocol pure C++. All Godot-dependent logic goes in `.cpp` files behind `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED`.

### Anti-Pattern 3: Modifying Existing Tool Function Signatures
**What:** Changing `create_node()` signature to support composite tools.
**Why bad:** Breaks all existing call sites and tests. High regression risk.
**Instead:** Composite tools call existing functions as-is. If lower-level control is needed, add NEW helper functions alongside existing ones.

### Anti-Pattern 4: Eager Error Enrichment
**What:** Running expensive diagnostics (scene tree traversal) on EVERY tool result, including successes.
**Why bad:** Performance hit on the happy path.
**Instead:** `enrich_error()` checks for `result.contains("error")` first. Success results pass through with zero overhead.

### Anti-Pattern 5: Resource Providers That Mutate State
**What:** Making `resources/read` handlers that modify the scene.
**Why bad:** MCP Resources are defined as read-only context. Mutation violates the spec's intent.
**Instead:** Resources are pure queries. Use `tools/call` for any mutations.

## Scalability Considerations

| Concern | Current (v1.4) | After v1.5 | Notes |
|---|---|---|---|
| Tool count | 50 | ~58 (8 composites) | Still manageable in if/else chain |
| Resource count | 2 | ~6 static + 2 templates | Minimal overhead |
| Prompt count | 7 | ~15 | Static text, no runtime cost |
| Dispatch function LOC | ~1060 | ~1250 | Consider table dispatch at v2.0 |
| New source files | 0 | 3 pairs (6 files) | Clean module boundaries |
| New test files | 0 | 1 unit + 3 UAT | Good coverage |

## Sources

- MCP Spec 2025-03-26 Resources: https://modelcontextprotocol.io/specification/2025-03-26/server/resources
- MCP Spec Resource Templates: RFC 6570 URI Templates referenced in spec
- Existing codebase analysis: direct code review of all source files (HIGH confidence)
- Architecture decisions: from PROJECT.md and MEMORY.md context (HIGH confidence)
