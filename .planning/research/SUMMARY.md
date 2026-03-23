# Project Research Summary

**Project:** Godot MCP Meow — v1.5 AI Workflow Enhancement
**Domain:** C++ GDExtension MCP Server Plugin for Godot Engine
**Researched:** 2026-03-23
**Confidence:** HIGH

## Executive Summary

Godot MCP Meow v1.5 is an architectural intelligence upgrade to an existing, battle-tested 50-tool MCP server. The research is unanimous on a key insight: this milestone adds zero new Godot API surface and zero new external libraries. Every v1.5 feature (composite tools, enriched resources, smart error handling, expanded prompts) is implemented by extending existing C++ code patterns within the same stack — C++17, godot-cpp v10+, nlohmann/json 3.12.0, SCons, and GoogleTest. The zero-dependency differentiator is preserved. The recommended implementation order is strict: smart error handling first, then enriched resources, then composite tools, then expanded prompts. This ordering is dependency-driven — errors improve all downstream features, resources validate read-only paths before mutations, composites depend on primitives being stable, and prompts must reference finalized tool names.

The dominant risk is not technical complexity but architectural discipline. Research identifies three systemic failure modes: (1) composite tools duplicating atomic tool logic instead of calling shared internal functions, causing behavioral divergence over time; (2) the monolithic `handle_request()` dispatcher — already 750-1060 lines — growing to 1500+ lines without refactoring; and (3) enriched resources returning unbounded data that floods the AI context window. Each of these risks has a clear prevention strategy that must be designed into the first phase, not retrofitted. A hard budget constraint must be set before any tool is added: `tools/list` payload must stay under 40KB and individual resource responses under 10KB.

The competitive differentiation case for v1.5 is strong. No competitor (Godot MCP Pro, Better Godot MCP, GDAI MCP) offers composite tools with single-action UndoRedo rollback — they all use WebSocket round-trips that create separate undo entries per step. No competitor uses proper MCP Resources for auto-context; they use tools. The `isError: true` error handling pattern, specified in MCP spec 2025-03-26, is absent from all reviewed competitors and is the highest-ROI, lowest-cost feature in the entire v1.5 scope.

---

## Key Findings

### Recommended Stack

**Zero new dependencies.** The existing stack handles all v1.5 requirements. A URI template library (RFC 6570) was explicitly evaluated and rejected — the `godot://node/{path}` scheme uses Level 1 simple variable expansion that requires only ~30 LOC of `std::string::find()` + `substr()`. An error catalog framework was rejected as over-engineering; a static `std::unordered_map<std::string, ErrorInfo>` at ~200 LOC covers all ~50 tools with ~15 error categories. Template engines for prompts were rejected because the existing `PromptDef` lambda pattern already handles `{argument}` substitution.

**New source files required (6 files total):**
- `src/composite_tools.h` / `.cpp` — composite tool implementations (~500-800 LOC)
- `src/resource_providers.h` / `.cpp` — enriched resource data providers
- `src/error_diagnostics.h` / `.cpp` — error categorization and enrichment (~200-300 LOC)

**Core technologies (all unchanged):**
- **C++17** — language standard, already in use
- **godot-cpp v10+ (Godot 4.3+)** — GDExtension bindings, already in use
- **nlohmann/json 3.12.0** — JSON handling, already latest release (2025-04-11)
- **SCons** — build system; `Glob("src/*.cpp")` auto-detects new files, no SConscript changes needed
- **GoogleTest** — unit testing; CMakeLists.txt needs new test file entries only

### Expected Features

**Must have — v1.5 launch (P1):**
- Smart error handling with `isError: true` flag and node-not-found suggestions — highest ROI, zero dependencies, improves all other features
- `find_nodes` composite tool — search by type, name pattern, property value; enables batch workflows
- `batch_set_property` composite tool — most-requested missing feature vs. competitors
- `create_character` composite tool — highest-visibility feature for new users
- Enriched `godot://scene_tree` resource with properties, scripts, and signals inline — biggest context improvement
- 4-5 new prompt templates (platformer, debug crash, tilemap, tool composition guide) — low cost, high discoverability

**Should have — v1.5.x patches (P2):**
- `create_ui_panel` composite tool — needed but input schema design is complex
- Resource templates (`godot://node/{path}`, `godot://script/{path}`) with `resources/templates/list` MCP protocol handler
- `duplicate_node` composite tool — useful but less common workflow
- Project structure resource with file metadata

**Defer — v2+ (P3):**
- "Create complete scene from template" composite — very high complexity, needs careful template design
- Scene diff resource — requires snapshot infrastructure
- Resource subscriptions (`resources/subscribe`) — spec support exists but marginal value for editor workflows; no major AI client supports it

### Architecture Approach

The v1.5 architecture is additive: three new module pairs (composite_tools, resource_providers, error_diagnostics) plug into the existing monolithic dispatcher with minimal surface-area changes to mcp_server.cpp. The Godot-free boundary (registry/protocol headers contain no Godot types, enabling GoogleTest without godot-cpp linkage) must be preserved — all Godot-dependent logic lives in `.cpp` files behind `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED`. The if/else dispatch chain in `handle_request()` is a recognized tech debt item but should NOT be refactored in v1.5 — the table-dispatch refactor is a separate concern for v2.0. Error enrichment is implemented as a passive wrapper called after each tool function returns, adding zero overhead on the success path.

**Major components (v1.5 additions):**
1. **composite_tools** — multi-step orchestrators calling existing atomic functions directly (not re-dispatching through MCP); each composite wraps all sub-steps in a single UndoRedo action
2. **resource_providers** — read-only query functions reusing existing tool modules (scene_tools, signal_tools, script_tools); expose via extended `resources/list` and new `resources/templates/list` handler
3. **error_diagnostics** — error categorization (`categorize_error`, Godot-free, unit-testable) plus Godot-dependent enrichment (`enrich_error`, adds did-you-mean suggestions and context); wired into dispatcher as a post-call wrapper
4. **mcp_prompts.cpp (extended)** — 5-8 new PromptDef entries using existing lambda pattern; prompts written after composite tools are finalized to reference accurate tool names

**Data flow for the key new pattern (composite tool):**
```
MCP Client -> tools/call "create_character"
  -> MCPServer::handle_request() dispatch
    -> composite_tools::create_character()
      -> scene_mutation::create_node()               (step 1)
      -> physics_tools::create_collision_shape()     (step 2)
      -> script_tools::write_script()               (step 3)
      -> script_tools::attach_script()              (step 4)
    -> error_diagnostics::enrich_error() (if error)
  -> mcp::create_tool_result()
-> MCP Client
```

### Critical Pitfalls

1. **Composite tool logic duplication** — Composite tools must call existing internal C++ functions (e.g., `create_node()` from scene_mutation.cpp), not reimplement Godot API calls. Divergent implementations will drift on bug fixes. Prevention: design shared internal function boundaries before writing any composite tool.

2. **Partial failure without rollback** — If a composite tool's step 3 of 5 fails, steps 1-2 have already mutated the scene. The AI retries and creates duplicates. Prevention: validate all inputs upfront before any mutation; wrap the entire composite in one UndoRedo action so rollback is atomic. Start with Option B (per-step UndoRedo, pragmatic) and document the behavior explicitly.

3. **Tool count explosion exceeding context budget** — 50 tools are already ~35KB of schema JSON. Adding 8+ composite tools without removing or shrinking atomic equivalents pushes toward 60KB+. Industry data: 40+ tools across 3 MCP servers consumed 143K of 200K context tokens; Perplexity dropped MCP internally citing 72% context waste (March 2026). Prevention: set hard limit of 40KB for `tools/list` response and measure after each addition. Trim descriptions of existing verbose tools (`inject_input`, `run_test_sequence`).

4. **Enriched resources returning unbounded data** — A 200-node scene with full script content, all signal connections, and all property details produces 50,000-500,000+ token Resource responses. Prevention: use tiered URIs (compact scene tree by default; detailed node info only via `godot://node/{path}`); enforce 10KB cap per resource response; never include full script source in Resources — return path and line count only.

5. **Prompt templates hardcoding tool names that drift** — Prompt templates reference tool names that change when composite tools supersede atomic tools. No build-time check exists. Prevention: add a unit test that parses all prompt template output, extracts `Tool: <name>` references, and asserts each exists in `get_all_tools()`. Build this test in Phase 1 before any tool names change.

---

## Implications for Roadmap

Based on all four research files, the dependency graph is unambiguous: smart errors enable everything else, composite tools depend on atomic tool stability, prompts depend on composite tool names. The recommended phase structure follows this dependency order exactly.

### Phase 1: Smart Error Handling Foundation

**Rationale:** Error enrichment is a passive wrapper with no dependencies on other v1.5 features. Adding it first means all subsequent features (composite tools, enriched resources) automatically get diagnostic-rich error responses from the moment they are written. This is the highest-ROI, lowest-risk starting point.

**Delivers:**
- `src/error_diagnostics.h/.cpp` with `categorize_error()` (Godot-free, GoogleTest-covered) and `enrich_error()` (Godot-dependent)
- `isError: true` on all tool error results via `create_tool_error_result()` in mcp_protocol.cpp
- Did-you-mean node suggestions using Levenshtein distance against actual scene tree
- Error catalog: ~15 error codes covering all 50 tools (NODE_NOT_FOUND, NO_SCENE_OPEN, INVALID_NODE_TYPE, SCRIPT_NOT_FOUND, GAME_NOT_RUNNING, BRIDGE_NOT_CONNECTED, etc.)
- Unit test: `test_error_diagnostics.cpp` validating categorization and suggestions without Godot linkage
- Unit test: prompt template tool reference validator (validation harness built before any tool names change)

**Addresses:** FEATURES.md P1 — smart error handling; PITFALLS.md P5 (prompt drift) mitigation infrastructure; PITFALLS.md P6 (error leakage) by establishing message budget constraints

**Avoids:** Inline error enhancement per tool in handle_request() (PITFALLS.md P8 anti-pattern — keeps dispatcher lean)

---

### Phase 2: Enriched MCP Resources

**Rationale:** Resources are read-only queries that reuse existing tool module functions. They have no mutation risk and validate the resource_providers module pattern before composite tools depend on similar infrastructure. Implementing resources before composites also gives AI clients richer scene context to use when invoking composite tools.

**Delivers:**
- `src/resource_providers.h/.cpp` with `get_node_details()`, `get_signal_map()`, `get_scene_scripts()`
- Extended `resources/list` with `godot://signal_map` and `godot://scene_scripts` static resources
- New `resources/templates/list` MCP protocol handler with `godot://node/{path}` and `godot://script/{path}` templates
- Updated `create_initialize_response()` to declare `resources` capability
- `create_resource_templates_list_response()` builder in mcp_protocol.cpp
- UAT test: `tests/uat_enriched_resources.py` — resources/list, resources/read, template resources

**Addresses:** FEATURES.md P1 — enriched scene_tree resource; FEATURES.md P2 — resource templates; PITFALLS.md P4 mitigation (tiered URIs, 10KB response cap enforced here)

**Avoids:** Including full script source in resource responses (return path + line count only); making resource handlers that mutate state; modifying existing `get_scene_tree()` function

---

### Phase 3: Composite Tools

**Rationale:** Composite tools are the most complex feature and the most likely to surface edge cases. They should be implemented after error diagnostics are in place (automatic error enrichment accelerates debugging during development) and after resource providers are validated (AI can use resources to verify composite results). Composite tool names must be finalized before Phase 4 prompts can reference them.

**Delivers:**
- `src/composite_tools.h/.cpp` with `create_character`, `batch_set_property`, `find_nodes`, `create_ui_panel`, `duplicate_node`
- ToolDef entries in mcp_tool_registry.cpp for each composite tool
- Dispatch branches in mcp_server.cpp (in a clearly marked `// === Composite Tools ===` section)
- Structured `nodes_created` + `steps_completed` result format for all composites
- Partial failure response with `partial_results` and `failed_step` fields
- UAT test: `tests/uat_composite_tools.py` — create_character, verify nodes exist, Ctrl+Z rollback
- Automated check: `tools/list` payload must remain under 40KB after each composite tool addition

**Addresses:** FEATURES.md P1 — create_character, batch_set_property, find_nodes; FEATURES.md P2 — create_ui_panel, duplicate_node; PITFALLS.md P1 (logic duplication), P2 (partial failure rollback), P3 (tool count budget), P11 (max 8 parameters per composite schema)

**Avoids:** Creating a generic pipeline framework for composites (ARCHITECTURE.md anti-pattern 1); modifying existing tool function signatures (ARCHITECTURE.md anti-pattern 3); including deferred async sub-steps inside composites (e.g., capture_viewport — keep composites in the editor-only domain)

---

### Phase 4: Expanded Prompt Templates

**Rationale:** Prompts are purely additive text content with zero mutation risk. They must come last because they reference specific tool names and parameters — including composite tool names from Phase 3. Writing prompts before composite tools are finalized leads to stale references from day one.

**Delivers:**
- 5-8 new PromptDef entries in mcp_prompts.cpp: `build_platformer_game`, `build_top_down_game`, `debug_game_crash`, `setup_tilemap_level`, `tool_composition_guide`, `fix_common_errors`, `create_game_from_scratch`
- Each prompt embeds resource references (link to `godot://scene_tree` etc.) and explicit tool call sequences with verified parameter formats
- Total prompt count: ~12-15 (7 existing + 5-8 new)
- Unit test: all new prompts verified against registry by the validation harness built in Phase 1

**Addresses:** FEATURES.md P1 — 4-5 new prompt templates; PITFALLS.md P5 and P10 (template maintenance) via the validation unit test from Phase 1

**Avoids:** Writing prompts before composite tools exist (PITFALLS.md P5); exceeding 15 total prompts without categorization (FEATURES.md anti-feature); embedding full GDScript code in prompt text (FEATURES.md anti-feature)

---

### Phase Ordering Rationale

- Error handling is implemented before everything because the `enrich_error()` wrapper has no dependencies and benefits every tool call during development. Every subsequent tool call during Phase 2 and 3 automatically gets diagnostic-rich errors.
- Resources are implemented before composites because they are read-only. They prove the new module pattern (resource_providers.h) works correctly before composite tools use the same module pattern. Declaring the `resources` capability in the initialize response is a protocol-level change best validated in isolation.
- Composite tools are implemented before prompts because prompt templates must reference real, stable tool names and parameter schemas. Composite tool names and schemas must be frozen before prompts are written.
- The prompt template validation unit test is built in Phase 1 (before any tool surface changes occur) so it catches drift from the very first composite tool addition.

### Research Flags

Phases needing deeper research during planning:
- **Phase 3 (Composite Tools):** UndoRedo single-action grouping implementation is the highest-risk technical detail. The ARCHITECTURE.md recommends starting with Option B (per-step UndoRedo, pragmatic) to avoid refactoring existing tool functions, but the UX implications should be validated with a prototype before committing to the approach. Additionally, the `create_ui_panel` input schema design (declarative layout spec) has no prior art in the codebase and may benefit from per-phase research.
- **Phase 2 (Enriched Resources):** The 10KB resource response cap and tiered URI design should be validated against real 100-200 node scenes before implementation. The exact fields to include in `get_node_details()` may need tuning based on what AI clients actually consume.

Phases with standard patterns (skip research-phase):
- **Phase 1 (Smart Error Handling):** The MCP `isError: true` pattern is fully specified in MCP spec 2025-03-26 and 2025-06-18. The Levenshtein distance implementation is a well-known algorithm. No additional research needed.
- **Phase 4 (Expanded Prompts):** Purely additive text following the established `PromptDef` lambda pattern. No new APIs or protocol changes. No additional research needed.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Zero new dependencies confirmed by cross-checking spec requirements against existing codebase. nlohmann/json 3.12.0 confirmed as latest release. No version conflicts. |
| Features | HIGH | Based on MCP spec analysis (primary), direct codebase review (primary), and competitor audit of Godot MCP Pro, Better Godot MCP, and GDAI MCP. Feature prioritization (P1/P2/P3) is well-grounded in both user value and implementation cost estimates. |
| Architecture | HIGH | Based on direct review of all source files. Component boundaries and data flows verified against existing code patterns in scene_mutation.cpp, physics_tools.cpp, and mcp_server.cpp. One area of genuine uncertainty: UndoRedo single-action grouping for composites (see Gaps). |
| Pitfalls | HIGH | 11 pitfalls identified from MCP ecosystem evidence (industry data on context window waste, Perplexity case study), v1.0-v1.4 project experience, and codebase analysis. Recovery strategies and prevention phases are mapped for all 11. |

**Overall confidence: HIGH**

### Gaps to Address

- **UndoRedo single-action grouping:** ARCHITECTURE.md recommends Option B (pragmatic per-step UndoRedo) to avoid refactoring existing tool functions. The correct choice between Option A (single atomic action) and Option B should be determined by prototyping `create_character` in Phase 3 and testing Ctrl+Z behavior before implementing other composite tools. Document the chosen behavior explicitly in the composite tool ToolDef descriptions.
- **tools/list payload baseline:** The current 50-tool `tools/list` payload is estimated at ~35KB but not precisely measured. Before Phase 3, measure the actual payload size to establish the headroom available for composite tool additions (budget: 40KB hard limit).
- **Resource response size for real scenes:** The 10KB cap on resource responses is derived from industry best practices but has not been measured against an actual Godot project. Validate against a 50-200 node test scene early in Phase 2.
- **create_ui_panel schema design:** No prior art exists in the codebase for a declarative UI layout specification as a JSON input parameter. This schema design is the highest-uncertainty element in Phase 3 and may benefit from reviewing how better-godot-mcp handles analogous composite inputs.

---

## Sources

### Primary (HIGH confidence)
- MCP Specification 2025-03-26 — resources, resource templates, URI schemes, capabilities declaration
- MCP Specification 2025-06-18 — tools `isError` flag, structured tool results, tool annotations
- Existing codebase (direct review) — mcp_server.cpp, mcp_protocol.h/.cpp, mcp_prompts.cpp, mcp_tool_registry.h/.cpp, scene_mutation.cpp, physics_tools.cpp, signal_tools.cpp, script_tools.cpp, ui_tools.cpp
- nlohmann/json GitHub Releases — confirmed 3.12.0 is latest (released 2025-04-11)
- Project MEMORY.md — architecture decisions, two-process pattern, threading model

### Secondary (MEDIUM confidence)
- [54 Patterns for Building Better MCP Tools (Arcade)](https://arcade.dev/blog/mcp-tool-patterns) — composite tool patterns, error-guided recovery, composition principles
- [Better MCP Tool Call Error Responses (Alpic AI)](https://dev.to/alpic/better-mcp-toolscall-error-responses-help-your-ai-recover-gracefully-15c7) — isError vs JSON-RPC errors, tool ordering guidance
- [MCP Server Best Practices (MCPcat)](https://mcpcat.io/blog/mcp-server-best-practices/) — tool composition, batch patterns
- [Better Godot MCP (18 mega-tools)](https://github.com/n24q02m/better-godot-mcp) — composite tool design reference, schema patterns
- [GDAI MCP Server](https://gdaimcp.com/) — debug loop with auto-screenshot, visual feedback workflow
- [MCP Prompts for Workflow Automation (Zuplo)](https://zuplo.com/blog/mcp-server-prompts) — prompt design patterns, multi-step workflow guidance

### Industry data (MEDIUM-HIGH confidence)
- [The MCP Context Window Problem (Junia AI)](https://www.junia.ai/blog/mcp-context-window-problem) — 40-tool threshold, 40KB budget guidance
- [Perplexity Drops MCP Internally (Nevo Systems)](https://nevo.systems/blogs/news/perplexity-drops-mcp-protocol-72-percent-context-window-waste) — 72% context waste figure, March 2026
- [Your MCP Server Is Eating Your Context Window (APIdecks)](https://www.apideck.com/blog/mcp-server-eating-context-window-cli-alternative) — 143K of 200K tokens consumed by 40 tools across 3 servers
- [Godot MCP Pro (162 tools)](https://github.com/youichi-uda/godot-mcp-pro) — batch_set_property, find_nodes_by_type reference implementation

---
*Research completed: 2026-03-23*
*Ready for roadmap: yes*
