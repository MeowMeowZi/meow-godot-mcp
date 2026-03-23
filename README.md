# Meow Godot MCP

[English](README.en.md)

**零依赖的 Godot MCP Server 插件** — 让 AI 直接控制 Godot 编辑器。

以 C++ GDExtension 形式构建，无需 Node.js/Python 等外部运行时。AI 工具（Claude、Cursor 等）通过标准 [MCP 协议](https://modelcontextprotocol.io/) 与 Godot 编辑器通信，实现场景操作、脚本管理、运行时控制等能力。

## 特性

- **50 个 MCP 工具**，覆盖完整编辑器 + 游戏运行时工作流：
  - 场景 CRUD：查询场景树、创建/修改/删除节点（支持 Ctrl+Z 撤销）
  - 场景文件管理：保存/打开/列出/创建场景，实例化 PackedScene
  - 脚本管理：读/写/编辑 GDScript，附加/分离脚本
  - 项目查询：文件列表、项目设置、资源信息
  - 运行时控制：启动/停止游戏、捕获日志输出、自动等待桥接连接
  - 信号管理：查询/连接/断开节点信号
  - UI 系统：布局预设、主题覆盖、StyleBox 创建、容器配置、UI 属性查询
  - 动画系统：创建动画、添加轨道、关键帧 CRUD、动画属性设置
  - 视口截图：捕获编辑器 2D/3D 视口截图（MCP ImageContent）
  - 游戏桥接：输入注入（键盘/鼠标/Action）、按路径点击 UI 节点、游戏视口截图
  - 运行时查询：读取游戏节点属性、执行 GDScript 表达式、获取运行时场景树
  - 集成测试：批量测试步骤执行与断言
  - TileMap 操作：批量放置/擦除瓦片、查询瓦片信息
  - 碰撞形状：一步创建 CollisionShape2D/3D 并配置形状
  - 资源属性：通过 `res://` 路径加载贴图/音频、`new:ClassName()` 内联创建资源
- **编辑器 Dock 面板**：实时连接状态、Godot 版本检测、启动/停止/重启控制、一键配置 Claude Code MCP
- **7 个 Prompt 模板**：创建玩家控制器、设置场景结构、调试物理、创建 UI 界面、构建 UI 布局、设置动画、测试游戏 UI
- **游戏桥接**：通过 EditorDebuggerPlugin 与运行中的游戏双向通信，注入输入、捕获截图
- **版本自适应**：运行时检测 Godot 版本，动态启用/禁用对应工具
- **完整 MCP 协议**：JSON-RPC 2.0、tools、resources、prompts、ImageContent（spec 2025-03-26）

## 架构

```
AI Client (Claude/Cursor)
    ↕ stdio (JSON-RPC)
Bridge 可执行文件 (~50KB)
    ↕ TCP localhost:6800
GDExtension 插件 (Godot 进程内)
    ↕ Godot API
Godot 编辑器
```

**为什么需要 Bridge？** GDExtension 是 Godot 进程内的共享库，无法被 AI 客户端直接作为子进程启动，且共享 Godot 的 stdout。Bridge 作为轻量级中继，解决了这两个问题。

## 安装

### 从 Release 安装（推荐）

1. 从 [GitHub Releases](https://github.com/MeowMeowZi/meow-godot-mcp/releases) 或 [Gitee Releases](https://gitee.com/MeowMeowZi/meow-godot-mcp/releases) 下载对应平台的 zip
2. 解压到 Godot 项目根目录（会创建 `addons/meow_godot_mcp/` 目录）
3. 打开 Godot → 项目设置 → 插件 → 启用 "Godot MCP Meow"

### 从源码编译

```bash
# 克隆仓库（含 godot-cpp 子模块）
git clone --recursive https://github.com/MeowMeowZi/meow-godot-mcp.git
cd meow-godot-mcp

# 编译 GDExtension 插件
scons platform=windows target=template_debug -j8   # Windows
scons platform=linux target=template_debug -j8     # Linux
scons platform=macos target=template_debug -j8     # macOS

# 编译 Bridge
scons bridge
```

编译产物在 `project/addons/meow_godot_mcp/bin/` 目录下。

## 配置 AI 客户端

### Claude Code（推荐）

1. 在 Godot 编辑器右下角 Dock 面板点击 **「配置 Claude Code MCP」** 按钮
2. 配置命令自动复制到剪贴板
3. 在 Claude Code 终端粘贴执行
4. 重启 Claude Code 即可连接

或手动执行：

```bash
claude mcp add --transport stdio --scope project godot -- "path/to/addons/meow_godot_mcp/bin/godot-mcp-bridge"
```

### Claude Desktop

在 `claude_desktop_config.json` 中添加：

```json
{
  "mcpServers": {
    "godot": {
      "command": "path/to/addons/meow_godot_mcp/bin/godot-mcp-bridge",
      "args": []
    }
  }
}
```

### 其他 MCP 客户端

Bridge 使用标准 stdio 传输，兼容任何支持 MCP 协议的 AI 客户端。默认连接 `localhost:6800`。

## 工具列表

### 场景操作（v1.0）

| 工具 | 说明 |
|------|------|
| `get_scene_tree` | 获取当前场景树结构 |
| `create_node` | 创建新节点（支持撤销） |
| `set_node_property` | 设置节点属性（支持 `res://` 资源加载和 `new:` 内联创建） |
| `delete_node` | 删除节点（支持撤销） |

### 脚本管理（v1.0）

| 工具 | 说明 |
|------|------|
| `read_script` | 读取 GDScript 文件 |
| `write_script` | 创建新 GDScript 文件 |
| `edit_script` | 编辑现有脚本 |
| `attach_script` | 将脚本附加到节点 |
| `detach_script` | 从节点分离脚本 |

### 项目查询（v1.0）

| 工具 | 说明 |
|------|------|
| `list_project_files` | 列出项目文件结构 |
| `get_project_settings` | 读取项目设置 |
| `get_resource_info` | 查询资源信息 |

### 运行时与信号（v1.0）

| 工具 | 说明 |
|------|------|
| `run_game` | 启动游戏（支持自动等待桥接连接） |
| `stop_game` | 停止游戏 |
| `get_game_output` | 获取游戏日志输出（支持 level/keyword/since 过滤） |
| `get_node_signals` | 查询节点信号 |
| `connect_signal` | 连接信号 |
| `disconnect_signal` | 断开信号 |

### 场景文件管理（v1.1）

| 工具 | 说明 |
|------|------|
| `save_scene` | 保存当前场景（支持另存为） |
| `open_scene` | 打开 .tscn 文件 |
| `list_open_scenes` | 列出编辑器中所有打开的场景 |
| `create_scene` | 创建新场景并打开 |
| `instantiate_scene` | 实例化 PackedScene 为子节点 |

### UI 系统（v1.1）

| 工具 | 说明 |
|------|------|
| `set_layout_preset` | 设置 Control 布局预设（full_rect、center 等） |
| `set_theme_override` | 批量设置主题覆盖（颜色、字体、字号） |
| `create_stylebox` | 创建 StyleBoxFlat 并应用到节点 |
| `get_ui_properties` | 查询 Control 的 UI 属性（anchors、size_flags 等） |
| `set_container_layout` | 配置 Container 布局参数 |
| `get_theme_overrides` | 查询节点的所有主题覆盖 |

### 动画系统（v1.1）

| 工具 | 说明 |
|------|------|
| `create_animation` | 创建 AnimationPlayer + AnimationLibrary + Animation |
| `add_animation_track` | 添加动画轨道（value/position/rotation/scale） |
| `set_keyframe` | 插入/修改/删除关键帧 |
| `get_animation_info` | 查询动画列表、轨道结构、关键帧数据 |
| `set_animation_properties` | 设置动画时长、循环模式、步长 |

### 视口截图（v1.1）

| 工具 | 说明 |
|------|------|
| `capture_viewport` | 捕获编辑器 2D/3D 视口截图（返回 MCP ImageContent） |

### 游戏桥接与输入（v1.1 + v1.2）

| 工具 | 说明 |
|------|------|
| `inject_input` | 向运行中的游戏注入输入（键盘/鼠标/Action） |
| `capture_game_viewport` | 捕获运行中游戏的视口截图 |
| `get_game_bridge_status` | 查询游戏桥接连接状态 |
| `click_node` | 按节点路径点击运行中游戏的 UI 控件 |
| `get_node_rect` | 获取 Control 节点的屏幕矩形坐标 |

### 运行时状态查询（v1.2）

| 工具 | 说明 |
|------|------|
| `get_game_node_property` | 读取运行中游戏的节点属性 |
| `eval_in_game` | 在运行中的游戏执行 GDScript 表达式 |
| `get_game_scene_tree` | 获取运行中游戏的完整场景树 |

### 集成测试（v1.2）

| 工具 | 说明 |
|------|------|
| `run_test_sequence` | 执行测试步骤序列并断言结果 |

### TileMap 操作（v1.4）

| 工具 | 说明 |
|------|------|
| `set_tilemap_cells` | 批量放置瓦片到指定网格坐标 |
| `erase_tilemap_cells` | 批量擦除指定坐标的瓦片 |
| `get_tilemap_cell_info` | 查询指定坐标的瓦片信息 |
| `get_tilemap_info` | 查询 TileMapLayer 元信息（TileSet、已用格数等） |

### 碰撞形状（v1.4）

| 工具 | 说明 |
|------|------|
| `create_collision_shape` | 一步创建 CollisionShape2D/3D 并配置形状（支持 9 种形状类型） |

### 编辑器控制（v1.4）

| 工具 | 说明 |
|------|------|
| `restart_editor` | 重启 Godot 编辑器（适用于 DLL 重编译后重载） |

## 系统要求

- **Godot 4.3+**（godot-cpp v10 最低要求）
- **Windows x86_64** / **Linux x86_64** / **macOS universal**

## 技术栈

- C++17, godot-cpp v10+
- nlohmann/json 3.12.0
- SCons 构建系统
- GoogleTest 单元测试 (82 tests) + Python UAT (23 tests)

## 许可证

[MIT](LICENSE)
