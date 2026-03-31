# Phase 26: Settings Persistence - Context

**Gathered:** 2026-03-31
**Status:** Ready for planning

<domain>
## Phase Boundary

User's Dock panel configuration (port number, disabled tools) survives editor restarts. Port conflicts produce immediate visible errors instead of silent desync with the bridge executable.

</domain>

<decisions>
## Implementation Decisions

### Persistence Mechanism
- Use ProjectSettings + save() for both port and disabled tools — already using set_setting(), just need save()
- Store disabled tools as comma-separated string in "meow_mcp/tools/disabled" — ProjectSettings native String support
- Remove set_initial_value(6800) call — handle default in code to ensure user-set 6800 also persists
- Load disabled tools in _enter_tree() after reading port, save in _on_tool_toggled()

### Port Conflict Handling
- Use push_error() + Dock panel status "端口被占用" for port conflict reporting — visible in both panels
- Remove auto-increment loop from BOTH _enter_tree() and _on_port_changed() — single attempt only
- On port failure: do not start server, dock shows "已停止" state — user can change port and restart

### Claude's Discretion
- Exact error message wording
- Whether to add a "retry" hint in the error message

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `mcp_plugin.cpp:54-73` — ProjectSettings registration pattern (port setting)
- `mcp_plugin.cpp:285-315` — `_on_port_changed()` handler
- `mcp_plugin.cpp:345-364` — `_on_tool_toggled()` handler
- `mcp_tool_registry.cpp:579-595` — `s_disabled_tools` set + get/set/is functions

### Established Patterns
- ProjectSettings accessed via `ProjectSettings::get_singleton()`
- Port stored as `meow_mcp/server/port` with PROPERTY_HINT_RANGE
- Tool state managed via static `s_disabled_tools` set in tool_registry (pure C++, no Godot deps)

### Integration Points
- `_enter_tree()` — load port + disabled tools from ProjectSettings
- `_on_port_changed()` — save port + restart server (single attempt)
- `_on_tool_toggled()` — save disabled tools list
- `mcp_tool_registry.h/cpp` — keep pure C++, persistence logic stays in mcp_plugin.cpp

</code_context>

<specifics>
## Specific Ideas

- Research confirmed ProjectSettings::save() may skip values matching set_initial_value() — mitigation: don't call set_initial_value() for port
- Tool registry stays pure C++ (no Godot headers) — persistence writes/reads happen in mcp_plugin.cpp

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>
