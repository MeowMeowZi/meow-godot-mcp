# Phase 17: Reliable Game Output - Context

**Gathered:** 2026-03-22
**Status:** Ready for planning

<domain>
## Phase Boundary

确保 get_game_output 在所有场景下可靠捕获 print() 输出（≤1秒延迟）。当前方案依赖 file_logging + std::ifstream 共享读取，但在某些时序下返回空结果。需要改用 companion script 主动转发 print 输出通过 debugger 通道。

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — infrastructure phase.
- companion script 转发机制的具体实现（覆写 _print handler vs 定期轮询）
- 是否保留 file_logging 方案作为 fallback
- 日志缓冲大小限制

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `game_bridge.cpp` — LogEntry struct, log_buffer, get_buffered_game_output()
- `meow_mcp_bridge.gd` — companion script, EngineDebugger message channel
- `runtime_tools.cpp` — file-based get_game_output() with std::ifstream
- `mcp_server.cpp` — dispatch 逻辑，已有 buffer 优先 + file fallback

### Established Patterns
- Game → Editor 通信：EngineDebugger.send_message("meow_mcp:*", data)
- Editor _capture() 接收并缓冲
- 但 Godot 4.6 的内置 "output" 消息不转发给 EditorDebuggerPlugin

### Integration Points
- meow_mcp_bridge.gd: 新增 print 输出转发机制
- game_bridge.cpp: _capture 接收转发的日志
- mcp_server.cpp: dispatch 优化

</code_context>

<specifics>
## Specific Ideas

- 在 companion script 中 hook `_notification(NOTIFICATION_PROCESS)` 每帧检查 OS 日志... 不可行
- 更好方案：companion script 提供自定义 print 函数，游戏代码无需修改
- 最佳方案：在 companion script 的 _ready 中用 add_handler 注册自定义 logger（如果 GDScript 支持）

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
