# Feature Research: v1.5 AI Workflow Enhancement

**Domain:** MCP Server plugin for Godot Engine (GDExtension/C++) -- v1.5 milestone
**Researched:** 2026-03-23
**Confidence:** HIGH (based on MCP spec analysis, competitor audit, existing codebase review)
**Competitive context:** Godot MCP Pro (162 tools, batch_set_property, find_nodes_by_type), Better Godot MCP (18 mega-tools), GDAI MCP (debug+screenshot loop)

## Existing Capabilities (v1.4 baseline)

50 MCP tools, 7 prompt templates, 2 MCP Resources (scene://tree, project://files), editor dock panel with connection status, game bridge for runtime interaction. Full scene CRUD, script management, UI system, animation, runtime control, signals, TileMap, collision shapes, resource properties. UndoRedo integration on all mutations. Version-adaptive tool registry.

**Key architectural fact for v1.5:** This milestone does NOT add new Godot API surface. It adds intelligence and ergonomics on top of existing tools -- composite operations that call existing primitives, richer context from existing data, better error messages from existing error paths, and workflow guidance from existing tool knowledge.

---

## Feature Area 1: Composite Tools

High-level tools that combine multiple existing primitive operations into single calls, reducing AI round-trips and preventing common sequencing errors.

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Create character with collision shape | AI currently needs 3-5 tool calls to create a CharacterBody2D + CollisionShape2D + Sprite2D + script. Competitors (Godot MCP Pro, Better Godot MCP) bundle these. Users switching from competitors will expect this. | MEDIUM | create_node, create_collision_shape, write_script, attach_script |
| Create UI panel with children | Building a menu requires 6-10 tool calls (container + children + layout + style). A composite tool accepts a declarative layout spec and creates it in one UndoRedo action. | MEDIUM | create_node, set_layout_preset, set_theme_override, set_container_layout, create_stylebox |
| Batch property set | Set the same property on multiple nodes at once (e.g., set `visible=false` on all enemies). Godot MCP Pro has `batch_set_property`. Without this, AI loops one-at-a-time with 50+ tool calls. | LOW | set_node_property (loop internally) |
| Find nodes by type/name pattern | Search the scene tree for all nodes matching a class or name pattern. Essential for "change all Labels to font_size 24" workflows. Currently AI must call get_scene_tree, parse it client-side, then call per-node. | LOW | get_scene_tree (filtered variant) |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Create complete scene from template | One call: "create a platformer level with Player, Ground, Camera, HUD". Generates the full node tree + starter scripts. Goes beyond competitors who only bundle 2-3 nodes. Unique to us because the existing prompt templates already describe these structures -- composite tool EXECUTES them instead of just describing. | HIGH | All scene/node/script tools. Must define sane templates for 4-6 game genres. |
| Atomic multi-step with rollback | Composite tools wrap all sub-operations in a SINGLE UndoRedo action. If step 3 of 5 fails, steps 1-2 are rolled back. Competitors using Node.js can't do this -- they make individual WebSocket calls. Our C++ GDExtension can batch UndoRedo natively. | LOW (architecture already supports this) | UndoRedo manager |
| Clone node subtree | Deep-copy a node and all its children/scripts to a new parent. Useful for "duplicate this enemy with a new name". Not a direct Godot API call -- requires recursive tree walk + re-ownership. | MEDIUM | create_node, set_node_property (recursive) |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| "Do everything" mega-tool | A single tool that accepts free-form instructions like "build me a game" creates unpredictable behavior, huge input schemas, and makes AI tool selection unreliable. Better Godot MCP's 18-mega-tool approach leads to bloated schemas. | Keep composites focused: 1 composite = 1 well-defined multi-step workflow. 5-8 composite tools, not 1 god tool. |
| Auto-generated scripts with hardcoded templates | Shipping full GDScript templates inside C++ strings is brittle, hard to maintain, and becomes stale. | Composite tools create the NODE TREE structure. Script creation is a separate step the AI handles with write_script using its own intelligence. Exception: very minimal starter scripts (extends + class_name) are OK. |
| Composite tools that bypass UndoRedo | Tempting to skip UndoRedo for speed, but users WILL need to undo composite operations. Breaking undo = breaking trust. | Always wrap in a single UndoRedo action. This is our GDExtension advantage over WebSocket competitors. |

### Implementation Notes

**Composite tool architecture pattern:**
```
composite_tool(params) {
    undo_redo->create_action("MCP: Composite - Create Player");
    // step 1: create root node (add_do_method)
    // step 2: create collision child (add_do_method)
    // step 3: create visual child (add_do_method)
    // step 4: set properties (add_do_property)
    // ... all in one action
    undo_redo->commit_action();
    return summary_result;
}
```

This is fundamentally different from how Node.js/Python MCP servers handle composition -- they must make multiple WebSocket round-trips, each creating a separate undo entry. Our single-action-commit is a genuine architectural advantage.

**Recommended composite tools (v1.5 scope):**

1. `create_character` -- CharacterBody2D/3D + CollisionShape + visual + optional starter script
2. `create_ui_panel` -- Declarative UI layout from JSON spec (container type, children, styling)
3. `batch_set_property` -- Set property on multiple nodes by path list or type filter
4. `find_nodes` -- Search scene tree by type, name pattern, property value, or script attachment
5. `duplicate_node` -- Deep-copy node subtree to new parent with rename

---

## Feature Area 2: Enriched MCP Resources

Richer scene context exposed through MCP Resources protocol, enabling AI to auto-acquire context without explicit tool calls.

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Scene tree with script content | Current scene://tree resource returns node names/types/paths but NOT the script source code. AI must follow up with read_script for every scripted node. Enriched resource includes script source inline. | LOW | get_scene_tree + read_script (combine) |
| Signal connection map | Current resources don't expose signal connections at all. AI cannot understand the wiring between nodes without calling get_node_signals per node. Resource should return a complete signal graph. | MEDIUM | get_node_signals (aggregate across tree) |
| Node property details | Current scene tree shows basic info (type, path, has_script). Missing: actual property values that matter for context (position, size, collision_layer, visible, custom exports). | MEDIUM | get_scene_tree with expanded property serialization |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Resource templates (URI parameterization) | MCP spec supports `resources/templates/list` with RFC 6570 URI templates. Instead of 2 fixed resources, expose templates like `godot://node/{path}` (single node detail), `godot://script/{path}` (script content), `godot://signals/{path}` (signal map for subtree). AI clients can request exactly the context they need. | MEDIUM | New MCP protocol handler for resources/templates/list |
| Scene diff resource | A resource that returns "what changed since last read" -- delta between current scene state and a cached snapshot. Drastically reduces context window usage on re-reads. | HIGH | Requires snapshot caching, diff computation |
| Project structure with metadata | Current project://files returns a flat file list. Enriched version includes: file sizes, last modified timestamps, file type classification (scene/script/resource/image), and dependency relationships between files. | MEDIUM | list_project_files with metadata expansion |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Auto-attaching all resources to every request | Dumping the full scene tree + all scripts + all signals into every AI context window wastes tokens and degrades AI performance. Resources should be pulled on demand, not pushed. | Use resource annotations with priority hints. Let the AI client decide what to pull. |
| Resource subscriptions with push notifications | MCP spec supports `resources/subscribe` and `notifications/resources/updated`, but implementing real-time scene change tracking adds significant complexity for minimal benefit in editor-tool workflows. | Defer subscriptions to v2+. Polling via resources/read is sufficient for v1.5. |
| Binary resource content (images, audio) | Exposing textures/audio as MCP resources bloats the protocol and wastes context. AI can't meaningfully process raw image bytes in a resource context. | Keep viewport capture as a tool (returns ImageContent). Resources stay text/JSON. |

### Implementation Notes

**Resource template URI scheme:**
```
godot://scene_tree                     -- Full scene tree (existing, enriched)
godot://project_files                  -- Project file listing (existing, enriched)
godot://node/{node_path}               -- Single node detail with properties + script
godot://signals/{node_path}            -- Signal connections for node subtree
godot://script/{script_path}           -- Script source code as resource
```

**Enriched scene_tree response structure:**
```json
{
  "name": "Player",
  "type": "CharacterBody2D",
  "path": "Player",
  "properties": {
    "position": "Vector2(100, 200)",
    "collision_layer": 1,
    "collision_mask": 1
  },
  "script": {
    "path": "res://scripts/player.gd",
    "source": "extends CharacterBody2D\n...",
    "line_count": 45
  },
  "signals": {
    "outgoing": [{"signal": "health_changed", "target": "HUD", "method": "_on_health_changed"}],
    "incoming": [{"source": "Enemy", "signal": "attack", "method": "_on_enemy_attack"}]
  },
  "children": [...]
}
```

**Important constraint:** Enriched resources are READ-ONLY. They must never modify scene state. The content should be JSON-serializable and stay under reasonable size limits. For large scenes (500+ nodes), depth-limiting or pagination is essential.

---

## Feature Area 3: Smart Error Handling

Transform error responses from dead-ends into recovery guidance. Errors include diagnostic context, common causes, and specific fix suggestions that enable AI self-correction.

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Structured error responses with isError flag | MCP spec defines `isError: true` in tool results, which gets injected into AI context window (unlike JSON-RPC errors which are protocol-level). Current code returns `{"error": "message"}` as regular results without isError flag. Must switch to proper MCP error format. | LOW | mcp_protocol.cpp create_tool_result modification |
| "Node not found" with similar node suggestions | Most common error. Currently returns bare "Node not found: Player/Sprit2D". Should suggest: "Did you mean 'Player/Sprite2D'? Available children of 'Player': Sprite2D, CollisionShape2D, AnimationPlayer". | MEDIUM | Scene tree traversal for fuzzy matching |
| "No scene open" with actionable guidance | Currently returns bare "No scene open". Should include: "No scene is currently open in the editor. Use create_scene to create a new scene, or open_scene to open an existing one." | LOW | Static message enhancement |
| Missing parameter errors with examples | Currently returns "Missing required parameter: node_path". Should include: "Missing required parameter: node_path. Expected a path like 'Player' or 'Player/Sprite2D'. Use get_scene_tree to see available node paths." | LOW | Static message enhancement per tool |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Tool ordering guidance in errors | When a tool fails because of prerequisite state (e.g., inject_input before run_game), the error tells the AI what to do first: "Game is not running. Call run_game first, then retry inject_input." This is the #1 MCP error pattern from industry research. | LOW | Conditional error messages based on server state |
| Property validation with type hints | When set_node_property fails, explain what type was expected: "Property 'position' on Node2D expects Vector2. Received: '100'. Use format: 'Vector2(100, 200)'." Include the valid format for common Godot types. | MEDIUM | variant_parser error path enhancement |
| Error response with tool suggestions | Each error includes a `suggested_tools` field listing which tools could help recover: `["get_scene_tree", "create_node"]`. AI can use this to plan its recovery without guessing. | LOW | Static mapping per error type |
| Script validation errors with line context | When attach_script fails due to parse errors, include the specific GDScript error and the offending line, not just "Script has errors". | MEDIUM | GDScript error extraction from Godot engine |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Auto-retry logic inside the server | Tempting to have the server automatically retry failed operations, but this violates MCP's model-controlled principle. The AI should decide whether and how to retry. | Return rich error context; let the AI decide the recovery strategy. |
| Error logging to external services | Adding telemetry/logging infrastructure adds complexity and privacy concerns. Not appropriate for an open-source editor plugin. | Log to Godot's output panel (already done via UtilityFunctions::printerr). |
| Overly verbose error messages | Returning 500-word error explanations wastes AI context window. Errors should be concise but actionable. | Target 2-4 sentences per error: what went wrong, why, and what to do. |

### Implementation Notes

**Error response structure (MCP-compliant):**
```json
{
  "content": [{
    "type": "text",
    "text": "Node not found: 'Player/Sprit2D'. Did you mean 'Player/Sprite2D'? Children of 'Player': Sprite2D, CollisionShape2D, AnimationPlayer."
  }],
  "isError": true
}
```

**Current vs. proposed error flow:**

Current:
```cpp
return {{"error", "Node not found: " + node_path}};
// Wrapped by create_tool_result -> content[{type:text, text:...}], isError NOT set
```

Proposed:
```cpp
// New helper: create_error_tool_result with isError: true
return mcp::create_tool_error_result(id,
    "Node not found: '" + node_path + "'. " + suggest_similar_nodes(scene_root, node_path),
    {"get_scene_tree"}  // suggested recovery tools
);
```

**Error categories and their enhancement plan:**

| Error Category | Current Message | Enhanced Message Pattern |
|----------------|----------------|--------------------------|
| Node not found | "Node not found: X" | "Node not found: 'X'. Did you mean 'Y'? Children of parent: [list]. Use get_scene_tree to browse." |
| No scene open | "No scene open" | "No scene open. Use create_scene or open_scene first." |
| Unknown class | "Unknown class: X" | "Unknown class: 'X'. Did you mean 'Y'? Common node types: Node2D, Sprite2D, CharacterBody2D, Control, Label." |
| Game not running | "Game bridge not initialized" | "Game is not running. Call run_game first. The game bridge connects automatically when the game starts." |
| Missing parameter | "Missing required parameter: X" | "Missing required parameter: 'X'. Example: {example_value}. See tool description for format." |
| Property type mismatch | (crashes or silent fail) | "Cannot set 'position' to 'hello'. Expected Vector2 format: 'Vector2(x, y)'. Example: 'Vector2(100, 200)'." |
| Script parse error | "Script has errors" | "Script 'res://x.gd' has parse errors: Line 15: 'Expected expression'. Source: '  var x = '." |

---

## Feature Area 4: Expanded Prompt Templates

Workflow-oriented prompts that guide AI through multi-step tool composition. Prompts encode domain expertise about Godot game development patterns.

### Table Stakes

| Feature | Why Expected | Complexity | Dependencies |
|---------|--------------|------------|-------------|
| Platformer game setup prompt | Complete workflow: create scene structure, player with collision, ground tilemap, camera, HUD. References actual tool names and parameter formats. Existing `setup_scene_structure` prompt only DESCRIBES the structure; this one tells AI exactly which tools to call. | LOW | Existing prompt infrastructure |
| Debug game issue prompt | Systematic debugging workflow: get scene tree, check node properties, run game, capture output, analyze errors. Users frequently ask AI "why doesn't my game work?" and the AI needs a structured debugging approach. | LOW | Existing prompt infrastructure |
| TileMap level design prompt | Workflow for creating TileMap-based levels: set up TileMapLayer, configure tile source, place tiles in patterns, add collision layers. TileMap tools (v1.4) exist but AI often struggles with the multi-step setup. | LOW | Existing prompt infrastructure |
| 2D game from scratch prompt | End-to-end workflow: create scene, build player, create level, add enemies, set up HUD, wire signals, run and test. The "ultimate" getting-started prompt. | LOW | Existing prompt infrastructure |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Error recovery prompts | Workflow for when things go wrong: "The game crashes on start -- here's how to diagnose using MCP tools: step 1 check get_game_output, step 2 validate scripts, step 3 check scene tree integrity." No competitor offers diagnostic workflow prompts. | LOW | Existing prompt infrastructure |
| Multi-prompt argument system | Prompts that accept parameters for customization: `create_game(genre="platformer", dimension="2d", complexity="simple")` generates a tailored workflow. Existing prompts already support arguments but only use 1-2 parameters. Expanding to multi-dimensional customization. | LOW | Existing argument substitution system |
| Tool composition cheat sheet prompt | A meta-prompt that teaches the AI which tools to combine for common tasks. Not a step-by-step workflow but a reference card: "To create a character: create_node -> create_collision_shape -> write_script -> attach_script -> set_node_property". Reduces AI trial-and-error. | LOW | Static text, no tools needed |

### Anti-Features

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Prompts that embed full GDScript code | Shipping complete game scripts inside prompts makes them huge, brittle, and version-specific. AI should write scripts based on context, not copy-paste. | Prompts describe WHAT the script should do ("movement with SPEED and JUMP_VELOCITY"), not provide the full source. |
| Too many prompts (20+) | MCP clients display prompts in lists. 20+ prompts create decision paralysis. Better to have 10-12 high-quality prompts covering common workflows. | Cap at 12-15 prompts total (7 existing + 5-8 new). |
| Prompts that duplicate tool descriptions | Prompts like "How to use create_node" just repeat the tool schema. Worthless -- AI already sees tool descriptions. | Prompts describe WORKFLOWS (multi-tool sequences), not individual tools. |

### Implementation Notes

**Recommended new prompts (5-8 for v1.5):**

1. `build_platformer_game` -- End-to-end 2D platformer setup workflow
2. `build_top_down_game` -- End-to-end top-down game setup workflow
3. `debug_game_crash` -- Systematic debugging when game fails to run
4. `debug_physics_issue` -- Physics-specific debugging (extends existing `debug_physics`)
5. `setup_tilemap_level` -- TileMap level creation workflow
6. `tool_composition_guide` -- Reference card for common tool combinations
7. `fix_common_errors` -- Guide for recovering from common MCP tool errors
8. `create_game_from_scratch` -- Full game creation workflow (parameterized by genre)

**Prompt template pattern (following existing codebase convention):**
```cpp
{
    "prompt_name",
    "Human-readable description",
    nlohmann::json::array({
        {{"name", "param1"}, {"description", "..."}, {"required", true}},
        {{"name", "param2"}, {"description", "..."}, {"required", false}}
    }),
    [](const nlohmann::json& args) -> nlohmann::json {
        // Generate messages array with tool-aware guidance
        return nlohmann::json::array({
            {{"role", "user"}, {"content", {{"type", "text"}, {"text", workflow_text}}}}
        });
    }
}
```

---

## Feature Dependencies

```
[Smart Error Handling]
    (no dependencies -- can be implemented first, improves all other features)

[Enriched MCP Resources]
    requires: existing get_scene_tree, read_script, get_node_signals
    (read-only, no mutation -- safe to implement independently)

[Composite Tools]
    requires: existing primitive tools (create_node, create_collision_shape, etc.)
    benefits from: Smart Error Handling (composite failures are more complex to diagnose)
    benefits from: Enriched Resources (AI can verify composite results)

[Expanded Prompt Templates]
    requires: knowledge of all existing tools + new composite tools
    benefits from: Smart Error Handling (prompts can reference error recovery)
    should be LAST (prompts reference tools that must exist first)
```

### Dependency Notes

- **Smart Error Handling has no dependencies** and improves ALL other features. Implement first.
- **Enriched Resources and Composite Tools are independent** of each other and can be built in parallel.
- **Prompt Templates should be last** because they reference tools (including new composite tools) and error recovery patterns. Writing prompts before the tools exist leads to stale/incorrect guidance.
- **Composite Tools benefit from Smart Errors** because multi-step operations have more failure modes. Having good error messages during composite tool development accelerates iteration.

---

## MVP Definition

### Launch With (v1.5)

- [x] Smart error handling with isError flag, node-not-found suggestions, prerequisite guidance -- highest ROI, lowest cost
- [x] `find_nodes` composite tool -- enables batch workflows, unblocks batch_set_property
- [x] `batch_set_property` composite tool -- most-requested missing feature vs competitors
- [x] `create_character` composite tool -- highest-visibility feature for new users
- [x] Enriched scene_tree resource (properties + scripts + signals inline) -- biggest context improvement
- [x] 4-5 new prompt templates (platformer, debug crash, tilemap, tool guide) -- low cost, high discoverability

### Add After Validation (v1.5.x)

- [ ] `create_ui_panel` composite tool -- needed but complex to design the input schema well
- [ ] Resource templates (URI parameterization) -- valuable but requires protocol-level changes
- [ ] `duplicate_node` composite tool -- useful but less common workflow
- [ ] Project structure resource with metadata -- nice-to-have for large projects

### Future Consideration (v2+)

- [ ] Create complete scene from template -- very high complexity, needs careful template design
- [ ] Scene diff resource -- high complexity, requires snapshot infrastructure
- [ ] Resource subscriptions -- spec support exists but marginal value for editor workflows

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Smart error handling (isError + suggestions) | HIGH | LOW | P1 |
| find_nodes (search by type/name) | HIGH | LOW | P1 |
| batch_set_property | HIGH | LOW | P1 |
| Enriched scene_tree resource | HIGH | MEDIUM | P1 |
| create_character composite | HIGH | MEDIUM | P1 |
| New prompt templates (5-8) | MEDIUM | LOW | P1 |
| create_ui_panel composite | MEDIUM | MEDIUM | P2 |
| Resource templates (URI params) | MEDIUM | MEDIUM | P2 |
| duplicate_node composite | MEDIUM | MEDIUM | P2 |
| Project metadata resource | LOW | MEDIUM | P2 |
| Scene from template composite | MEDIUM | HIGH | P3 |
| Scene diff resource | LOW | HIGH | P3 |

**Priority key:**
- P1: Must have for v1.5 launch. Delivers the core "intelligence upgrade" promise.
- P2: Should have, add in v1.5.x patches. Valuable but not essential for launch.
- P3: Nice to have, future consideration. High complexity or uncertain value.

---

## Competitor Feature Analysis

| Feature | Godot MCP Pro | Better Godot MCP | GDAI MCP | Our Approach (v1.5) |
|---------|---------------|------------------|----------|---------------------|
| Composite/batch tools | batch_set_property, batch_get_properties, cross_scene_set_property | 18 mega-tools (one tool per domain) | No explicit composites | Focused composites: create_character, batch_set_property, find_nodes. Single UndoRedo action (unique advantage). |
| Rich context resources | No MCP Resources (tools only) | help tool for on-demand docs | Auto-screenshot for visual context | MCP Resources with inline scripts, signals, properties. Standards-compliant, auto-context capable. |
| Smart errors | validate_script, get_editor_errors (separate tools) | Compressed descriptions for token savings | Reads parse errors + script errors | isError flag, fuzzy node matching, prerequisite guidance, suggested tools. Errors AS context (MCP best practice). |
| Prompt templates | Unknown | Unknown | Custom prompt for GDScript | 7 existing + 5-8 new workflow prompts. Tool-composition guidance, genre-specific game setup. |
| Search/discovery | find_nodes_by_type, find_signal_connections, find_script_references, detect_circular_dependencies | N/A | Search files by res:// | find_nodes with type + name + property filters. Signal map in enriched resources. |

**Our unique advantages:**
1. **Single UndoRedo action** for composite tools -- no competitor can match this (they use WebSocket round-trips)
2. **MCP Resources protocol** for auto-context -- competitors use tools, we use the proper MCP primitive
3. **isError flag** error handling -- follows MCP spec best practices for AI self-correction
4. **Zero-dependency GDExtension** -- no Node.js/Python runtime needed

---

## Sources

- [MCP Specification 2025-06-18: Tools](https://modelcontextprotocol.io/specification/2025-06-18/server/tools) -- isError flag, structured content, tool annotations (HIGH confidence)
- [MCP Specification 2025-06-18: Resources](https://modelcontextprotocol.io/specification/2025-06-18/server/resources) -- Resource templates, URI schemes, annotations, subscriptions (HIGH confidence)
- [54 Patterns for Building Better MCP Tools](https://arcade.dev/blog/mcp-tool-patterns) -- Composite tool patterns, error-guided recovery, composition principles (MEDIUM confidence)
- [Better MCP Tool Call Error Responses](https://dev.to/alpic/better-mcp-toolscall-error-responses-help-your-ai-recover-gracefully-15c7) -- isError vs JSON-RPC errors, tool ordering guidance, validation messages, fallback instructions (MEDIUM confidence)
- [Godot MCP Pro (162 tools)](https://github.com/youichi-uda/godot-mcp-pro) -- batch_set_property, find_nodes_by_type, validate_script, permission model (HIGH confidence)
- [Better Godot MCP (18 mega-tools)](https://github.com/n24q02m/better-godot-mcp) -- Composite tool architecture, token-efficient descriptions, help tool pattern (MEDIUM confidence)
- [GDAI MCP Server](https://gdaimcp.com/) -- Debug loop with auto-screenshot, visual feedback workflow (MEDIUM confidence)
- [MCP Prompts for Workflow Automation](https://zuplo.com/blog/mcp-server-prompts) -- Prompt design patterns, multi-step workflow guidance, tool composition prompts (MEDIUM confidence)
- Existing codebase review: mcp_server.cpp, mcp_prompts.cpp, mcp_tool_registry.cpp, scene_mutation.cpp, physics_tools.cpp (HIGH confidence)

---
*Feature research for: Godot MCP Meow v1.5 AI Workflow Enhancement*
*Researched: 2026-03-23*
