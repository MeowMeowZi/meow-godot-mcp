# Pitfalls Research

**Domain:** v1.5 AI Workflow Enhancement -- Composite Tools, Enriched Resources, Smart Error Handling, Expanded Prompts
**Project:** Godot MCP Meow (C++ GDExtension, godot-cpp v10+)
**Researched:** 2026-03-23
**Confidence:** HIGH (based on codebase analysis, MCP ecosystem evidence, prior v1.0-v1.4 experience)

**Context:** Adding workflow enhancement features to an existing 50-tool MCP server. The codebase uses a monolithic `handle_request()` dispatcher (~750 lines of if/else chains), a static `ToolDef` registry, hardcoded prompt templates in C++ lambdas, and a two-thread architecture (IO thread + main thread queue). All four v1.5 features interact with this existing structure and with each other.

---

## Critical Pitfalls

### Pitfall 1: Composite Tool Logic Duplication Creates Divergent Behavior

**What goes wrong:**
Composite tools (e.g., `create_character` that creates a CharacterBody2D + CollisionShape + Sprite + Script) reimplement the logic of existing atomic tools (`create_node`, `attach_script`, `create_collision_shape`) rather than calling them internally. Over time, the atomic tools get bug fixes or behavior changes (e.g., improved path normalization, new property parsing), but the composite tool's copy of that logic does not get updated. The same operation produces different results depending on whether the AI calls the composite tool or chains the atomic tools.

**Why it happens:**
The current architecture makes reuse structurally difficult. Each tool's logic lives in a standalone C++ function (e.g., `create_node()` in `scene_mutation.cpp`) that returns `nlohmann::json`. The `handle_request()` method in `mcp_server.cpp` extracts parameters, calls the function, and wraps the result. A composite tool cannot easily "call" `create_node()` then "call" `attach_script()` because the parameter extraction and error-response wrapping logic is tangled into the dispatcher. The natural shortcut is to copy-paste the underlying Godot API calls into the composite tool function.

**How to avoid:**
- Factor each atomic tool into a reusable internal function that takes typed C++ parameters (not JSON) and returns a structured result. The JSON parameter extraction and response wrapping stay in the dispatcher layer. Composite tools call the internal functions directly.
- Concretely: `create_node_internal(type, parent_path, name, properties, undo_redo) -> CreateNodeResult` with a thin JSON adapter in the dispatcher. Composite tools call `create_node_internal()` directly.
- Alternatively, composite tools can literally invoke the existing JSON-level functions and deserialize the result, but this is less efficient and harder to test.

**Warning signs:**
- A composite tool file includes the same `#include` headers as the atomic tool files it replicates.
- A composite tool function contains `ClassDB::instantiate()` or `get_node_or_null()` calls that also exist in atomic tool functions.
- Bug fix applied to an atomic tool does not appear in the composite tool.

**Phase to address:**
First phase -- composite tools must be designed around reusable internals from the start. Retrofitting reuse after composite tools are written duplicated is costly.

---

### Pitfall 2: Composite Tool Partial Failure Without Rollback Leaves Scene in Broken State

**What goes wrong:**
A composite tool like `create_ui_panel` performs 5 steps: create PanelContainer, set layout preset, create VBoxContainer child, create Label child, apply StyleBox. If step 4 succeeds but step 5 fails (e.g., invalid StyleBox parameter), the scene now has a partially constructed UI panel. The AI receives an error, but 4 nodes have already been created. The AI does not know which steps succeeded, so it cannot clean up. Retrying the entire composite tool creates duplicates.

**Why it happens:**
The current codebase wraps each atomic operation in its own UndoRedo action (`undo_redo->create_action()` ... `commit_action()` per operation). If a composite tool calls 5 atomic operations, each commits its own action. There is no transaction boundary spanning the entire composite operation. Godot's UndoRedo system supports grouping (by calling `create_action` once and `commit_action` once), but this requires all sub-operations to participate in the same action scope.

**How to avoid:**
- Wrap the entire composite tool execution in a single UndoRedo action. All sub-operations add their do/undo methods to this one action. If any sub-operation fails, call `undo_redo->commit_action()` on what was accumulated so far, then immediately `undo_redo->undo()` to roll back everything. Return the error with a clear message: "Step 4 of 5 failed (Label creation): [reason]. All changes rolled back."
- Validate all inputs upfront before performing any mutations. Check that all class names are valid, all parent paths exist, all required parameters are present. This catches most failures before any scene modification occurs.
- Return a structured result showing which steps succeeded and which failed, even in the rollback case, so the AI can diagnose the issue.

**Warning signs:**
- Composite tool error responses say "Error: invalid property" without indicating which of the N sub-steps failed.
- After a composite tool error, `get_scene_tree` shows partially created nodes.
- AI retries a failed composite tool and creates duplicate nodes.

**Phase to address:**
Composite tools phase. The UndoRedo transaction boundary design must be established before implementing any composite tool.

---

### Pitfall 3: Tool Count Explosion Overwhelms AI Context Window

**What goes wrong:**
The server currently has 50 tools. Adding composite tools on top of the existing atomic tools pushes the total toward 60-70+. Each tool definition (name + description + JSON schema) consumes 500-2000 tokens. At 60 tools, the `tools/list` response alone can consume 30,000-120,000 tokens. This is the well-documented "MCP context tax" problem: industry data shows that 40+ tools across 3 MCP servers consumed 143,000 of 200,000 available context tokens, leaving almost nothing for actual reasoning. Perplexity dropped MCP internally in March 2026, citing 72% context window waste from tool definitions.

**Why it happens:**
The instinct is to add composite tools alongside atomic tools -- "the AI can choose whichever granularity it needs." But AI models degrade with large tool counts: they confuse similar tools, pick wrong ones more often, and waste tokens on selection. The existing tool descriptions are already verbose (see `create_collision_shape` at ~400 bytes of description + schema).

**How to avoid:**
- Set a hard budget: the total `tools/list` payload must stay under 40KB of JSON (approximately 50 tools with moderate descriptions). Measure this after each tool addition.
- Do NOT add composite tools as additional tools on top of all existing atomic tools. Instead, composite tools should replace common multi-step sequences. If `create_character` is added, consider whether `create_collision_shape` can be removed (since `create_character` handles it internally, and standalone collision shapes can still be created via `create_node` + `set_node_property`).
- Use concise descriptions. The current `run_test_sequence` description is 342 characters. The current `inject_input` schema has 7 conditional parameters with verbose descriptions. These should be trimmed.
- Consider the MCP spec's tool annotations (`readOnlyHint`, `destructiveHint`) added in the 2025-06-18 spec update. These help AI clients prioritize tools without reading full descriptions.

**Warning signs:**
- The `tools/list` JSON response exceeds 40KB.
- AI models start confusing `create_node` with `create_character` or using the wrong one for simple tasks.
- AI models take 2+ turns to select the right tool for a straightforward request.
- Response latency increases noticeably due to larger prompts.

**Phase to address:**
Composite tools phase. Tool budget must be decided before any implementation. Every new composite tool should specify which atomic tool it makes less necessary.

---

### Pitfall 4: Enriched Resources Return Unbounded Data That Exceeds Practical Limits

**What goes wrong:**
The v1.5 plan calls for enriched MCP Resources that include script content, signal connection graphs, and node property details. A moderate Godot scene has 50-200 nodes. Including full script content (each script 50-500 lines), all signal connections, and detailed properties for every node produces a Resource response of 50,000-500,000+ tokens. This exceeds what any AI client can usefully consume. The AI either truncates the data (losing important information) or its reasoning degrades under the weight of irrelevant context.

**Why it happens:**
The current `godot://scene_tree` resource returns a compact tree (node name, type, path, basic properties). Developers enriching this resource think "more information is always better for the AI." But AI models have bounded attention: beyond ~10,000 tokens of context data, additional information degrades rather than improves performance. A 200-node scene tree with full script content is anti-helpful.

**How to avoid:**
- Use parameterized Resource URIs instead of monolithic endpoints. Instead of one `godot://scene_tree` that returns everything, offer:
  - `godot://scene/tree` -- compact tree structure (current behavior, the default)
  - `godot://scene/node/{path}` -- detailed info for ONE node (properties, script, signals)
  - `godot://scene/signals` -- signal connection graph only (compact)
  - `godot://scene/scripts` -- list of scripts with paths (not content, just metadata)
- Enforce a response size budget per resource. If the computed response exceeds 8KB of JSON, truncate with a `"truncated": true` flag and a `"hint": "Use godot://scene/node/Player for details"` message.
- Never include full script source code in Resource responses. Return the script path and line count. The AI can use the existing `read_script` tool to fetch content for specific scripts it needs.

**Warning signs:**
- Resource response JSON exceeds 10KB.
- AI clients start truncating Resource data or ignoring it entirely.
- AI asks for scene tree context but then fails to use the information in the response because it is too large.

**Phase to address:**
Resources enrichment phase. URI design and size budget must be established before implementation.

---

### Pitfall 5: Prompt Templates Hardcode Tool Names That Drift from Actual Tool Registry

**What goes wrong:**
The current prompt templates (7 templates in `mcp_prompts.cpp`) contain hardcoded tool names and parameter examples like `Tool: create_node\n Parameters: { "type": "Control", "name": "MainMenu" }`. If a tool is renamed (e.g., `create_node` -> `add_node`), has its parameters changed (e.g., `parent` -> `parent_path`), or is superseded by a composite tool (e.g., `create_character` replaces the sequence of `create_node` + `create_collision_shape` + `attach_script`), the prompt templates silently become incorrect. The AI follows the prompt template instructions, calls a tool name that no longer exists or uses wrong parameter names, and gets an error.

**Why it happens:**
The prompt templates are static C++ string literals compiled into the DLL. They reference tool names and schemas by value, not by reference. There is no build-time or runtime check that the tool names mentioned in prompts actually exist in the tool registry. The current prompts were written for the v1.0-v1.1 tool surface; v1.5 changes the tool surface but the prompt templates are a separate file that is easy to forget.

**How to avoid:**
- Add a compile-time or startup-time validation step that checks every tool name referenced in prompt templates against the actual tool registry. Concretely: in `mcp_prompts.cpp`, maintain a `static const std::vector<std::string> REFERENCED_TOOLS` per prompt, and at startup (or in unit tests), verify each name exists in `get_all_tools()`.
- When adding composite tools, update ALL prompt templates that reference the atomic tools being composed. Grep for the atomic tool name across `mcp_prompts.cpp`.
- Consider generating prompt text fragments from the tool registry rather than hardcoding them. E.g., `get_tool_description("create_node")` pulls the description from the registry, ensuring consistency.
- Write a unit test: for each prompt, extract all `Tool: <name>` references and assert they exist in the registry.

**Warning signs:**
- A prompt template mentions a tool name not present in `get_all_tools()`.
- AI follows a prompt template and gets "Tool not found" errors.
- Prompt template references parameter names that differ from the tool's actual `inputSchema`.

**Phase to address:**
Prompt expansion phase, but the validation mechanism should be built in the first phase since composite tools will change the tool surface.

---

## Moderate Pitfalls

### Pitfall 6: Smart Error Handling Over-Contextualizes and Leaks Internal Architecture

**What goes wrong:**
In the pursuit of "actionable" error messages, developers include internal details like C++ class names, file paths, line numbers, or Godot engine internals. Examples:
- "Failed at scene_mutation.cpp:52 -- ClassDB::instantiate returned null for 'CharacterBody2d'" (leaks internal file structure)
- "Node not found at NodePath('Player/Sprite2D'). The scene root is 'Main' (Node2D). Available children: World, Player, UI, Camera2D. Player has children: CollisionShape2D, AnimatedSprite2D. Did you mean 'Player/AnimatedSprite2D'?" (leaks entire scene structure in every error)

The second example is too helpful -- it dumps the entire node tree into every error response, which the AI may not need and which inflates context for no benefit.

**How to avoid:**
- Error messages should contain three components, no more:
  1. **What failed:** "Node not found: Player/Sprite2D"
  2. **Likely cause:** "No child named 'Sprite2D' exists under 'Player'"
  3. **Suggested fix:** "Check node name spelling. Use get_scene_tree to see available nodes."
- Never include C++ source locations, internal variable names, or Godot engine error codes.
- Do not dump sibling nodes or tree structure in error messages. Instead, suggest the AI use `get_scene_tree` to discover the correct path. This keeps error messages small and teaches the AI the right workflow.
- Budget error messages to under 200 characters of actionable content.

**Warning signs:**
- Error messages contain `::`, `.cpp`, `.h`, or memory addresses.
- Error messages exceed 500 characters.
- The same node tree information appears in both error responses AND resource responses.

**Phase to address:**
Smart error handling phase.

---

### Pitfall 7: Error Suggestions Create Infinite Retry Loops

**What goes wrong:**
Smart error messages that say "try X instead" can cause the AI to blindly follow suggestions in an infinite loop. Example: Error: "Node 'Playr' not found. Did you mean 'Player'?" -- AI retries with 'Player'. But if 'Player' also fails for a different reason (e.g., wrong scene), the error says "Node 'Player' not found. Did you mean 'PlayerCharacter'?" -- AI retries with 'PlayerCharacter'. This continues until the AI exhausts its retry budget.

**Why it happens:**
Error messages with "did you mean X?" suggestions assume the alternative will succeed. But fuzzy matching on node names or tool parameters can chain through multiple wrong suggestions. Each suggestion is contextually plausible but factually wrong.

**How to avoid:**
- Never suggest a specific alternative unless you have verified it exists. "Did you mean 'Player'?" is only valid if a node named 'Player' actually exists in the current scene tree.
- Prefer generic guidance over specific alternatives: "Node not found. Use get_scene_tree to find the correct path." This breaks the retry loop by redirecting the AI to a discovery tool.
- Add a `"suggestion_type"` field to error responses: `"verified"` (the suggestion is known-valid) vs `"hint"` (the suggestion is a guess). AIs can treat these differently.
- For property name errors, match against the node's actual property list. Return verified alternatives only.

**Warning signs:**
- AI tool call logs show 3+ sequential calls to the same tool with slightly different parameters, each returning an error with a different suggestion.
- Error messages contain "Did you mean X?" where X is also invalid.

**Phase to address:**
Smart error handling phase.

---

### Pitfall 8: `handle_request()` Grows Into an Unmaintainable 1500-Line Function

**What goes wrong:**
The current `handle_request()` in `mcp_server.cpp` is ~750 lines of sequential if/else blocks, one per tool. Adding composite tools, enriched resource URIs, and expanded error handling logic will push this toward 1200-1500 lines. At this scale, the function becomes:
- Impossible to navigate (scrolling through 50+ if-blocks to find one tool).
- Prone to copy-paste errors (each block follows the same pattern but with subtle variations).
- A merge conflict magnet (any two developers adding tools will conflict in the same file/function).

**Why it happens:**
The pattern was simple and correct for 10 tools. It scaled reasonably to 30. At 50+ tools with enriched error handling per tool, it is past the tipping point. But developers keep appending to the existing pattern because it works.

**How to avoid:**
- Refactor tool dispatch into a registry-based pattern BEFORE adding v1.5 tools. Instead of if/else chains, use a `std::unordered_map<std::string, std::function<json(const json&, const json&)>>` that maps tool names to handler functions.
- Each tool handler is a standalone function (which they already are -- `create_node()`, `set_node_property()`, etc.). The registry eliminates the dispatcher boilerplate.
- This also enables composite tools to be registered alongside atomic tools with no structural difference.
- If refactoring the dispatcher is too costly for v1.5, at minimum extract the parameter-extraction + error-checking boilerplate into a helper. Currently each tool block has ~5 lines of `get_string(args, "x")` followed by `if (x.empty()) return error`. This can be a validation helper: `auto [args, err] = validate_tool_args(params, {"node_path:required", "property:required", "value:required"})`.

**Warning signs:**
- `mcp_server.cpp` exceeds 1200 lines.
- Adding a new tool requires scrolling past 40+ existing tool blocks.
- Two PRs modifying different tools conflict in `handle_request()`.

**Phase to address:**
First phase. Refactoring the dispatcher before adding tools is much cheaper than after.

---

### Pitfall 9: Enriched Resources Become Slow Due to Scene Tree Traversal on Every Request

**What goes wrong:**
Enriched resources that include signal connections, script content, and detailed properties must traverse the entire scene tree and perform property queries on every node. For a 200-node scene, this involves 200+ `get()` calls, 200+ signal list queries, and potentially 20-50 `FileAccess` reads for script content. If the AI client polls resources frequently (some clients auto-refresh resources), this creates noticeable editor lag.

**Why it happens:**
The current `get_scene_tree()` function is lightweight -- it walks the tree and collects name/type/path per node. Enriching it with properties, signals, and scripts multiplies the work per node by 10-50x. There is no caching layer; every resource request recomputes everything from scratch.

**How to avoid:**
- Use a tiered response strategy: the default resource returns the compact tree (current behavior). Detailed information is only fetched on demand via parameterized URIs (`godot://scene/node/Player`).
- Add a simple cache with a generation counter: increment the counter on any scene mutation (node create/delete/property change). Resource handlers check the counter and return cached data if unchanged. The UndoRedo system provides a natural mutation detection point.
- Set a hard timeout: if scene tree traversal takes longer than 100ms, truncate and return what has been collected with a `"truncated": true` flag.
- Never read script file content in a resource handler. Return script path only. Let the AI use `read_script` explicitly.

**Warning signs:**
- Editor stutters or freezes when AI client is connected and actively querying resources.
- Resource response latency exceeds 200ms.
- CPU usage spikes every time the AI client refreshes resources.

**Phase to address:**
Resources enrichment phase.

---

### Pitfall 10: New Prompt Templates Become Stale as Tool Surface Evolves Post-v1.5

**What goes wrong:**
V1.5 adds new prompt templates referencing the v1.5 tool surface. In v1.6 or v2.0, tools are renamed, removed, or restructured. The prompt templates become stale again -- the same problem as Pitfall 5, but now at a larger scale (more templates, more tool references). This is a recurring maintenance debt pattern, not a one-time fix.

**Why it happens:**
Prompt templates are prose text that references tool names. There is no structural coupling between the template text and the tool registry. Every tool surface change requires manually auditing all prompt templates. As the number of templates grows (currently 7, v1.5 may add 5-10 more), the audit burden grows quadratically.

**How to avoid:**
- Build prompt templates from composable fragments, not monolithic strings. Each "step" in a workflow template should be generated from a tool reference:
  ```cpp
  std::string step = make_tool_step("create_node", {{"type", "CharacterBody2D"}, {"name", "Player"}});
  ```
  `make_tool_step()` looks up the tool in the registry, validates parameters against the schema, and generates the step text. If the tool does not exist, it fails at compile-time or startup.
- Alternatively, keep the current string-literal approach but add a mandatory unit test: parse every prompt template's output, extract all tool name references, and assert each exists in `get_all_tools()`. Run this test in CI.
- Consider whether prompt templates even belong in compiled C++ code. An alternative is to ship them as `.json` or `.md` files in the addon directory, making them editable without recompilation. The C++ code loads them at startup.

**Warning signs:**
- More than 10 prompt templates exist with no automated validation.
- A prompt template references a tool that was renamed 2 releases ago.
- Users report "prompt template told me to use X but X does not exist."

**Phase to address:**
Prompt expansion phase.

---

### Pitfall 11: Composite Tools with Too Many Parameters Become Unusable

**What goes wrong:**
A composite tool like `create_character` tries to expose every option from every sub-operation: character type (2D/3D), body class, collision shape type, shape dimensions, sprite texture, script template, movement speed, gravity, collision layers, collision masks, node names... The resulting parameter schema has 15-25 fields. AI models struggle with schemas this large -- they omit required fields, confuse similar parameters, and waste tokens filling in defaults.

**Why it happens:**
The composite tool is meant to be a "power tool" that replaces 5-6 atomic tool calls. Developers expose every option from every sub-operation to ensure the composite tool is "as flexible as the atomic tools." But a tool with 20 parameters is not simpler than 5 tools with 4 parameters each -- it is more complex because all parameters are in one namespace.

**How to avoid:**
- Composite tools should have at most 5-7 parameters. Use opinionated defaults for everything else.
- Follow the "create_collision_shape" pattern: it has 4 parameters (parent_path, shape_type, shape_params, name) where `shape_params` is a flexible object. The tool makes decisions internally (2D vs 3D based on parent type).
- If the AI needs to customize beyond the composite tool's defaults, it can use the atomic tools for the remaining adjustments. The composite tool handles the common case (80/20 rule).
- Design composite tools around common workflows observed in real AI usage, not around parameter exhaustiveness. Analyze the most common 5-tool sequences in UAT logs to identify what parameters are actually varied vs. left default.

**Warning signs:**
- A composite tool's `inputSchema` has more than 8 properties.
- AI frequently passes wrong values for composite tool parameters.
- AI sometimes uses the composite tool and then immediately calls atomic tools to "fix" properties the composite tool set incorrectly.

**Phase to address:**
Composite tools phase.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Copy-paste atomic tool logic into composite tool | Fast implementation, no refactoring needed | Divergent behavior on bug fixes, doubled maintenance | Never -- always extract shared internals |
| Add composite tools without removing/deprecating atomic equivalents | Backward compatible, no breaking changes | Tool count inflation, context window waste | Only if composite tool covers a genuinely different use case |
| Inline error enhancement logic per tool in `handle_request()` | Quick to add per-tool error messages | `handle_request()` grows to 2000+ lines, unmaintainable | Never -- extract error enhancement into a shared layer |
| Hardcode "did you mean X?" suggestions without validation | Feels more helpful to users | Infinite retry loops, incorrect suggestions | Never -- only suggest verified alternatives |
| Include full script content in enriched Resource responses | AI has "everything it needs" in one call | Massive context consumption, editor lag | Only for single-node detail endpoints, never for full scene |
| Ship prompt templates as C++ string literals | No file I/O, no packaging complexity | Every template change requires recompilation and redistribution | Acceptable for v1.5 if unit tests validate tool references |

## Integration Gotchas

Common mistakes when connecting new v1.5 features to the existing codebase.

| Integration Point | Common Mistake | Correct Approach |
|-------------------|----------------|------------------|
| Composite tool + UndoRedo | Each sub-operation commits its own action | Wrap entire composite in one `create_action()`/`commit_action()` pair |
| Enriched Resource + `get_scene_tree()` | Modify the existing `get_scene_tree()` to return more data | Create new resource handlers; keep existing `get_scene_tree()` unchanged |
| Smart errors + existing tool functions | Add error enhancement inside each tool function (e.g., `scene_mutation.cpp`) | Add error enhancement in the dispatcher layer, after the tool function returns |
| Prompt templates + composite tools | Write prompts referencing composite tools but leave old prompts referencing atomic tools | Update or deprecate old prompts that cover the same workflow as new composite tools |
| Error suggestions + Resources | Error message says "use get_scene_tree to see nodes" but resources already expose this | Error suggestions should reference both tools and resources as appropriate |
| Composite tools + game bridge | Composite tool tries to combine editor operations with runtime operations (e.g., create + run + test) | Keep composite tools within one domain (editor-only or runtime-only). Cross-domain workflows belong in prompt templates, not tools. |

## Performance Traps

Patterns that work at small scale but fail as scenes grow.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Full scene traversal in Resource handler | Editor lag during AI session | Tiered URIs + caching with generation counter | 100+ nodes in active scene |
| Signal connection graph traversal for every resource read | 200ms+ resource response time | Cache signal graph, invalidate on `connect_signal`/`disconnect_signal` | 50+ nodes with signals |
| Full script content in resource responses | 100KB+ resource payload, AI truncation | Return script path + line count only, not content | 10+ scripts in scene |
| Composite tool error messages including full context | 2KB+ error responses, context waste | Budget error messages to <200 characters of content | Any scene |
| No pagination on enriched resources | Single response too large for AI client | Add `max_nodes` parameter with default 50, return `has_more` flag | 200+ nodes |

## Security Mistakes

Domain-specific security issues for an editor plugin MCP server.

| Mistake | Risk | Prevention |
|---------|------|------------|
| Smart error messages expose file system paths | Reveals project structure to connected AI client (acceptable for editor plugin, but problematic if MCP session is logged/shared) | Use `res://` paths only, never absolute OS paths |
| Composite tool's `eval_in_game` step leaks runtime state | Composite tools combining editor + runtime operations could expose game state in error messages | Keep composite tools editor-only; runtime operations should require explicit tool calls |
| Prompt templates contain hardcoded file paths | If templates reference specific `res://` paths, they may not apply to the user's project | Use placeholder paths in templates with clear "[your_path_here]" markers |
| Enriched resources expose full script source via Resource protocol | Script content accessible without explicit `read_script` call, bypassing any future access controls | Never include script source in Resources; return metadata only |

## UX Pitfalls

Common user experience mistakes when enhancing AI workflow tools.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Composite tool names too generic (e.g., `create_entity`) | AI does not know when to use it vs. atomic tools; user confused by vague tool listing | Use specific names: `create_platformer_character`, `create_ui_panel` |
| Error messages too long and detailed | AI wastes tokens processing verbose errors; user sees walls of text in tool output | Keep error messages under 200 chars: what failed + how to fix |
| Resource responses change format between versions | AI scripts and workflows break on plugin update | Version resource URIs or maintain backward-compatible format |
| Prompt templates reference absolute step numbers ("Step 5: ...") | If AI skips or modifies earlier steps, step numbers become misleading | Use descriptive labels: "Create the root node:", "Apply the stylesheet:" |
| Adding 10+ new prompt templates without categorization | AI cannot find the right prompt template in a large list | Group templates by workflow domain: "scene_creation/", "ui_building/", "testing/" |
| Smart error handling adds latency to every tool call | Even successful operations feel slower | Only enhance errors on failure path; success path returns immediately unchanged |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Composite tools:** Verify UndoRedo rollback actually works -- create a composite tool, let step 3 of 5 fail, press Ctrl+Z. All nodes from steps 1-2 should be removed. If they persist, the single-action wrapping is broken.
- [ ] **Enriched Resources:** Measure the JSON payload size for a real 100-node scene. If it exceeds 10KB, the response needs trimming or pagination. Testing with a 5-node scene hides this.
- [ ] **Smart error handling:** Verify error suggestions reference valid tools by name. Run unit test that parses error message strings and validates tool references against registry. Manual testing misses renamed tools.
- [ ] **Prompt templates:** Test every prompt template against the actual tool registry. Run a unit test that calls `get_prompt_messages()` for every prompt, extracts `Tool: <name>` references, and asserts each exists. Do this AFTER adding composite tools.
- [ ] **Composite tools:** Verify that composite tools work when their sub-operations require deferred responses (e.g., a composite tool that includes `capture_viewport` needs to handle the async `frame_post_draw` callback). The `__deferred` pattern in the current code only supports top-level tool calls.
- [ ] **Resource caching:** Verify cache invalidation fires on ALL mutation paths -- not just `create_node`/`delete_node` but also `set_node_property`, `attach_script`, `connect_signal`, and composite tool mutations.
- [ ] **Error message budget:** After implementing smart errors, measure the total error response size for the 10 most common error cases. If any exceeds 500 bytes, trim it.
- [ ] **Tool count:** After adding all v1.5 tools, call `tools/list` and measure the response. If it exceeds 40KB, tools need to be consolidated or descriptions trimmed.

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Logic duplication in composite tools (P1) | MEDIUM | Extract shared internal functions, rewrite composite tools to use them, add regression tests comparing atomic vs composite behavior |
| Partial failure without rollback (P2) | HIGH | Redesign composite tools with single UndoRedo action scope; requires touching every composite tool implementation |
| Tool count explosion (P3) | LOW | Audit tools, consolidate, trim descriptions. Non-breaking: just remove tools from registry and update prompts |
| Unbounded resource responses (P4) | MEDIUM | Add size budget enforcement and pagination. May require URI scheme changes if clients cached old URIs |
| Stale prompt templates (P5) | LOW | Add validation unit test, fix all references. Quick once the test exists |
| Error messages leak internals (P6) | LOW | Audit and sanitize error message strings. No architectural change needed |
| Retry loops from bad suggestions (P7) | MEDIUM | Add validation for all "did you mean" suggestions. Requires touching every error path that suggests alternatives |
| Unmaintainable dispatcher (P8) | HIGH | Refactor to registry-based dispatch. Significant code change touching core server file |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| P1: Logic duplication | Phase 1 (Composite tools) | Code review: composite tools must call internal functions, not reimplementing Godot API calls |
| P2: Partial failure rollback | Phase 1 (Composite tools) | UAT: create composite tool, inject failure at each step, verify Ctrl+Z undoes everything |
| P3: Tool count explosion | Phase 1 (Composite tools) | Automated check: `tools/list` response size < 40KB after each tool addition |
| P4: Unbounded resources | Phase 2 (Resources) | Automated check: resource response size < 10KB for 100-node test scene |
| P5: Prompt-tool drift | Phase 1 (Composite tools) + Phase 4 (Prompts) | Unit test: all tool names in prompts exist in registry |
| P6: Error message leakage | Phase 3 (Error handling) | Code review: no `.cpp`, `::`, or OS file paths in error strings |
| P7: Retry loop suggestions | Phase 3 (Error handling) | UAT: trigger each error path, verify suggestions reference valid entities |
| P8: Dispatcher growth | Phase 1 (before adding tools) | Line count check: `handle_request()` < 800 lines after v1.5 |
| P9: Resource performance | Phase 2 (Resources) | Performance test: resource response time < 100ms for 200-node scene |
| P10: Template maintenance | Phase 4 (Prompts) | CI unit test: prompt template tool reference validation |
| P11: Over-parameterized composites | Phase 1 (Composite tools) | Design review: no composite tool schema exceeds 8 properties |

## Sources

### MCP Ecosystem (Context Window & Tool Design)
- [The MCP Context Window Problem: Why Too Many Tools Can Cripple AI Agents](https://www.junia.ai/blog/mcp-context-window-problem) -- industry data on token costs, 40-tool threshold, mitigation strategies
- [Perplexity Drops MCP Internally, Citing Context Window Waste](https://nevo.systems/blogs/news/perplexity-drops-mcp-protocol-72-percent-context-window-waste) -- 72% context waste figure, March 2026
- [MCP and Context Overload: Why More Tools Make Your AI Agent Worse](https://eclipsesource.com/blogs/2026/01/22/mcp-context-overload/) -- behavioral degradation from tool count
- [Your MCP Server Is Eating Your Context Window](https://www.apideck.com/blog/mcp-server-eating-context-window-cli-alternative) -- 143K of 200K tokens consumed by 40 tools
- [54 Patterns for Building Better MCP Tools](https://arcade.dev/blog/mcp-tool-patterns) -- tool composition, error-guided recovery, partial failure patterns

### MCP Error Handling
- [Better MCP Tool Call Error Responses: Help Your AI Recover Gracefully](https://alpic.ai/blog/better-mcp-tool-call-error-responses-ai-recover-gracefully) -- actionable error patterns, anti-patterns
- [Error Handling in MCP Servers - Best Practices Guide](https://mcpcat.io/guides/error-handling-custom-mcp-servers/) -- isError flag, error tiers, recovery patterns
- [Error Handling in MCP Tools](https://apxml.com/courses/getting-started-model-context-protocol/chapter-3-implementing-tools-and-logic/error-handling-reporting) -- structured error responses for AI self-correction

### MCP Best Practices
- [MCP Best Practices: Architecture & Implementation Guide](https://mcp-best-practice.github.io/mcp-best-practice/best-practice/) -- single responsibility, bounded toolsets, prompt template standardization
- [Recovering from Partial Failures in Enterprise MCP Tools](https://www.workato.com/the-connector/recovering-from-partial-failures-in-enterprise-mcp-tools/) -- saga pattern, compensating transactions
- [MCP Specification 2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25) -- tool annotations, output schemas

### Project-Specific (Codebase Analysis)
- `mcp_server.cpp` -- current 750-line `handle_request()` dispatcher, sequential if/else pattern
- `mcp_tool_registry.cpp` -- 50 tool definitions, ~35KB of schema JSON
- `mcp_prompts.cpp` -- 7 prompt templates with hardcoded tool name references
- `scene_mutation.cpp` -- existing tool function patterns (typed C++ functions returning JSON)
- v1.0-v1.4 PITFALLS.md -- prior research on Godot engine pitfalls (still applicable)

---
*Pitfalls research for: v1.5 AI Workflow Enhancement*
*Researched: 2026-03-23*
