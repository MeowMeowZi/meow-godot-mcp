## Meow Godot MCP

零依赖的 Godot MCP Server 插件 — 让 AI 直接控制 Godot 编辑器。

### 安装

1. 下载 zip 解压到 Godot 项目根目录（创建 `addons/meow_godot_mcp/`）
2. 打开 Godot → 项目设置 → 插件 → 启用 "Godot MCP Meow"
3. Dock 面板点击"配置 Claude Code MCP"按钮，粘贴命令到 Claude Code 终端

### 包含内容

- Windows x86_64、Linux x86_64、macOS universal 全平台预编译二进制
- Bridge 可执行文件（stdio ↔ TCP 中继）
- 游戏桥接 companion 脚本（输入注入/截图需手动添加 Autoload）

### 系统要求

- Godot 4.3+
- 支持平台：Windows x86_64 / Linux x86_64 / macOS universal

详细功能列表见 [README](https://github.com/MeowMeowZi/meow-godot-mcp/blob/master/README.md)。
