# Phase 23: Enriched Resources - Context

**Gathered:** 2026-03-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Enrich MCP Resources to deliver rich scene context automatically: scene tree with inline scripts/signals/properties, parameterized URI templates for per-node/script/signal queries, and project file metadata (size, type, modification time).

</domain>

<decisions>
## Implementation Decisions

### Resource Content Scope
- Enriched scene_tree includes script path + line count + source code (truncated at 100 lines; first 50 + notice for large scripts)
- Signal connections shown as outgoing + incoming arrays per node (signal name, target, method)
- Properties included: @export properties + transform properties (position/rotation/scale) + visibility. No internal properties
- 10KB response size limit with depth=3 default. Oversized responses return summary + suggest using URI templates for specific nodes

### URI Template Design
- Three URI templates: `godot://node/{path}`, `godot://script/{path}`, `godot://signals/{path}` — simple string prefix matching
- Implement `resources/templates/list` MCP method handler so AI clients can discover available templates
- Single node detail returns: full property list + script source + child list (one level) + signal connections — combines get_scene_tree + read_script + get_node_signals

### Project Files Enrichment
- File type classification by extension: scene(.tscn/.scn), script(.gd), resource(.tres/.res), image(.png/.jpg/.svg), audio(.ogg/.wav/.mp3), other
- File size via FileAccess::get_length()
- Modification time via FileAccess::get_modified_time() (Godot 4.3+)

### Claude's Discretion
- Internal organization of resource provider functions
- Exact JSON structure for enriched responses
- How to handle missing scripts/signals gracefully

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `mcp_server.cpp:366-401` — existing resources/list and resources/read handlers (2 resources: scene_tree, project_files)
- `mcp_protocol.cpp:130-136` — `create_resources_list_response()` and `create_resource_read_response()` helpers
- `get_scene_tree()` function — existing scene tree traversal with depth/properties support
- `list_project_files()` function — existing flat file listing
- `read_script()` / `get_node_signals()` in script_tools and signal_tools — reusable for enrichment

### Established Patterns
- Resources return JSON via `create_resource_read_response()` with contents array
- Each content item has uri, mimeType, text fields
- Tools and resources share same Godot API access (main thread via queue)

### Integration Points
- `mcp_server.cpp` resources/list handler — add new resource templates
- `mcp_server.cpp` resources/read handler — add URI template matching
- New `resources/templates/list` method handler needed

</code_context>

<specifics>
## Specific Ideas

No specific requirements — standard MCP Resources enrichment based on research findings.

</specifics>

<deferred>
## Deferred Ideas

- Resource subscriptions (resources/subscribe) — deferred to v2+ per research recommendation
- Scene diff resource — high complexity, deferred to future

</deferred>
