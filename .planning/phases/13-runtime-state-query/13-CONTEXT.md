# Phase 13: Runtime State Query - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

新增三个运行时查询工具：get_game_node_property（读取运行中游戏节点属性）、eval_in_game（执行 GDScript 表达式）、get_game_scene_tree（获取运行中游戏场景树）。基于 Phase 12 建立的 EditorDebuggerSession deferred 响应模式。

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — infrastructure phase extending the Phase 12 game bridge pattern. Key technical decisions:
- Deferred response reuse Phase 12 的 PendingType 枚举扩展
- 消息格式遵循 `meow_mcp:*` 协议
- eval_in_game 的安全限制（如果有的话）
- get_game_scene_tree 的深度限制和返回格式

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `MeowDebuggerPlugin` (game_bridge.h/cpp): PendingType enum pattern from Phase 12
- `meow_mcp_bridge.gd`: _resolve_node helper, message dispatch pattern
- Deferred response mechanism: pending state + callback + queue_deferred_response()
- Phase 12 patterns: click_node/get_node_rect as reference for new deferred tools

### Established Patterns
- C++ tool: register in mcp_tool_registry.cpp → implement in game_bridge.cpp → dispatch in mcp_server.cpp
- GDScript: add message handler in _on_message match → implement handler func → send result via EngineDebugger
- Deferred: PendingType enum + pending_* state variables + _capture() handler

### Integration Points
- game_bridge.h/cpp: New PendingType variants, new tool methods
- meow_mcp_bridge.gd: New message handlers
- mcp_tool_registry.cpp: 3 new tool definitions (tools 41-43)
- mcp_server.cpp: 3 new dispatch branches

</code_context>

<specifics>
## Specific Ideas

No specific requirements — infrastructure phase extending proven patterns.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
