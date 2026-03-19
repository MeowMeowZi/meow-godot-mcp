# Phase 15: Integration Testing Toolkit - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

综合 Phase 12-14 的能力（输入注入、状态查询、日志捕获），构建集成测试工具链：run_test_sequence 批量测试工具、UI 自动化断言组合、测试 Prompt 模板。

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion
All implementation choices are at Claude's discretion — this is the capstone phase combining prior phases' tools:
- run_test_sequence 的 API 设计（步骤格式、断言语法、结果收集）
- UI 自动化断言的实现方式（基于 click_node + get_game_node_property 组合）
- Prompt 模板的结构和覆盖场景
- 是否需要新的 C++ 工具还是纯 MCP Prompt 模板 + 文档

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- Phase 12: inject_input (auto-cycle), click_node, get_node_rect
- Phase 13: get_game_node_property, eval_in_game, get_game_scene_tree
- Phase 14: get_game_output (debugger-channel, structured filtering)
- Phase 11: Prompt template system (prompt_tools.cpp)
- 现有 43 个 MCP 工具

### Established Patterns
- 工具注册 + dispatch 标准流程
- Deferred response for game-side operations
- Prompt 模板：generate_prompt 函数返回步骤列表

### Integration Points
- mcp_tool_registry.cpp: run_test_sequence 工具定义
- mcp_server.cpp / game_bridge.cpp: dispatch + 实现
- prompt_tools.cpp: 新 Prompt 模板
- meow_mcp_bridge.gd: 可能需要游戏端支持

</code_context>

<specifics>
## Specific Ideas

- run_test_sequence 应该接收一个步骤数组，每步包含 action（click_node/inject_input/wait）+ assert（get_game_node_property check）
- 结果应该是结构化的 pass/fail 报告
- Prompt 模板应该引导 AI 完成典型的 UI 测试流程

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>
