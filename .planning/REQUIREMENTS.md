# Requirements: Meow Godot MCP

**Defined:** 2026-03-18
**Core Value:** AI 能通过标准 MCP 协议读取和操控 Godot 编辑器中的场景树与节点，实现真正的 AI 辅助游戏开发

## v1.1 Requirements

Requirements for v1.1 release. Each maps to roadmap phases.

### Scene File Management

- [ ] **SCNF-01**: AI 可保存当前场景到磁盘
- [ ] **SCNF-02**: AI 可打开指定路径的场景文件
- [ ] **SCNF-03**: AI 可查询当前编辑器中已打开的所有场景列表
- [ ] **SCNF-04**: AI 可创建新的空场景并指定根节点类型
- [ ] **SCNF-05**: AI 可将场景树打包为 .tscn/.scn 文件（PackedScene）
- [ ] **SCNF-06**: AI 可在场景中实例化已有的 PackedScene（场景继承/复用）

### UI System

- [ ] **UISYS-01**: AI 可设置 Control 节点的 anchor/margin 布局预设
- [ ] **UISYS-02**: AI 可设置 Control 节点的 theme override（颜色、字体、样式）
- [ ] **UISYS-03**: AI 可创建和编辑 StyleBox 资源（StyleBoxFlat 的圆角、边框、背景色等）
- [ ] **UISYS-04**: AI 可查询 Control 节点的 UI 特有属性（size_flags、anchor、minimum_size 等）
- [ ] **UISYS-05**: AI 可配置 Container 节点的布局参数（separation、alignment 等）
- [ ] **UISYS-06**: AI 可设置 Control 节点的焦点邻居（focus_neighbor）实现键盘/手柄导航链

### Animation System

- [ ] **ANIM-01**: AI 可创建 AnimationPlayer 并添加 AnimationLibrary 和 Animation 资源
- [ ] **ANIM-02**: AI 可在 Animation 中添加/删除轨道（Value、Position3D、Rotation3D 等类型）
- [ ] **ANIM-03**: AI 可在轨道上插入/删除/修改关键帧
- [ ] **ANIM-04**: AI 可查询 AnimationPlayer 的动画列表、轨道结构和关键帧数据
- [ ] **ANIM-05**: AI 可设置 Animation 的时长、循环模式等属性

### Viewport Screenshots

- [ ] **VWPT-01**: AI 可截取编辑器 2D 视口的当前画面
- [ ] **VWPT-02**: AI 可截取编辑器 3D 视口的当前画面
- [ ] **VWPT-03**: 截图以 base64 PNG 格式通过 MCP ImageContent 返回给 AI 客户端

### Running Game Bridge

- [ ] **BRDG-01**: 插件自动注入 companion autoload 脚本到游戏进程，建立编辑器-游戏通信通道
- [ ] **BRDG-02**: AI 可向运行中的游戏注入键盘按键事件（按下/释放）
- [ ] **BRDG-03**: AI 可向运行中的游戏注入鼠标事件（移动、点击、滚轮）
- [ ] **BRDG-04**: AI 可向运行中的游戏注入 Input Action 事件
- [ ] **BRDG-05**: AI 可截取运行中游戏的视口画面

### Prompt Templates

- [ ] **PMPT-01**: 提供 UI 界面构建工作流模板（如"创建主菜单"、"创建 HUD"）
- [ ] **PMPT-02**: 提供动画设置工作流模板（如"创建角色行走动画"、"创建 UI 过渡动画"）

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
| SCNF-01 | Phase 6 | Pending |
| SCNF-02 | Phase 6 | Pending |
| SCNF-03 | Phase 6 | Pending |
| SCNF-04 | Phase 6 | Pending |
| SCNF-05 | Phase 6 | Pending |
| SCNF-06 | Phase 6 | Pending |
| UISYS-01 | Phase 7 | Pending |
| UISYS-02 | Phase 7 | Pending |
| UISYS-03 | Phase 7 | Pending |
| UISYS-04 | Phase 7 | Pending |
| UISYS-05 | Phase 7 | Pending |
| UISYS-06 | Phase 7 | Pending |
| ANIM-01 | Phase 8 | Pending |
| ANIM-02 | Phase 8 | Pending |
| ANIM-03 | Phase 8 | Pending |
| ANIM-04 | Phase 8 | Pending |
| ANIM-05 | Phase 8 | Pending |
| VWPT-01 | Phase 9 | Pending |
| VWPT-02 | Phase 9 | Pending |
| VWPT-03 | Phase 9 | Pending |
| BRDG-01 | Phase 10 | Pending |
| BRDG-02 | Phase 10 | Pending |
| BRDG-03 | Phase 10 | Pending |
| BRDG-04 | Phase 10 | Pending |
| BRDG-05 | Phase 10 | Pending |
| PMPT-01 | Phase 11 | Pending |
| PMPT-02 | Phase 11 | Pending |

**Coverage:**
- v1.1 requirements: 27 total
- Mapped to phases: 27
- Unmapped: 0

---
*Requirements defined: 2026-03-18*
*Traceability updated: 2026-03-18*
