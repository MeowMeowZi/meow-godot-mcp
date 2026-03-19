# Phase 6: Scene File Management - Context

**Gathered:** 2026-03-18
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can persist, load, and organize scene files without manual intervention. Covers saving scenes to disk, opening existing .tscn files, listing open scenes, creating brand-new scenes with specified root types, and instantiating PackedScenes as child nodes. Does NOT cover scene inheritance, scene resource editing, or advanced scene tree restructuring.

Requirements: SCNF-01, SCNF-02, SCNF-03, SCNF-04, SCNF-05, SCNF-06

</domain>

<decisions>
## Implementation Decisions

### Tool Architecture & Naming
- 5 independent MCP tools: save_scene, open_scene, list_open_scenes, create_scene, instantiate_scene — 1:1 mapping to success criteria, consistent with existing tool-per-operation pattern
- No close_scene tool — not in SCNF requirements, keep scope minimal
- save_scene supports "save as" via optional path parameter: no path = overwrite save current, with path = save to new location
- New scene_file_tools.h/.cpp file pair — follows existing code organization pattern (scene_tools, scene_mutation, script_tools)

### Save & Open Behavior
- Unsaved scene (no file path) + no path parameter = return error requiring path — no auto-generated paths
- open_scene does NOT close current scene — Godot editor supports multi-tab, just adds/activates the tab
- No overwrite protection on save — consistent with editor Ctrl+S behavior, version control provides safety
- Save format follows file extension: .tscn (text, default) or .scn (binary) based on provided path

### Scene Creation, Instantiation & Response Format
- create_scene auto-opens in editor after creation — success criteria explicitly requires it
- instantiate_scene uses UndoRedo — consistent with create_node pattern, all mutation operations use UndoRedo
- list_open_scenes returns array of {path, title, is_active} per scene — comprehensive but concise
- Unified response format: success = {"success": true, "path": "res://..."} + operation-specific fields; failure = {"error": "..."} — matches all existing tools

### Claude's Discretion
- Exact parameter naming and ordering within tool definitions
- Error message text and detail level
- Whether save_scene returns additional metadata (file size, timestamp)
- Whether create_scene accepts optional initial properties for the root node
- Internal implementation details (helper functions, validation order)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` -- Full v1.1 requirements, Phase 6 maps to SCNF-01..06
- `.planning/ROADMAP.md` -- Phase 6 success criteria, dependency chain

### Prior Phase Context
- `.planning/phases/1/1-CONTEXT.md` -- Phase 1: connection model (port 6800), bridge relay architecture, error patterns
- `.planning/phases/03-script-project-management/03-CONTEXT.md` -- Phase 3: IO thread + queue/promise, tool dispatch, file scope (res://)
- `.planning/phases/04-editor-integration/04-CONTEXT.md` -- Phase 4: ToolDef registry with version filtering, dock panel
- `.planning/phases/05-runtime-signals-distribution/05-CONTEXT.md` -- Phase 5: runtime tools pattern, EditorInterface usage

### Existing Implementation
- `src/mcp_server.h` / `src/mcp_server.cpp` -- MCPServer: handle_request dispatch, IO+main thread architecture
- `src/scene_mutation.h` / `src/scene_mutation.cpp` -- create_node, set_node_property, delete_node with UndoRedo — pattern for new scene mutation tools
- `src/mcp_tool_registry.h` / `src/mcp_tool_registry.cpp` -- ToolDef struct, get_filtered_tools_json()
- `src/mcp_protocol.h` / `src/mcp_protocol.cpp` -- JSON-RPC builders, tool/resource response helpers
- `src/runtime_tools.cpp` -- EditorInterface::get_singleton() usage pattern for editor operations
- `src/script_tools.cpp` -- ResourceLoader/ResourceSaver patterns, manual GDScript construction

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `EditorInterface::get_singleton()` — access to save_scene, open_scene_from_path, get_open_scenes, etc.
- `MCPServer::handle_request()` — tool dispatch via if-else chain, new tools plug in here
- `ToolDef` registry — version-gated tool registration for all new tools
- `create_tool_result()`, `create_error_response()` — protocol response builders
- UndoRedo pattern from `scene_mutation.cpp` — reuse for instantiate_scene
- `ClassDB::instantiate()` pattern from `scene_mutation.cpp` — reuse for create_scene root node

### Established Patterns
- Each tool domain: separate header/cpp pair (scene_tools, scene_mutation, script_tools, runtime_tools, signal_tools)
- Tools are free functions taking parameters + returning nlohmann::json
- EditorInterface singleton for all editor operations
- All mutation tools take EditorUndoRedoManager* parameter
- Error format: {"error": "description"}, success: {"success": true, ...}
- Thread safety: all Godot API calls on main thread via queue/promise

### Integration Points
- `handle_request()` in mcp_server.cpp: add 5 new tool handlers
- `mcp_tool_registry.cpp`: register 5 new ToolDef entries with {4, 3, 0} min_version
- New `scene_file_tools.h/.cpp` files added to SConstruct build
- Godot API: EditorInterface::save_scene(), open_scene_from_path(), get_open_scenes(), PackedScene::instantiate()

</code_context>

<specifics>
## Specific Ideas

- save_scene mirrors editor Ctrl+S / Ctrl+Shift+S behavior — AI gets same save control as human developer
- Scene file management fills critical gap: AI can create nodes and write scripts but cannot save its work to disk
- list_open_scenes enables AI to understand editor state before making changes
- instantiate_scene enables scene composition workflow: create reusable scene → instantiate in other scenes

</specifics>

<deferred>
## Deferred Ideas

- close_scene tool — could be useful for AI workspace management, consider for future milestone
- Scene inheritance/variant editing — complex, separate feature
- Batch scene operations (save all, close all) — not in current requirements
- Scene diff/comparison tools — advanced feature for later

</deferred>

---

*Phase: 06-scene-file-management*
*Context gathered: 2026-03-18*
