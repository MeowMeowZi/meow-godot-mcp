# Godot MCP Meow

## What This Is

一个 Godot 引擎的 MCP (Model Context Protocol) Server 插件，以 GDExtension (C++/godot-cpp) 形式构建。它让 AI 工具（如 Claude、Cursor）能够通过 MCP 协议直接与 Godot 编辑器通信，实现场景查询、节点操作、脚本管理、运行时控制和信号连接等全方位编辑器辅助开发能力。零外部依赖，即插即用。

## Core Value

AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发。

## Requirements

### Validated

- ✓ MCP Server 核心：stdio bridge + TCP relay 双进程架构 — v1.0
- ✓ 场景查询：AI 可读取场景树结构、节点属性、类型 — v1.0
- ✓ 场景 CRUD：AI 可创建/修改/删除节点，集成 UndoRedo — v1.0
- ✓ Variant 类型解析：字符串自动转换为 Vector2/3、Color 等 Godot 类型 — v1.0
- ✓ 脚本管理：AI 可读/写/编辑 GDScript，附加/分离脚本 — v1.0
- ✓ 项目管理：AI 可查询文件结构、项目设置、资源信息 — v1.0
- ✓ MCP Resources：场景树和项目结构通过 Resources 协议暴露 — v1.0
- ✓ 编辑器 Dock 面板：实时连接状态、Start/Stop/Restart 控制 — v1.0
- ✓ 版本适配：运行时检测 Godot 版本，动态启用/禁用 MCP 工具 — v1.0
- ✓ MCP Prompts：4 个预建工作流模板 — v1.0
- ✓ 运行时控制：AI 可启动/停止游戏、捕获日志输出 — v1.0
- ✓ 信号管理：AI 可查询/连接/断开节点信号 — v1.0
- ✓ 跨平台分发：Windows/Linux/macOS 预编译二进制 + CI/CD — v1.0

### Active

（暂无 — 使用 `/gsd:new-milestone` 开始下一个里程碑）

### Shipped

**v1.5 — AI 工作流增强 (shipped 2026-03-24)**

- ✓ 智能错误处理：MCP isError 协议、Levenshtein 模糊匹配、10 类错误分类 + 恢复引导
- ✓ Resources 增强：场景树内联脚本/信号/属性、3 个 URI 资源模板、项目文件元数据
- ✓ 5 个复合工具：find_nodes、batch_set_property、create_character、create_ui_panel、duplicate_node
- ✓ 8 个新 Prompt 模板：工具组合指南、调试工作流、游戏搭建工作流

**v1.4 — 2D 游戏开发核心工具 (shipped 2026-03-22)**

- ✓ 资源属性支持：res:// 路径加载资源、new:ClassName() 内联创建资源
- ✓ TileMap 操作：批量放置/擦除瓦片、查询瓦片信息
- ✓ 碰撞形状快速创建：一步创建 CollisionShape2D/3D 并配置形状
- ✓ 编辑器重启：restart_editor 工具

**v1.3 — 开发者体验优化 (shipped 2026-03-22)**

- ✓ run_game 自动等待 bridge 连接（消除手动 sleep + 轮询）
- ✓ 所有工具统一根节点路径约定（"" 和 "." 均表示场景根节点）
- ✓ game output 可靠捕获（companion 端日志转发）
- ✓ set_layout_preset 支持根节点操作

**v1.2 — 运行时交互增强 (shipped 2026-03-20)**

- ✓ 输入注入增强：click 自动 press+release、click_node 按路径点击、get_node_rect 获取坐标
- ✓ 运行时状态查询：get_game_node_property、eval_in_game、get_game_scene_tree
- ✓ 游戏日志自动化：debugger 通道捕获 print()，支持 level/since/keyword 过滤
- ✓ 集成测试工具链：run_test_sequence 批量测试、test_game_ui prompt 模板

**v1.1 — UI 系统 + 编辑器能力拓展 (shipped 2026-03-19)**

- ✓ UI 系统：Control 节点层级、Theme/StyleBox 管理、Container 布局
- ✓ Animation 系统：AnimationPlayer 轨道编辑、关键帧管理
- ✓ 场景文件管理：保存/加载/切换场景
- ✓ 输入注入：向运行中的游戏注入键盘/鼠标输入
- ✓ 视口截图：捕获游戏视口截图用于视觉反馈
- ✓ 更多 Prompt 模板：UI 构建、动画设置等常见工作流

### Out of Scope

- 游戏运行时 AI 集成（NPC/行为树）— 本项目专注编辑器辅助
- GDScript LSP 替代 — Godot 内置 LSP 已足够
- C#/Rust 绑定 — 仅用 C++ godot-cpp
- Godot 3.x 支持 — GDExtension 仅限 4.x
- 多编辑器协作 — 复杂度极高
- 自定义 LLM/模型托管 — MCP 是传输层协议
- 插件市场/付费基础设施 — 开源项目，MIT 许可证

## Context

**Current State (v1.5 shipped):**
- C++17 + GDScript, godot-cpp v10+, nlohmann/json 3.12.0, SCons + CMake
- 55 MCP tools, 15 prompt templates, 5 MCP Resources (2 static + 3 URI templates)
- 25 phases shipped across 6 milestones (v1.0 → v1.1 → v1.2 → v1.3 → v1.4 → v1.5)
- GoogleTest unit tests (119+) + automated UAT suites (Python)
- GitHub Actions CI/CD for 3 platforms
- Minimum Godot version: 4.3 (godot-cpp v10)

**Competitive landscape:** 14+ existing Godot MCP servers — ALL use Node.js/Python + WebSocket. Zero-dependency GDExtension is the core differentiator.

## Constraints

- **Tech stack**: C++17 + godot-cpp — GDExtension 官方推荐方式
- **Transport**: stdio via bridge relay — MCP 标准传输，兼容 Claude Desktop 等 AI 工具
- **Compatibility**: Godot 4.3+ — godot-cpp v10 最低版本要求
- **Distribution**: Godot addon 形式分发 — 符合 Asset Library 结构规范
- **Threading**: IO 线程 + 主线程队列模式 — Godot 场景树非线程安全

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| C++ GDExtension 而非 GDScript 插件 | 性能更好，能访问更底层 API，适合 MCP Server 长连接场景 | ✓ Good — 零依赖，编译到 ~600KB DLL |
| stdio bridge + TCP relay 双进程 | GDExtension 无法被 AI 客户端直接 spawn，且共享 Godot stdout | ✓ Good — bridge 仅 ~50KB，架构清晰 |
| 版本自适应工具注册表 | 最大化用户覆盖，4.3+ 用户都能用 | ✓ Good — ToolDef 数组 + 版本过滤 |
| 自建 MCP 协议层 (~800 LOC) | 现有 C++ SDK 要么过期要么太重 | ✓ Good — 完全掌控，spec 兼容 |
| nlohmann/json 而非手动解析 | 广泛使用的 header-only 库，可靠 | ✓ Good — 零问题 |
| GoogleTest 单元测试 + Python UAT | 分层测试：协议逻辑用 C++ 测，端到端用 Python 测 | ✓ Good — 132 + 5 套 UAT |
| 手动构建 GDScript 替代 ResourceLoader::load() | ResourceLoader 对新建 .gd 文件会崩溃 | ✓ Good — 修复了 attach_script 崩溃 |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-03-24 after v1.5 milestone complete*
