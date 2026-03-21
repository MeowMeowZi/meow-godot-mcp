# Phase 18: Tool Ergonomics - Context

**Gathered:** 2026-03-22
**Status:** Ready for planning

<domain>
## Phase Boundary

修复 set_layout_preset 不支持根节点的问题。可能包含其他在实际使用中发现的工具易用性改进。

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — infrastructure phase.
- set_layout_preset 的根节点路径处理方式
- 是否有其他工具需要同样的修复

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `mcp_server.cpp` — set_layout_preset dispatch
- `ui_tools.cpp` — set_layout_preset 实现
- Phase 16 的根节点路径统一逻辑可直接复用

### Established Patterns
- Phase 12 修复了 click_node/get_node_rect 的空 node_path 问题
- Phase 16 会统一所有工具的根节点路径处理

### Integration Points
- mcp_server.cpp: set_layout_preset dispatch 修复
- 可能需要检查其他工具是否有同样问题

</code_context>

<specifics>
## Specific Ideas

No specific requirements — infrastructure phase extending Phase 16 patterns.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
