# Roadmap: Godot MCP Meow

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-18)
- v1.1 UI & Editor Expansion -- Phases 6-11 (shipped 2026-03-19)
- v1.2 Runtime Interaction Enhancement -- Phases 12-15 (planning)

## Phases

<details>
<summary>v1.0 MVP (Phases 1-5) -- SHIPPED 2026-03-18</summary>

- [x] Phase 1: Foundation & First Tool (4/4 plans) -- completed 2026-03-16
- [x] Phase 2: Scene CRUD (3/3 plans) -- completed 2026-03-16
- [x] Phase 3: Script & Project Management (3/3 plans) -- completed 2026-03-17
- [x] Phase 4: Editor Integration (2/2 plans) -- completed 2026-03-18
- [x] Phase 5: Runtime, Signals & Distribution (3/3 plans) -- completed 2026-03-18

**31/31 requirements | 18 tools | 132 unit tests | 5 UAT suites**

See `.planning/milestones/v1.0-ROADMAP.md` for full details.

</details>

<details>
<summary>v1.1 UI & Editor Expansion (Phases 6-11) -- SHIPPED 2026-03-19</summary>

- [x] Phase 6: Scene File Management (3/3 plans) -- completed 2026-03-18
- [x] Phase 7: UI System (3/3 plans) -- completed 2026-03-18
- [x] Phase 8: Animation System (3/3 plans) -- completed 2026-03-18
- [x] Phase 9: Editor Viewport Screenshots (3/3 plans) -- completed 2026-03-18
- [x] Phase 10: Running Game Bridge (3/3 plans) -- completed 2026-03-18
- [x] Phase 11: Prompt Templates (1/1 plan) -- completed 2026-03-19

**27/27 requirements | 20 new tools (38 total) | 160 unit tests | 6 UAT suites**

See `.planning/milestones/v1.1-ROADMAP.md` for full details.

</details>

### v1.2 Runtime Interaction Enhancement (In Progress)

**Milestone Goal:** 增强 AI 与运行中游戏的交互能力，实现精确 UI 操控、运行时状态查询、自动化测试验证，从"能控制编辑器"升级为"能测试和调试游戏"。

- [x] **Phase 12: Input Injection Enhancement** - click 自动 press+release，按节点路径点击 UI
- [x] **Phase 13: Runtime State Query** - 读取运行中游戏的节点属性和变量值
- [x] **Phase 14: Game Output Enhancement** - 自动化日志捕获，无需手动配置
- [ ] **Phase 15: Integration Testing Toolkit** - 端到端自动测试工作流

## Phase Details

### Phase 12: Input Injection Enhancement
**Goal**: inject_input click 自动完成按下+释放完整周期；新增按节点路径点击和获取节点坐标工具
**Depends on**: Phase 10 (Running Game Bridge)
**Requirements**: INPT-01, INPT-02, INPT-03
**Plans:** 2/2 plans complete
Plans:
- [x] 12-01-PLAN.md — Core implementation: auto-cycle click, click_node tool, get_node_rect tool (C++ + GDScript)
- [x] 12-02-PLAN.md — UAT test suite and end-to-end verification
**Success Criteria** (what must be TRUE):
  1. inject_input 的 click 动作自动包含 press+release 完整周期，单次调用完成点击
  2. AI 可通过 click_node 工具按节点路径点击运行中游戏的 UI 节点
  3. AI 可通过 get_node_rect 工具获取运行中节点的屏幕坐标和尺寸

### Phase 13: Runtime State Query
**Goal**: AI 可读取运行中游戏的节点属性、执行 GDScript 表达式、获取运行时场景树
**Depends on**: Phase 12
**Requirements**: RTST-01, RTST-02, RTST-03
**Plans:** 2/2 plans complete
Plans:
- [x] 13-01-PLAN.md — Core implementation: get_game_node_property, eval_in_game, get_game_scene_tree (C++ + GDScript + registry + dispatch)
- [x] 13-02-PLAN.md — UAT test suite, build verification, and end-to-end human verification
**Success Criteria** (what must be TRUE):
  1. AI 可通过 get_game_node_property 读取运行中游戏任意节点的属性值
  2. AI 可通过 eval_in_game 在运行中游戏执行 GDScript 表达式并返回结果
  3. AI 可通过 get_game_scene_tree 获取运行中游戏的完整场景树结构

### Phase 14: Game Output Enhancement
**Goal**: 游戏日志自动捕获，无需手动配置项目设置，支持结构化查询
**Depends on**: Phase 12
**Requirements**: GOUT-01, GOUT-02, GOUT-03
**Plans:** 2/2 plans complete
Plans:
- [x] 14-01-PLAN.md — Core implementation: debugger-channel log capture, log buffer, enhanced get_game_output with structured filtering
- [x] 14-02-PLAN.md — UAT test suite, build verification, and end-to-end human verification
**Success Criteria** (what must be TRUE):
  1. 游戏启动时自动启用日志捕获（通过 companion script 或 debugger 通道）
  2. 支持结构化日志查询（按级别过滤、按时间范围、关键字搜索）
  3. print() 输出实时可用，不依赖 file_logging 项目设置

### Phase 15: Integration Testing Toolkit
**Goal**: 综合输入注入、状态查询、日志捕获能力，提供完整的 AI 自动测试闭环
**Depends on**: Phase 13, Phase 14
**Requirements**: TEST-01, TEST-02, TEST-03
**Plans:** 2 plans
Plans:
- [ ] 15-01-PLAN.md — Core implementation: run_test_sequence tool (async state machine) + test_game_ui prompt template
- [ ] 15-02-PLAN.md — UAT test suite (15 tests) and end-to-end human verification
**Success Criteria** (what must be TRUE):
  1. AI 可通过 run_test_sequence 批量执行输入序列并收集结果
  2. 结合 click_node + get_game_node_property 实现 UI 自动化断言
  3. 新增 Prompt 模板：自动测试游戏 UI 工作流

## Progress

| Phase | Milestone | Plans | Status | Completed |
|-------|-----------|-------|--------|-----------|
| 1. Foundation & First Tool | v1.0 | 4/4 | Complete | 2026-03-16 |
| 2. Scene CRUD | v1.0 | 3/3 | Complete | 2026-03-16 |
| 3. Script & Project Management | v1.0 | 3/3 | Complete | 2026-03-17 |
| 4. Editor Integration | v1.0 | 2/2 | Complete | 2026-03-18 |
| 5. Runtime, Signals & Distribution | v1.0 | 3/3 | Complete | 2026-03-18 |
| 6. Scene File Management | v1.1 | 3/3 | Complete | 2026-03-18 |
| 7. UI System | v1.1 | 3/3 | Complete | 2026-03-18 |
| 8. Animation System | v1.1 | 3/3 | Complete | 2026-03-18 |
| 9. Editor Viewport Screenshots | v1.1 | 3/3 | Complete | 2026-03-18 |
| 10. Running Game Bridge | v1.1 | 3/3 | Complete | 2026-03-18 |
| 11. Prompt Templates | v1.1 | 1/1 | Complete | 2026-03-19 |
| 12. Input Injection Enhancement | v1.2 | Complete    | 2026-03-19 | 2026-03-20 |
| 13. Runtime State Query | v1.2 | Complete    | 2026-03-19 | 2026-03-20 |
| 14. Game Output Enhancement | v1.2 | Complete    | 2026-03-19 | 2026-03-20 |
| 15. Integration Testing Toolkit | v1.2 | 0/2 | Planning | - |
