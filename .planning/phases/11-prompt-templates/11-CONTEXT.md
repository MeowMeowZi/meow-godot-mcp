# Phase 11: Prompt Templates - Context

**Gathered:** 2026-03-19
**Status:** Ready for planning

<domain>
## Phase Boundary

AI gets guided workflow templates for common v1.1 tasks. Covers 2 MCP Prompt templates: UI building workflow and animation setup workflow. Templates reference real v1.1 tool names with step-by-step instructions. Does NOT cover v1.0 tool workflows, custom user-defined prompts, or dynamic prompt generation.

Requirements: PMPT-01, PMPT-02

</domain>

<decisions>
## Implementation Decisions

### Architecture
- Add 2 new prompts to existing `mcp_prompts.cpp` — Phase 4 already implemented MCP Prompts protocol
- 2 templates: `build_ui_layout` (PMPT-01) + `setup_animation` (PMPT-02)
- Each prompt has optional parameters for customization

### build_ui_layout Prompt (PMPT-01)
- Parameter: `layout_type` (optional) — e.g., "main_menu", "hud", "settings", "inventory"
- Step-by-step workflow referencing: create_node, set_layout_preset, set_theme_override, create_stylebox, set_container_layout
- Each step includes tool name + parameter examples + expected result
- Guides AI through complete UI construction: root Control → container hierarchy → styling → theme

### setup_animation Prompt (PMPT-02)
- Parameter: `animation_type` (optional) — e.g., "walk_cycle", "ui_transition", "idle", "attack"
- Step-by-step workflow referencing: create_animation, add_animation_track, set_keyframe, set_animation_properties
- Each step includes tool name + parameter examples + expected result
- Guides AI through complete animation setup: AnimationPlayer → tracks → keyframes → properties

### Template Structure
- Step 1→2→3... format with tool name + parameters + expected outcome per step
- References real v1.1 tool names (all 38 tools finalized)
- Includes example parameter values that demonstrate real usage

### Testing
- Unit tests: verify prompt registration, parameter schema, response format
- UAT: verify prompts return via MCP protocol with correct content

### Claude's Discretion
- Exact prompt text content and step descriptions
- Number of steps per template
- Example parameter values
- Whether to include error handling guidance in prompts
- Prompt description text for MCP listing

</decisions>

<canonical_refs>
## Canonical References

### Existing Implementation
- `src/mcp_prompts.h` / `src/mcp_prompts.cpp` — MCP Prompts protocol implementation (Phase 4)
- `src/mcp_server.cpp` — Prompts dispatch in handle_request
- `src/mcp_protocol.cpp` — create_prompts_list_response, create_prompt_get_response builders
- `tests/test_protocol.cpp` — Existing prompt protocol tests

### Tool Names Referenced in Templates
- UI tools: create_node, set_layout_preset, set_theme_override, create_stylebox, set_container_layout, get_ui_properties
- Animation tools: create_animation, add_animation_track, set_keyframe, get_animation_info, set_animation_properties

</canonical_refs>

<deferred>
## Deferred Ideas

- v1.0 tool workflow templates (scene building, scripting)
- User-defined custom prompt templates
- Dynamic prompt generation based on project state
- Template marketplace/sharing

</deferred>

---

*Phase: 11-prompt-templates*
*Context gathered: 2026-03-19*
