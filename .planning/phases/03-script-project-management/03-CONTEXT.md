# Phase 3: Script & Project Management - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can read, write, and attach GDScript files, query project structure (directories and files under res://), read project.godot settings, query resource file properties, and expose scene tree / project structure as MCP Resources. Also includes IO thread + queue/promise threading model (MCP-04, deferred from Phase 1).

Requirements: SCRP-01, SCRP-02, SCRP-03, SCRP-04, PROJ-01, PROJ-02, PROJ-03, PROJ-04, MCP-04

</domain>

<decisions>
## Implementation Decisions

### Script editing granularity
- Provide both full-file replacement AND line-level editing
- `write_script`: create new .gd files with full content (errors if file already exists)
- `read_script`: returns entire file content (no line-range support needed, GDScript files are typically small)
- `edit_script`: single tool with `operation` parameter (insert/replace/delete) for line-level editing
- Line numbers are 1-based (first line is line 1)
- No UndoRedo for script file operations — git is the undo mechanism for file changes

### Script attachment
- `attach_script`: attaches an existing .gd file to a node (does NOT create files inline)
- `detach_script`: removes script from a node
- If node already has a script, attach directly replaces it (no error, no force flag needed)
- attach_script uses UndoRedo (it's a scene tree operation, not a file operation)

### File scope and naming
- Tool names and descriptions are GDScript-specific (read_script, write_script, edit_script)
- Internal implementation is generic text file read/write (future-proof for shader/cfg extension)
- All file paths use `res://` prefix format (e.g., `res://scripts/player.gd`)
- File access restricted to res:// scope only (project directory)
- No write-protection on specific paths — AI clients have their own safety mechanisms
- After file write, call EditorFileSystem scan to refresh editor file tree immediately

### Resource file operations (PROJ-03)
- Read-only: load resources via Godot ResourceLoader API, read properties
- Returns resource type (e.g., Theme, ShaderMaterial) + property names and values
- Supports both .tres (text) and .res (binary) via Godot's unified ResourceLoader
- No resource creation or modification in Phase 3

### Project structure query (PROJ-01, PROJ-02)
- `list_project_files`: flat list of all files under res:// with file paths and types (.gd/.tscn/.tres etc.)
- `get_project_settings`: reads project.godot settings and returns as structured data

### MCP Resources (PROJ-04)
- Two MCP Resources: `godot://scene_tree` and `godot://project_files`
- Scene tree Resource coexists with `get_scene_tree` tool — Resource provides read-only snapshot for context loading, Tool provides parameterized query (max_depth, root_path etc.)
- Project files Resource returns flat file listing with paths and types
- Requires adding `resources` capability to MCP initialize response

### Claude's Discretion
- IO thread + queue/promise architecture for MCP-04 (threading model implementation details)
- Tool registration pattern (extend existing if-else chain or refactor)
- MCP Resources protocol implementation details (resources/list, resources/read handlers)
- Whether to add `resources/templates` capability alongside resources
- Exact JSON structure of resource query results
- Error message wording for new tools
- Whether edit_script supports multiple operations in single call

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### MCP Protocol
- `.planning/REQUIREMENTS.md` -- Full v1 requirements list, Phase 3 maps to SCRP-01..04, PROJ-01..04, MCP-04
- `.planning/ROADMAP.md` -- Phase 3 success criteria and dependency chain

### Prior Phase Context
- `.planning/phases/1/1-CONTEXT.md` -- Phase 1 decisions: connection model, error handling, bridge behavior, scene tree query design

### Existing Implementation
- `src/mcp_server.h` / `src/mcp_server.cpp` -- MCPServer class, tool dispatch in handle_request(), TCP polling, single-client model
- `src/mcp_protocol.h` / `src/mcp_protocol.cpp` -- JSON-RPC protocol layer, tool listing, MCP message builders
- `src/scene_tools.h` / `src/scene_tools.cpp` -- Scene tree query (get_scene_tree) implementation pattern
- `src/scene_mutation.h` / `src/scene_mutation.cpp` -- Scene CRUD tools pattern (free functions returning nlohmann::json)
- `src/variant_parser.h` / `src/variant_parser.cpp` -- String-to-Variant type parser (reusable for resource property display)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `variant_parser`: can convert Variant values back to human-readable strings for resource property display
- `mcp_protocol.cpp::create_tools_list_response()`: tool schema registration pattern — new tools follow same JSON structure
- `mcp_protocol.cpp::create_initialize_response()`: capability declaration — needs `resources` capability added
- `scene_tools.cpp::get_scene_tree()`: scene tree serialization — reusable for scene tree MCP Resource

### Established Patterns
- Each tool domain has its own header/cpp pair (scene_tools, scene_mutation) — new tools should follow same pattern (script_tools, project_tools)
- Tools are free functions returning `nlohmann::json` results
- Tool dispatch is in `mcp_server.cpp::handle_request()` via if-else chain on tool name
- Tool schemas are static JSON in `mcp_protocol.cpp::create_tools_list_response()`
- Pure C++ protocol layer (mcp_protocol) kept Godot-free for testability

### Integration Points
- New tool functions registered in `mcp_server.cpp::handle_request()`
- New tool schemas added to `mcp_protocol.cpp::create_tools_list_response()`
- MCP Resources need new method handlers: `resources/list` and `resources/read` in `handle_request()`
- Resources capability added to `create_initialize_response()`
- EditorFileSystem access needed for file scanning after writes
- ResourceLoader/ResourceSaver access needed for .tres/.res reading
- FileAccess API needed for script file read/write operations

</code_context>

<specifics>
## Specific Ideas

- Script tools should feel natural for AI: read full file, understand context, write back modified version — the same workflow Claude uses with its own tools
- File paths consistently use `res://` prefix to match Godot's mental model — AI sees paths the same way Godot docs describe them
- MCP Resources provide "always available context" for AI — scene tree and file list loaded automatically, while Tools are for on-demand operations

</specifics>

<deferred>
## Deferred Ideas

- Shader file editing (.gdshader) — internal implementation is ready, just needs new tool registration in future phase
- Resource modification (write .tres/.res via ResourceSaver) — read-only for now, extend if users request
- Line-range reading for very large scripts — full file read is sufficient for typical GDScript files
- Resource write protection / path blacklist — not needed given AI client safety mechanisms

</deferred>

---

*Phase: 03-script-project-management*
*Context gathered: 2026-03-17*
