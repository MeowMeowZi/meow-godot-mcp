# Meow Godot MCP

[English](README.en.md)

**零依赖的 Godot MCP Server 插件** — 让 AI 直接控制 Godot 编辑器。

以 C++ GDExtension 形式构建，无需 Node.js/Python 等外部运行时。AI 工具（Claude、Cursor 等）通过标准 [MCP 协议](https://modelcontextprotocol.io/) 与 Godot 编辑器通信，实现场景操作、脚本管理、运行时控制等能力。

## 特性

- **18 个 MCP 工具**，覆盖完整编辑器工作流：
  - 场景 CRUD：查询场景树、创建/修改/删除节点（支持 Ctrl+Z 撤销）
  - 脚本管理：读/写/编辑 GDScript，附加/分离脚本
  - 项目查询：文件列表、项目设置、资源信息
  - 运行时控制：启动/停止游戏、捕获日志输出
  - 信号管理：查询/连接/断开节点信号
- **编辑器 Dock 面板**：实时连接状态、Godot 版本检测、Start/Stop/Restart 控制
- **4 个 Prompt 模板**：创建玩家控制器、设置场景结构、调试物理、创建 UI 界面
- **版本自适应**：运行时检测 Godot 版本，动态启用/禁用对应工具
- **完整 MCP 协议**：JSON-RPC 2.0、tools、resources、prompts（spec 2025-03-26）

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

1. 从 [Releases](https://gitee.com/MeowMeowZi/meow-godot-mcp/releases) 下载对应平台的 zip
2. 解压到 Godot 项目根目录（会创建 `addons/meow_godot_mcp/` 目录）
3. 打开 Godot → 项目设置 → 插件 → 启用 "Godot MCP Meow"

### 从源码编译

```bash
# 克隆仓库（含 godot-cpp 子模块）
git clone --recursive https://gitee.com/MeowMeowZi/meow-godot-mcp.git
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

| 工具 | 说明 |
|------|------|
| `get_scene_tree` | 获取当前场景树结构 |
| `create_node` | 创建新节点 |
| `set_node_property` | 设置节点属性 |
| `delete_node` | 删除节点 |
| `read_script` | 读取 GDScript 文件 |
| `write_script` | 创建新 GDScript 文件 |
| `edit_script` | 编辑现有脚本 |
| `attach_script` | 将脚本附加到节点 |
| `detach_script` | 从节点分离脚本 |
| `list_project_files` | 列出项目文件结构 |
| `get_project_settings` | 读取项目设置 |
| `get_resource_info` | 查询资源信息 |
| `run_game` | 启动游戏 |
| `stop_game` | 停止游戏 |
| `get_game_output` | 获取游戏日志输出 |
| `get_node_signals` | 查询节点信号 |
| `connect_signal` | 连接信号 |
| `disconnect_signal` | 断开信号 |

## 系统要求

- **Godot 4.3+**（godot-cpp v10 最低要求）
- **Windows x86_64** / **Linux x86_64** / **macOS universal**

## 技术栈

- C++17, godot-cpp v10+
- nlohmann/json 3.12.0
- SCons 构建系统
- GoogleTest 单元测试 (132 tests)

## 许可证

[MIT](LICENSE)
