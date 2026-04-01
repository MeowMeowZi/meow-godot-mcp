# Phase 28: Logging & Cleanup - Context

**Gathered:** 2026-04-01
**Status:** Ready for planning
**Mode:** Auto-generated (infrastructure phase — discuss skipped)

<domain>
## Phase Boundary

Plugin errors are visible in both Godot output panels (Output + Debugger > Errors). Dead code left over from the 59-to-30 tool consolidation is removed from error_enrichment.cpp.

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — infrastructure phase. Use ROADMAP phase goal, success criteria, and codebase conventions to guide decisions.

Key guidance from research:
- Replace printerr() calls for actual errors with push_error() so they appear in both Output and Debugger panels
- Keep print() for informational messages (Output-only is fine)
- Remove stale TOOL_PARAM_HINTS entries for tools deleted during 59→30 consolidation
- Verify remaining entries match current 30 tools by cross-referencing mcp_tool_registry.cpp

</decisions>

<code_context>
## Existing Code Insights

Codebase context will be gathered during plan-phase research.

</code_context>

<specifics>
## Specific Ideas

No specific requirements — infrastructure phase. Refer to ROADMAP phase description and success criteria.

</specifics>

<deferred>
## Deferred Ideas

None — discuss phase skipped.

</deferred>
