# Roadmap: Godot MCP Meow

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-18)
- v1.1 UI & Editor Expansion -- Phases 6-11 (shipped 2026-03-19)
- v1.2 Runtime Interaction Enhancement -- Phases 12-15 (shipped 2026-03-20)
- v1.3 Developer Experience Polish -- Phases 16-18 (planning)

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

<details>
<summary>v1.2 Runtime Interaction Enhancement (Phases 12-15) -- SHIPPED 2026-03-20</summary>

- [x] Phase 12: Input Injection Enhancement (2/2 plans) -- completed 2026-03-20
- [x] Phase 13: Runtime State Query (2/2 plans) -- completed 2026-03-20
- [x] Phase 14: Game Output Enhancement (2/2 plans) -- completed 2026-03-20
- [x] Phase 15: Integration Testing Toolkit (2/2 plans) -- completed 2026-03-20

**12/12 requirements | 6 new tools (44 total) | 1 new prompt (7 total) | 4 UAT suites**

See `.planning/milestones/v1.2-ROADMAP.md` for full details.

</details>

### v1.3 Developer Experience Polish (In Progress)

**Milestone Goal:** 消除 MCP 工具使用中的摩擦点，让 AI 辅助游戏开发的工作流更顺畅。

- [x] **Phase 16: Game Bridge Auto-Wait** - run_game 自动等待 bridge 连接；根节点路径统一 -- completed 2026-03-22
- [ ] **Phase 17: Reliable Game Output** - game output 可靠捕获，不依赖 file_logging 刷新时机
- [ ] **Phase 18: Tool Ergonomics** - set_layout_preset 等工具支持根节点；其他易用性改进

## Phase Details

### Phase 16: Game Bridge Auto-Wait
**Goal**: run_game 返回时 bridge 已就绪，所有工具统一根节点路径约定
**Depends on**: Phase 15
**Requirements**: DX-01, DX-02
**Plans:** 2/2 plans complete
Plans:
- [x] 16-01-PLAN.md — Deferred wait_for_bridge + unified node_path root handling
- [x] 16-02-PLAN.md — UAT test suite for DX-01 and DX-02
**Success Criteria** (what must be TRUE):
  1. run_game 带 wait_for_bridge 参数时，返回时 bridge 已连接（或超时报错）
  2. 所有接受 node_path 的工具统一支持 "" 和 "." 表示场景根节点

### Phase 17: Reliable Game Output
**Goal**: print() 输出在所有场景下都能被 get_game_output 可靠捕获
**Depends on**: Phase 16
**Requirements**: DX-03
**Success Criteria** (what must be TRUE):
  1. 游戏中的 print() 调用在 1 秒内可通过 get_game_output 获取
  2. 不依赖 file_logging 项目设置的刷新时机

### Phase 18: Tool Ergonomics
**Goal**: 修复工具易用性问题，减少常见操作的调用次数
**Depends on**: Phase 16
**Requirements**: DX-04
**Success Criteria** (what must be TRUE):
  1. set_layout_preset 可对场景根节点使用
  2. 常见 UI 构建流程的工具调用次数减少

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
| 12. Input Injection Enhancement | v1.2 | 2/2 | Complete | 2026-03-20 |
| 13. Runtime State Query | v1.2 | 2/2 | Complete | 2026-03-20 |
| 14. Game Output Enhancement | v1.2 | 2/2 | Complete | 2026-03-20 |
| 15. Integration Testing Toolkit | v1.2 | 2/2 | Complete | 2026-03-20 |
| 16. Game Bridge Auto-Wait | v1.3 | Complete    | 2026-03-21 | 2026-03-22 |
| 17. Reliable Game Output | v1.3 | - | Planned | - |
| 18. Tool Ergonomics | v1.3 | - | Planned | - |
