# Phase 14: Game Output Enhancement - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

增强游戏日志捕获能力：自动启用日志捕获（不依赖 file_logging 项目设置）、支持结构化日志查询、print() 输出实时可用。通过 EditorDebugger 通道替代文件日志方案。

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — infrastructure phase. Key decisions:
- 日志捕获机制选择（debugger message 通道 vs 其他方案）
- 结构化日志查询的 API 设计（过滤参数、时间范围等）
- 日志缓冲策略（内存中保留多少日志）
- 是否保留现有 file_logging 方案作为 fallback

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `runtime_tools.cpp`: 现有 get_game_output() 实现（基于文件日志读取）
- `meow_mcp_bridge.gd`: EditorDebugger 消息通道基础设施
- `game_bridge.h/cpp`: deferred response pattern from Phase 12/13
- Phase 12/13 新增的 PendingType 枚举模式

### Established Patterns
- 游戏端通过 EngineDebugger.send_message 发送数据到编辑器
- 编辑器端通过 _capture() 处理接收到的消息
- 工具注册 + dispatch 标准流程

### Integration Points
- runtime_tools.cpp: 改造 get_game_output 或新增增强版
- game_bridge.h/cpp: 日志接收和缓冲
- meow_mcp_bridge.gd: 日志拦截和转发
- mcp_tool_registry.cpp / mcp_server.cpp: 新/改工具注册和 dispatch

</code_context>

<specifics>
## Specific Ideas

- 核心问题：当前 get_game_output 依赖 `debug/file_logging/enable_file_logging` 项目设置
- 目标：通过 debugger 通道直接捕获 print() 输出，无需任何项目设置
- Godot 的 EngineDebugger 提供了 output 捕获能力，可能通过 `EngineDebugger.send_message` 拦截 print 输出

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
