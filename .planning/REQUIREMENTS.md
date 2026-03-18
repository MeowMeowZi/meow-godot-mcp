# Requirements: Godot MCP Meow

**Defined:** 2026-03-14
**Core Value:** AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### MCP Transport

- [x] **MCP-01**: Bridge 可执行文件通过 stdio 接收 AI 客户端请求并中继到 GDExtension（TCP localhost）
- [x] **MCP-02**: GDExtension 内实现 JSON-RPC 2.0 协议处理 MCP 消息
- [x] **MCP-03**: 支持 MCP initialize/initialized 握手流程（spec 2025-03-26）
- [x] **MCP-04**: IO 线程与 Godot 主线程通过队列+promise 模式安全通信

### Scene Operations

- [x] **SCNE-01**: AI 可查询当前场景树结构（节点名称、类型、路径、层级关系）
- [x] **SCNE-02**: AI 可创建指定类型的新节点并设置父节点和初始属性
- [x] **SCNE-03**: AI 可修改节点属性（transform、name、visibility、自定义属性等）
- [x] **SCNE-04**: AI 可删除指定路径的节点
- [x] **SCNE-05**: 所有场景修改操作集成 Godot UndoRedo 系统，支持 Ctrl+Z 撤销
- [x] **SCNE-06**: 属性值自动解析 Godot 类型（Vector2/3、Color、NodePath 等字符串格式自动转换）

### Script Management

- [x] **SCRP-01**: AI 可读取项目中 GDScript 文件内容
- [x] **SCRP-02**: AI 可创建新 GDScript 文件并写入内容
- [x] **SCRP-03**: AI 可编辑现有 GDScript 文件内容
- [x] **SCRP-04**: AI 可将脚本附加到节点或从节点分离脚本

### Runtime & Debug

- [x] **RNTM-01**: AI 可启动当前项目在调试模式下运行
- [x] **RNTM-02**: AI 可停止正在运行的项目实例
- [x] **RNTM-03**: AI 可捕获游戏运行时的 stdout/stderr 日志输出
- [x] **RNTM-04**: AI 可查询节点上已定义和已连接的信号列表
- [x] **RNTM-05**: AI 可创建节点间的信号连接（指定信号名、目标节点、目标方法）
- [x] **RNTM-06**: AI 可断开节点间已有的信号连接

### Project & Resources

- [x] **PROJ-01**: AI 可查询项目文件结构和目录列表（res:// 文件系统）
- [x] **PROJ-02**: AI 可读取 project.godot 项目设置
- [x] **PROJ-03**: AI 可查询和操作 .tres/.res 资源文件
- [x] **PROJ-04**: 场景树、项目结构等数据通过 MCP Resources 规范暴露为结构化只读数据

### Editor Integration

- [x] **EDIT-01**: 编辑器底部 Dock 面板显示 MCP 服务连接状态（已连接/断开/等待中）
- [x] **EDIT-02**: 编辑器面板提供 MCP 服务启动/停止/重启控制按钮
- [x] **EDIT-03**: 运行时检测 Godot 版本并按版本动态启用/禁用对应 MCP 工具
- [x] **EDIT-04**: 提供预建 MCP Prompts 模板（创建玩家控制器、设置 TileMap、调试物理等常见工作流）

### Distribution & Compatibility

- [x] **DIST-01**: 以 GDExtension addon 形式打包（addons/godot-mcp-meow/ 目录结构）
- [x] **DIST-02**: 提供 Windows（x86_64）、Linux（x86_64）、macOS（universal）预编译二进制
- [x] **DIST-03**: 支持 Godot 4.3+ 并通过 godot-cpp 兼容层向前兼容 4.4/4.5/4.6

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Advanced Runtime

- **ADVR-01**: AI 可注入键盘/鼠标/Action 输入到运行中的游戏进行自动化测试
- **ADVR-02**: AI 可捕获游戏视口截图用于视觉反馈

### Advanced MCP

- **ADVM-01**: 支持批量操作（多个工具调用原子执行）
- **ADVM-02**: 支持 SSE/HTTP 传输方式（远程连接和多客户端）

### Specialized Tools

- **SPEC-01**: AnimationTree 专用编辑工具
- **SPEC-02**: TileMap/TileSet 专用编辑工具
- **SPEC-03**: Shader/材质系统编辑工具

## Out of Scope

| Feature | Reason |
|---------|--------|
| 游戏运行时 AI 集成（NPC/行为树） | 不同产品类别，与编辑器辅助开发无关 |
| 资产生成（纹理/模型/音频） | 应由专门工具（如 Stable Diffusion）的独立 MCP Server 处理 |
| GDScript LSP 替代 | Godot 内置 LSP 已足够，重复实现没有价值 |
| C#/Rust 语言绑定 | 仅用 C++ godot-cpp，避免分散精力 |
| Godot 3.x 支持 | GDExtension 仅限 Godot 4.x，3.x 需要完全不同的 GDNative 系统 |
| 多编辑器协作 | 复杂度极高，非 MCP 设计用途 |
| 自定义 LLM/模型托管 | MCP 是传输层协议，不关心哪个 AI 模型连接 |
| 插件市场/付费基础设施 | 开源社区项目，MIT 许可证 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| MCP-01 | Phase 1 | Complete |
| MCP-02 | Phase 1 | Complete |
| MCP-03 | Phase 1 | Complete |
| MCP-04 | Phase 1 | Complete |
| SCNE-01 | Phase 1 | Complete |
| SCNE-02 | Phase 2 | Complete |
| SCNE-03 | Phase 2 | Complete |
| SCNE-04 | Phase 2 | Complete |
| SCNE-05 | Phase 2 | Complete |
| SCNE-06 | Phase 2 | Complete |
| SCRP-01 | Phase 3 | Complete |
| SCRP-02 | Phase 3 | Complete |
| SCRP-03 | Phase 3 | Complete |
| SCRP-04 | Phase 3 | Complete |
| PROJ-01 | Phase 3 | Complete |
| PROJ-02 | Phase 3 | Complete |
| PROJ-03 | Phase 3 | Complete |
| PROJ-04 | Phase 3 | Complete |
| EDIT-01 | Phase 4 | Complete |
| EDIT-02 | Phase 4 | Complete |
| EDIT-03 | Phase 4 | Complete |
| EDIT-04 | Phase 4 | Complete |
| RNTM-01 | Phase 5 | Complete |
| RNTM-02 | Phase 5 | Complete |
| RNTM-03 | Phase 5 | Complete |
| RNTM-04 | Phase 5 | Complete |
| RNTM-05 | Phase 5 | Complete |
| RNTM-06 | Phase 5 | Complete |
| DIST-01 | Phase 1 | Complete |
| DIST-02 | Phase 5 | Complete |
| DIST-03 | Phase 5 | Complete |

**Coverage:**
- v1 requirements: 31 total
- Mapped to phases: 31
- Unmapped: 0

---
*Requirements defined: 2026-03-14*
*Last updated: 2026-03-16 after Phase 2 completion (all SCNE requirements UAT-verified)*
