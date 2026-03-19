# Changelog

## v1.1 — UI & Editor Expansion (2026-03-19)

20 个新 MCP 工具，从基础编辑器助手升级为完整编辑器助手。

### 新增工具

**场景文件管理 (5 tools)**
- `save_scene` — 保存当前场景（支持另存为）
- `open_scene` — 打开 .tscn 文件
- `list_open_scenes` — 列出编辑器中所有打开的场景
- `create_scene` — 创建新场景并打开
- `instantiate_scene` — 实例化 PackedScene 为子节点

**UI 系统 (6 tools)**
- `set_layout_preset` — 设置 Control 布局预设
- `set_theme_override` — 批量设置主题覆盖
- `create_stylebox` — 创建 StyleBoxFlat 并应用到节点
- `get_ui_properties` — 查询 Control 的 UI 属性
- `set_container_layout` — 配置 Container 布局参数
- `get_theme_overrides` — 查询节点的所有主题覆盖

**动画系统 (5 tools)**
- `create_animation` — 创建 AnimationPlayer + AnimationLibrary + Animation
- `add_animation_track` — 添加动画轨道（value/position/rotation/scale）
- `set_keyframe` — 插入/修改/删除关键帧
- `get_animation_info` — 查询动画列表、轨道结构、关键帧数据
- `set_animation_properties` — 设置动画时长、循环模式、步长

**视口截图 (1 tool)**
- `capture_viewport` — 捕获编辑器 2D/3D 视口截图（MCP ImageContent）

**游戏桥接 (3 tools)**
- `inject_input` — 向运行中的游戏注入输入（键盘/鼠标/Action）
- `capture_game_viewport` — 捕获运行中游戏的视口截图
- `get_game_bridge_status` — 查询游戏桥接连接状态

### 新增 Prompt 模板

- `build_ui_layout` — UI 界面构建分步工作流
- `setup_animation` — 动画设置分步工作流

### 编辑器改进

- Dock 面板全面中文化（状态/端口/工具数/启动/停止/重启）
- 新增「配置 Claude Code MCP」一键按钮（复制命令到剪贴板）
- Autoload 缺失警告提示（游戏桥接功能需要）
- MCP 客户端连接后配置提示自动消失
- `get_project_settings` 默认精简返回，支持 `category` 参数过滤

### Bug 修复

- 修复 `save_scene` / `save_scene_as` 在 Godot 4.6 GDExtension 中崩溃
- 修复 `inject_input` 缺少 type 参数时未返回错误
- 修复 MCP initialize 响应中 `resources: null` 导致 Claude Code 无法连接
- 修复 autoload 检测大小写不匹配（MeowMCPBridge vs MeowMcpBridge）

### 已知限制

- `add_autoload_singleton()` 在 Godot 4.6 中崩溃 — 游戏桥接需手动添加 Autoload
- 仅 Windows 本地构建；Linux/macOS 通过 GitHub Actions CI 自动编译

### 统计

- 38 个 MCP 工具（18 v1.0 + 20 v1.1）
- 6 个 Prompt 模板（4 v1.0 + 2 v1.1）
- 160 个单元测试
- 6,956 行 C++ 代码

---

## v1.0 — MVP (2026-03-18)

首个功能完整版本，18 个 MCP 工具覆盖编辑器核心工作流。

### 工具

- 场景 CRUD：get_scene_tree, create_node, set_node_property, delete_node
- 脚本管理：read_script, write_script, edit_script, attach_script, detach_script
- 项目查询：list_project_files, get_project_settings, get_resource_info
- 运行时：run_game, stop_game, get_game_output
- 信号管理：get_node_signals, connect_signal, disconnect_signal

### 架构

- C++ GDExtension + stdio Bridge 双进程架构
- 编辑器 Dock 面板
- 4 个 Prompt 模板
- GitHub Actions CI（三平台自动编译）
- 132 个单元测试
