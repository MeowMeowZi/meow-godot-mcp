# Requirements: Meow Godot MCP

**Defined:** 2026-03-18
**Core Value:** AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发

## v1.1 Requirements

Requirements for v1.1 release. Each maps to roadmap phases.

### Scene File Management

- [x] **SCNF-01**: AI 可保存当前场景到磁盘
- [x] **SCNF-02**: AI 可打开指定路径的场景文件
- [x] **SCNF-03**: AI 可查询当前编辑器中已打开的所有场景列表
- [x] **SCNF-04**: AI 可创建新的空场景并指定根节点类型
- [x] **SCNF-05**: AI 可将场景树打包为 .tscn/.scn 文件（PackedScene）
- [x] **SCNF-06**: AI 可在场景中实例化已有的 PackedScene（场景继承/复用）

### UI System

- [x] **UISYS-01**: AI 可设置 Control 节点的 anchor/margin 布局预设
- [x] **UISYS-02**: AI 可设置 Control 节点的 theme override（颜色、字体、样式）
- [x] **UISYS-03**: AI 可创建和编辑 StyleBox 资源（StyleBoxFlat 的圆角、边框、背景色等）
- [x] **UISYS-04**: AI 可查询 Control 节点的 UI 特有属性（size_flags、anchor、minimum_size 等）
- [x] **UISYS-05**: AI 可配置 Container 节点的布局参数（separation、alignment 等）
- [x] **UISYS-06**: AI 可设置 Control 节点的焦点邻居（focus_neighbor）实现键盘/手柄导航链

### Animation System

- [x] **ANIM-01**: AI 可创建 AnimationPlayer 并添加 AnimationLibrary 和 Animation 资源
- [x] **ANIM-02**: AI 可在 Animation 中添加/删除轨道（Value、Position3D、Rotation3D 等类型）
- [x] **ANIM-03**: AI 可在轨道上插入/删除/修改关键帧
- [x] **ANIM-04**: AI 可查询 AnimationPlayer 的动画列表、轨道结构和关键帧数据
- [x] **ANIM-05**: AI 可设置 Animation 的时长、循环模式等属性

### Viewport Screenshots

- [x] **VWPT-01**: AI 可截取编辑器 2D 视口的当前画面
- [x] **VWPT-02**: AI 可截取编辑器 3D 视口的当前画面
- [x] **VWPT-03**: 截图以 base64 PNG 格式通过 MCP ImageContent 返回给 AI 客户端

### Running Game Bridge

- [x] **BRDG-01**: 插件自动注入 companion autoload 脚本到游戏进程，建立编辑器-游戏通信通道
- [x] **BRDG-02**: AI 可向运行中的游戏注入键盘按键事件（按下/释放）
- [x] **BRDG-03**: AI 可向运行中的游戏注入鼠标事件（移动、点击、滚轮）
- [x] **BRDG-04**: AI 可向运行中的游戏注入 Input Action 事件
- [x] **BRDG-05**: AI 可截取运行中游戏的视口画面

### Prompt Templates

- [x] **PMPT-01**: 提供 UI 界面构建工作流模板（如"创建主菜单"、"创建 HUD"）
- [x] **PMPT-02**: 提供动画设置工作流模板（如"创建角色行走动画"、"创建 UI 过渡动画"）

## v2 Requirements

Deferred to future release.

### Advanced Editor

- **ADVE-01**: AI 可编辑 TileMap/TileSet 数据
- **ADVE-02**: AI 可操作 Shader 和材质参数
- **ADVE-03**: AI 可管理 AudioStream 和音频总线

### Advanced MCP

- **ADVM-01**: 支持批量操作（多个工具调用原子执行）
- **ADVM-02**: 支持 SSE/HTTP 传输方式（远程连接和多客户端）

## Out of Scope

| Feature | Reason |
|---------|--------|
| 游戏运行时 AI 集成（NPC/行为树） | 不同产品类别，与编辑器辅助开发无关 |
| GDScript LSP 替代 | Godot 内置 LSP 已足够 |
| C#/Rust 绑定 | 仅用 C++ godot-cpp |
| Godot 3.x 支持 | GDExtension 仅限 4.x |
| 多编辑器协作 | 复杂度极高 |
| TileMap/Shader/Audio 编辑 | 延迟到 v2 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SCNF-01 | Phase 6 | Complete |
| SCNF-02 | Phase 6 | Complete |
| SCNF-03 | Phase 6 | Complete |
| SCNF-04 | Phase 6 | Complete |
| SCNF-05 | Phase 6 | Complete |
| SCNF-06 | Phase 6 | Complete |
| UISYS-01 | Phase 7 | Complete |
| UISYS-02 | Phase 7 | Complete |
| UISYS-03 | Phase 7 | Complete |
| UISYS-04 | Phase 7 | Complete |
| UISYS-05 | Phase 7 | Complete |
| UISYS-06 | Phase 7 | Complete |
| ANIM-01 | Phase 8 | Complete |
| ANIM-02 | Phase 8 | Complete |
| ANIM-03 | Phase 8 | Complete |
| ANIM-04 | Phase 8 | Complete |
| ANIM-05 | Phase 8 | Complete |
| VWPT-01 | Phase 9 | Complete |
| VWPT-02 | Phase 9 | Complete |
| VWPT-03 | Phase 9 | Complete |
| BRDG-01 | Phase 10 | Complete |
| BRDG-02 | Phase 10 | Complete |
| BRDG-03 | Phase 10 | Complete |
| BRDG-04 | Phase 10 | Complete |
| BRDG-05 | Phase 10 | Complete |
| PMPT-01 | Phase 11 | Complete |
| PMPT-02 | Phase 11 | Complete |

**Coverage:**
- v1.1 requirements: 27 total
- Mapped to phases: 27
- Unmapped: 0

---
*Requirements defined: 2026-03-18*
*Traceability updated: 2026-03-18*
