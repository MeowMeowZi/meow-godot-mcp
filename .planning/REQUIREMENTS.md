# Requirements: Godot MCP Meow v1.6

**Defined:** 2026-03-31
**Core Value:** AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发

## v1.6 Requirements

Requirements for v1.6 MCP 细节优化。每条需求映射到 roadmap phase。

### Settings Persistence (设置持久化)

- [x] **PERSIST-01**: 用户在 Dock 面板设置的端口号持久化到 project.godot，编辑器重启后自动恢复
- [x] **PERSIST-02**: 用户在 Dock 面板禁用的工具列表持久化到 project.godot，编辑器重启后自动恢复
- [x] **PERSIST-03**: 端口被占用时直接报错并提示用户，不再静默自增端口号

### Timeout Safety (超时安全)

- [ ] **TIMEOUT-01**: MCP 工具调用在 30 秒内未收到响应时，IO 线程返回 JSON-RPC 超时错误给 AI 客户端
- [ ] **TIMEOUT-02**: Game bridge 延迟请求（视口截图、eval_in_game 等）在 15 秒内未收到游戏端响应时，返回超时错误
- [ ] **TIMEOUT-03**: 超时后收到的迟到响应被正确丢弃，不会污染下一个请求的响应

### Logging & Cleanup (日志与清理)

- [ ] **LOG-01**: 插件错误日志同时出现在 Godot 的 Output 面板和 Debugger Errors 面板
- [ ] **CLEAN-01**: error_enrichment.cpp 中已删除工具（59→30 精简后的遗留）的 TOOL_PARAM_HINTS 条目被移除

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### MCP Sampling

- **SAMP-01**: Server 可通过 MCP Sampling 请求 AI 客户端分析当前场景状态
- **SAMP-02**: `analyze_scene` 工具利用 Sampling 自动检测常见问题

### Advanced Composites

- **ACOMP-01**: `create_scene_from_template` 从游戏类型模板创建完整场景
- **ACOMP-02**: Scene diff resource（场景变更差异追踪）

### Resource Subscriptions

- **RSUB-01**: 支持 MCP resources/subscribe 实时场景变更通知

## Out of Scope

| Feature | Reason |
|---------|--------|
| MCP cancellation 协议支持 | 目前没有 AI 客户端实际发送 cancellation 通知 |
| 国际化 (i18n) UI | 当前用户群以中文为主，推迟到需求出现时 |
| Game bridge 请求管线化（多并发） | 架构改动大，当前单请求模式够用 |
| 非 Windows 平台 bridge 父进程看门狗 | Windows 已有，其他平台推迟 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| PERSIST-01 | Phase 26 | Complete |
| PERSIST-02 | Phase 26 | Complete |
| PERSIST-03 | Phase 26 | Complete |
| TIMEOUT-01 | Phase 27 | Pending |
| TIMEOUT-02 | Phase 27 | Pending |
| TIMEOUT-03 | Phase 27 | Pending |
| LOG-01 | Phase 28 | Pending |
| CLEAN-01 | Phase 28 | Pending |

**Coverage:**
- v1.6 requirements: 8 total
- Mapped to phases: 8
- Unmapped: 0

---
*Requirements defined: 2026-03-31*
*Last updated: 2026-03-31 after roadmap creation*
