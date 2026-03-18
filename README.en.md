# Meow Godot MCP

[中文](README.md)

**Zero-dependency Godot MCP Server plugin** — Let AI directly control the Godot editor.

Built as a C++ GDExtension with no Node.js/Python runtime required. AI tools (Claude, Cursor, etc.) communicate with the Godot editor via the standard [MCP protocol](https://modelcontextprotocol.io/) for scene manipulation, script management, runtime control, and more.

## Features

- **18 MCP tools** covering the full editor workflow:
  - Scene CRUD: query scene tree, create/modify/delete nodes (Ctrl+Z undo support)
  - Script management: read/write/edit GDScript, attach/detach scripts
  - Project queries: file listing, project settings, resource info
  - Runtime control: run/stop game, capture log output
  - Signal management: query/connect/disconnect node signals
- **Editor Dock panel**: live connection status, Godot version detection, Start/Stop/Restart controls
- **4 Prompt templates**: create player controller, setup scene structure, debug physics, create UI interface
- **Version-adaptive**: detects Godot version at runtime, dynamically enables/disables tools
- **Full MCP compliance**: JSON-RPC 2.0, tools, resources, prompts (spec 2025-03-26)

## Architecture

```
AI Client (Claude/Cursor)
    ↕ stdio (JSON-RPC)
Bridge executable (~50KB)
    ↕ TCP localhost:6800
GDExtension plugin (inside Godot process)
    ↕ Godot API
Godot Editor
```

**Why a Bridge?** GDExtension is a shared library inside the Godot process — it cannot be spawned as a subprocess by AI clients, and shares Godot's stdout. The bridge acts as a lightweight relay to solve both issues.

## Installation

### From Release (Recommended)

1. Download the zip for your platform from [Releases](https://gitee.com/MeowMeowZi/meow-godot-mcp/releases)
2. Extract to your Godot project root (creates `addons/meow_godot_mcp/`)
3. Open Godot → Project Settings → Plugins → Enable "Godot MCP Meow"

### Build from Source

```bash
# Clone with godot-cpp submodule
git clone --recursive https://gitee.com/MeowMeowZi/meow-godot-mcp.git
cd meow-godot-mcp

# Build GDExtension plugin
scons platform=windows target=template_debug -j8   # Windows
scons platform=linux target=template_debug -j8     # Linux
scons platform=macos target=template_debug -j8     # macOS

# Build Bridge
scons bridge
```

Build output is in `project/addons/meow_godot_mcp/bin/`.

## Configure AI Client

### Claude Desktop

Add to `claude_desktop_config.json`:

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

### Other MCP Clients

The bridge uses standard stdio transport, compatible with any MCP-compliant AI client. Default connection: `localhost:6800`.

## Tool List

| Tool | Description |
|------|-------------|
| `get_scene_tree` | Get current scene tree structure |
| `create_node` | Create a new node |
| `set_node_property` | Set node property |
| `delete_node` | Delete a node |
| `read_script` | Read GDScript file |
| `write_script` | Create new GDScript file |
| `edit_script` | Edit existing script |
| `attach_script` | Attach script to node |
| `detach_script` | Detach script from node |
| `list_project_files` | List project file structure |
| `get_project_settings` | Read project settings |
| `get_resource_info` | Query resource info |
| `run_game` | Start game |
| `stop_game` | Stop game |
| `get_game_output` | Get game log output |
| `get_node_signals` | Query node signals |
| `connect_signal` | Connect signal |
| `disconnect_signal` | Disconnect signal |

## Requirements

- **Godot 4.3+** (godot-cpp v10 minimum)
- **Windows x86_64** / **Linux x86_64** / **macOS universal**

## Tech Stack

- C++17, godot-cpp v10+
- nlohmann/json 3.12.0
- SCons build system
- GoogleTest unit tests (132 tests)

## License

[MIT](LICENSE)
