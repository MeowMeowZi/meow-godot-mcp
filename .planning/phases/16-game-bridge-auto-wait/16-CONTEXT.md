# Phase 16: Game Bridge Auto-Wait - Context

**Gathered:** 2026-03-22
**Status:** Ready for planning

<domain>
## Phase Boundary

两个改进：(1) run_game 增加 wait_for_bridge 参数，内部轮询 bridge 连接状态直到就绪或超时；(2) 所有接受 node_path 的工具统一支持 "" 和 "." 表示场景根节点。

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — pure infrastructure phase.
- wait_for_bridge 的默认超时时间
- 轮询间隔
- 根节点路径统一的具体实现方式（dispatch 层统一处理 vs 每个工具单独处理）

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `runtime_tools.cpp run_game()` — 当前游戏启动逻辑
- `game_bridge.cpp is_game_connected()` — bridge 连接状态检查
- `mcp_server.cpp` — 所有工具 dispatch 逻辑，node_path 参数提取

### Established Patterns
- C++ 工具方法返回 nlohmann::json 结果
- node_path 参数在 dispatch 层从 params["arguments"] 提取
- Phase 12 已修复 click_node/get_node_rect 的空 node_path 问题（has_node_path flag）

### Integration Points
- runtime_tools.cpp: run_game 增加 wait_for_bridge 逻辑
- mcp_server.cpp: 统一 node_path 处理
- mcp_tool_registry.cpp: run_game schema 增加 wait_for_bridge 参数

</code_context>

<specifics>
## Specific Ideas

- wait_for_bridge 可以在 run_game 返回前用主线程轮询（call_deferred 或 timer），但需注意不能阻塞 Godot 主线程太久
- 或者用 deferred response 模式：run_game 返回 __deferred，bridge 连接后再回调

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
