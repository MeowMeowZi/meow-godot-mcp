# Phase 25: Prompt Templates - Context

**Gathered:** 2026-03-24
**Status:** Ready for planning
**Mode:** Auto-generated (infrastructure phase — low ambiguity)

<domain>
## Phase Boundary

Add 8 new MCP prompt templates that guide AI through multi-tool workflows: tool composition guide, debugging workflows, game-building workflows, and error recovery. All prompts reference finalized tool names from the 55-tool registry.

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — this is a pure content phase. Use the existing PromptDef pattern in mcp_prompts.cpp. Key constraints:
- All 8 templates must reference only tools that exist in the tool registry (55 tools)
- Follow existing prompt pattern: name, description, arguments array, lambda returning messages
- Total prompts after phase: 7 existing + 8 new = 15 (at the cap per research recommendation)
- Templates should be workflow-oriented (multi-tool sequences), not individual tool descriptions

### Template List (from REQUIREMENTS.md)
1. PROMPT-01: `tool_composition_guide` — speed reference card for common tasks → tool sequences
2. PROMPT-02: `debug_game_crash` — systematic crash debugging workflow
3. PROMPT-03: `build_platformer_game` — end-to-end 2D platformer from scratch
4. PROMPT-04: `setup_tilemap_level` — TileMap level creation workflow
5. PROMPT-05: `build_top_down_game` — end-to-end top-down game from scratch
6. PROMPT-06: `debug_physics_issue` — physics-specific debugging workflow
7. PROMPT-07: `create_game_from_scratch` — parameterized full game creation (genre argument)
8. PROMPT-08: `fix_common_errors` — common MCP tool error recovery guide

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/mcp_prompts.h/cpp` — 7 existing prompts with PromptDef pattern
- `src/mcp_tool_registry.cpp` — 55 tools, ToolDef registry for valid tool name reference

### Established Patterns
- PromptDef: `{name, description, args_json, lambda(args)->messages_json}`
- Arguments: `{{"name", "x"}, {"description", "..."}, {"required", bool}}`
- Messages: `nlohmann::json::array({{{"role","user"},{"content",{{"type","text"},{"text","..."}}}}})`

### Integration Points
- `mcp_prompts.cpp` — add 8 new PromptDef entries to prompts vector
- No other files need modification (prompts are self-contained)

</code_context>

<specifics>
## Specific Ideas

No specific requirements — standard prompt template content based on research.

</specifics>

<deferred>
## Deferred Ideas

None — all 8 templates are in scope.

</deferred>
