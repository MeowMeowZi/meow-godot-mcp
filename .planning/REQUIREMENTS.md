# Requirements: Godot MCP Meow v1.5

**Defined:** 2026-03-23
**Core Value:** AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发

## v1.5 Requirements

Requirements for v1.5 AI 工作流增强 milestone. Each maps to roadmap phases.

### Smart Error Handling

- [ ] **ERR-01**: AI 收到的工具错误响应使用 MCP `isError: true` 标志（而非普通文本结果）
- [ ] **ERR-02**: 节点未找到时，错误信息包含模糊匹配建议（相似节点名 + 父节点的子节点列表）
- [ ] **ERR-03**: "无场景打开"和"游戏未运行"错误包含具体下一步操作引导
- [ ] **ERR-04**: 缺少必需参数时，错误信息包含参数格式示例和使用说明
- [ ] **ERR-05**: 前置条件不满足时，错误信息包含应先调用的工具（如 "先调 run_game"）
- [ ] **ERR-06**: 属性类型不匹配时，错误信息包含期望格式和示例（如 "Vector2(100, 200)"）
- [ ] **ERR-07**: 错误响应包含建议的恢复工具列表（suggested_tools）
- [ ] **ERR-08**: 脚本解析错误包含具体出错行号和行内容

### Composite Tools

- [ ] **COMP-01**: AI 可使用 `find_nodes` 按类型、名称模式、属性值搜索场景树中的节点
- [ ] **COMP-02**: AI 可使用 `batch_set_property` 批量设置多个节点的属性（按路径列表或类型过滤）
- [ ] **COMP-03**: AI 可使用 `create_character` 一步创建角色（CharacterBody + CollisionShape + 视觉节点），整个操作为单个 UndoRedo action
- [ ] **COMP-04**: AI 可使用 `create_ui_panel` 从声明式 JSON 规格创建 UI 面板（容器 + 子节点 + 样式），单个 UndoRedo action
- [ ] **COMP-05**: AI 可使用 `duplicate_node` 深拷贝节点子树到新父节点，包含所有子节点和属性

### Enriched Resources

- [ ] **RES-01**: 场景树资源（scene_tree）包含每个节点的脚本源码、信号连接和关键属性值
- [ ] **RES-02**: 支持参数化资源模板（godot://node/{path}、godot://signals/{path}、godot://script/{path}），AI 可按需查询单个节点/脚本/信号详情
- [ ] **RES-03**: 项目文件资源包含文件大小、类型分类（场景/脚本/资源/图片）和修改时间戳

### Prompt Templates

- [ ] **PROMPT-01**: `tool_composition_guide` 模板——工具组合速查卡，教 AI 如何组合工具完成常见任务
- [ ] **PROMPT-02**: `debug_game_crash` 模板——游戏崩溃时的系统性排查工作流
- [ ] **PROMPT-03**: `build_platformer_game` 模板——从零搭建 2D 平台跳跃游戏的完整流程
- [ ] **PROMPT-04**: `setup_tilemap_level` 模板——TileMap 关卡搭建工作流
- [ ] **PROMPT-05**: `build_top_down_game` 模板——从零搭建俯视角游戏的完整流程
- [ ] **PROMPT-06**: `debug_physics_issue` 模板——物理问题专项调试工作流
- [ ] **PROMPT-07**: `create_game_from_scratch` 模板——按游戏类型参数化的全流程建游戏指南
- [ ] **PROMPT-08**: `fix_common_errors` 模板——常见 MCP 工具错误的恢复指南

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### MCP Sampling

- **SAMP-01**: Server 可通过 MCP Sampling 请求 AI 客户端分析当前场景状态
- **SAMP-02**: `analyze_scene` 工具利用 Sampling 自动检测常见问题（缺少碰撞体、未连接信号等）

### Advanced Composites

- **ACOMP-01**: `create_scene_from_template` 从游戏类型模板创建完整场景（玩家 + 关卡 + 相机 + HUD）
- **ACOMP-02**: Scene diff resource（场景变更差异追踪）

### Resource Subscriptions

- **RSUB-01**: 支持 MCP resources/subscribe 实时场景变更通知

## Out of Scope

| Feature | Reason |
|---------|--------|
| "Do everything" 万能工具 | 单个工具接受自由指令，schema 不可控，AI 选择不可靠 |
| 自动重试逻辑 | 违反 MCP 的 model-controlled 原则，AI 应决定是否重试 |
| 复合工具内嵌完整 GDScript 模板 | C++ 内维护 GDScript 脆弱且过时快，AI 应自行生成脚本 |
| 资源订阅推送（v1.5） | 实现复杂且编辑器场景收益不大，留 v2+ |
| 二进制资源（图片/音频）作为 MCP Resource | 浪费上下文，AI 无法处理原始字节 |
| 超过 15 个 Prompt 模板 | 太多选项造成选择困难，保持精炼 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| ERR-01 | Phase 22 | Pending |
| ERR-02 | Phase 22 | Pending |
| ERR-03 | Phase 22 | Pending |
| ERR-04 | Phase 22 | Pending |
| ERR-05 | Phase 22 | Pending |
| ERR-06 | Phase 22 | Pending |
| ERR-07 | Phase 22 | Pending |
| ERR-08 | Phase 22 | Pending |
| RES-01 | Phase 23 | Pending |
| RES-02 | Phase 23 | Pending |
| RES-03 | Phase 23 | Pending |
| COMP-01 | Phase 24 | Pending |
| COMP-02 | Phase 24 | Pending |
| COMP-03 | Phase 24 | Pending |
| COMP-04 | Phase 24 | Pending |
| COMP-05 | Phase 24 | Pending |
| PROMPT-01 | Phase 25 | Pending |
| PROMPT-02 | Phase 25 | Pending |
| PROMPT-03 | Phase 25 | Pending |
| PROMPT-04 | Phase 25 | Pending |
| PROMPT-05 | Phase 25 | Pending |
| PROMPT-06 | Phase 25 | Pending |
| PROMPT-07 | Phase 25 | Pending |
| PROMPT-08 | Phase 25 | Pending |

**Coverage:**
- v1.5 requirements: 24 total
- Mapped to phases: 24
- Unmapped: 0

---
*Requirements defined: 2026-03-23*
*Last updated: 2026-03-23 after roadmap creation*
