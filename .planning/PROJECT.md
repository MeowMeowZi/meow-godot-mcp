# Godot MCP Meow

## What This Is

一个 Godot 引擎的 MCP (Model Context Protocol) Server 插件，以 GDExtension (C++/godot-cpp) 形式构建。它让 AI 工具（如 Claude）能够通过 MCP 协议直接与 Godot 编辑器通信，实现场景查询、节点操作、脚本管理等编辑器辅助开发能力。面向开源社区发布，支持 Godot 4.0+ 并按版本自动适配可用功能。

## Core Value

AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发。

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] MCP Server 核心：通过 stdio 实现 MCP 协议通信
- [ ] 场景查询：AI 可读取当前场景树结构、节点属性、节点类型
- [ ] 节点创建：AI 可在场景中创建新节点并设置类型和属性
- [ ] 节点修改：AI 可修改现有节点的属性（transform、名称等）
- [ ] 节点删除：AI 可删除场景中的节点
- [ ] 编辑器插件面板：提供 MCP 服务状态显示和开关控制
- [ ] GDExtension 打包：可作为 addon 分发，用户拖入项目即可使用
- [ ] 脚本管理：AI 可生成/编辑 GDScript 并附加到节点
- [ ] 资源管理：AI 可查询和组织项目资源文件
- [ ] 项目信息查询：AI 可读取项目结构、设置、文件列表等
- [ ] 运行/调试控制：AI 可启动游戏、读取日志输出
- [ ] 信号/连接管理：AI 可查询和创建节点间的信号连接
- [ ] 项目构建/导出：AI 可触发项目导出和管理构建配置
- [ ] 版本适配：运行时检测 Godot 版本，按版本启用/禁用对应 API 能力

### Out of Scope

- 游戏运行时 AI 集成（NPC 对话、智能行为等）— 本项目专注编辑器辅助
- SSE/HTTP 传输 — v1 仅支持 stdio，未来可扩展
- 编辑器视口截图给 AI — 复杂度高，收益不明确
- C#/Rust 绑定 — 仅用 C++ godot-cpp

## Context

- MCP (Model Context Protocol) 是 Anthropic 推出的 AI 工具交互标准协议，支持 tools、resources、prompts 等概念
- GDExtension 是 Godot 4.x 的原生插件系统，取代了 GDNative，允许用 C++ 等语言扩展引擎
- godot-cpp 是官方 C++ 绑定库，提供与 Godot API 的交互能力
- 项目名 "meow" 是一个有趣的代号
- 目标是发布到 GitHub，并可能上架 Godot Asset Library

## Constraints

- **Tech stack**: C++ + godot-cpp — GDExtension 官方推荐方式
- **Transport**: stdio — MCP 标准传输方式，兼容 Claude Desktop 等 AI 工具
- **Compatibility**: Godot 4.0+ — 需要处理不同版本间的 API 差异
- **Distribution**: 作为 Godot addon 分发 — 需要符合 Asset Library 结构规范

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| C++ GDExtension 而非 GDScript 插件 | 性能更好，能访问更底层 API，适合 MCP Server 长连接场景 | — Pending |
| stdio 传输 | 最简单直接，兼容主流 AI 工具，本地使用场景足够 | — Pending |
| 版本自适应而非固定版本 | 最大化用户覆盖，4.0+ 用户都能用 | — Pending |
| v1 聚焦基础读写 | 先做通 MCP 通信 + 场景基本操作，验证核心价值 | — Pending |

---
*Last updated: 2026-03-14 after initialization*
