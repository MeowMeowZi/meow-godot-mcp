# Requirements: v1.2 Runtime Interaction Enhancement

## INPT — Input Injection Enhancement

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| INPT-01 | click 动作自动包含 press+release 完整周期，单次调用完成点击 | Must | Done (12-01) |
| INPT-02 | 新增 `click_node` 工具，按节点路径点击运行中游戏的 UI 节点 | Must | Done (12-01) |
| INPT-03 | 新增 `get_node_rect` 工具，获取运行中节点的屏幕坐标和尺寸 | Must | Done (12-01) |

**Motivation:** 当前 click 需要两次调用（press/release），且只能靠截图猜坐标点击

## RTST — Runtime State Query

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| RTST-01 | 新增 `get_game_node_property` 工具，读取运行中游戏节点的属性值 | Must | Done (13-01) |
| RTST-02 | 新增 `eval_in_game` 工具，在运行中游戏执行 GDScript 表达式并返回结果 | Must | Done (13-01) |
| RTST-03 | 新增 `get_game_scene_tree` 工具，获取运行中游戏的场景树结构 | Must | Done (13-01) |

**Motivation:** 当前验证游戏状态只能靠截图"看"，无法程序化断言

## GOUT — Game Output Enhancement

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| GOUT-01 | 游戏启动时自动启用日志捕获（通过 companion script 或 debugger 通道） | Must | Done (14-01) |
| GOUT-02 | 支持结构化日志查询（按级别过滤、按时间范围、关键字搜索） | Should | Done (14-01) |
| GOUT-03 | `print()` 输出实时可用，不依赖 file_logging 项目设置 | Must | Done (14-01) |

**Motivation:** 当前 get_game_output 依赖用户手动开启 file_logging，开箱体验差

## TEST — Integration Testing Toolkit

| ID | Requirement | Priority |
|----|-------------|----------|
| TEST-01 | 新增 `run_test_sequence` 工具，批量执行输入序列并收集结果 | Must | Done (15-01) |
| TEST-02 | 结合 click_node + get_game_node_property 实现 UI 自动化断言 | Must | Done (15-01) |
| TEST-03 | 新增 Prompt 模板：自动测试游戏 UI 工作流 | Should | Done (15-01) |

**Motivation:** 综合前几个 Phase 的能力，提供完整的 AI 自动测试闭环

## Summary

| Category | Must | Should | Total |
|----------|------|--------|-------|
| INPT | 3 | 0 | 3 |
| RTST | 3 | 0 | 3 |
| GOUT | 2 | 1 | 3 |
| TEST | 2 | 1 | 3 |
| **Total** | **10** | **2** | **12** |
