# Phase 12: Input Injection Enhancement - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

增强 inject_input 工具的 click 行为（自动 press+release），新增 click_node（按节点路径点击）和 get_node_rect（获取节点屏幕坐标）两个工具。所有新功能基于现有 EditorDebuggerSession 消息通道。

</domain>

<decisions>
## Implementation Decisions

### Click 自动完成行为
- mouse_action=click 时默认自动发送 press+release 两个事件（无需 opt-in）
- press 和 release 之间加 50ms 延迟，模拟真实快速点击
- 保留 pressed 参数向后兼容：当显式设置 pressed=true/false 时保持单发行为；仅在未显式设置时自动双发

### click_node 工具设计
- click_node 作为独立 MCP 工具，参数简单（只需 node_path）
- 节点路径使用场景树相对路径（如 "BackpackUI/BtnSearch"），与 get_scene_tree 输出一致
- 使用异步 deferred 模式：游戏端解析路径→获取坐标→注入点击→返回结果确认

### get_node_rect 响应设计
- 返回 viewport 坐标空间，与 inject_input 的 position 参数一致，可直接用于点击
- 返回 position + size + global_position（完整 Rect2 加全局位置）
- 节点不存在或不可见时返回 error，明确报错原因（not found / not visible / not Control）

### Claude's Discretion
- 50ms 延迟的具体实现方式（C++ 端 timer 或游戏端延迟）
- click_node 的 deferred 响应是否复用 viewport capture 的 deferred 机制或新建独立机制
- 单元测试的具体覆盖范围

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `MeowDebuggerPlugin` (game_bridge.h/cpp): EditorDebuggerPlugin 子类，管理与运行中游戏的通信
- `meow_mcp_bridge.gd`: 游戏端 autoload，处理 `meow_mcp:*` 消息，注入 InputEvent
- 现有 deferred 响应机制（viewport capture）：pending state + callback pattern
- `inject_input_tool()` (game_bridge.cpp:167-282): 现有输入注入实现

### Established Patterns
- 消息格式：`"meow_mcp:<action>"` + Array 数据
- 输入注入：fire-and-forget（editor 发消息，game 端执行后发 `input_result` 确认）
- Deferred 响应：pending state tracking + callback + queue_deferred_response()
- 工具注册：mcp_tool_registry.cpp 的 ToolDef 数组 + 版本过滤

### Integration Points
- game_bridge.cpp: 新增 click_node_tool() 和 get_node_rect_tool() 方法
- meow_mcp_bridge.gd: 新增消息处理函数
- mcp_tool_registry.cpp: 注册新工具定义
- mcp_server.cpp: dispatch 新工具到 game_bridge

</code_context>

<specifics>
## Specific Ideas

- 50ms 延迟用户明确要求，确保 Godot UI 控件能正确响应 pressed/released 事件循环
- click_node 应返回实际点击的坐标（用于调试和验证）

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
