# Meow Godot MCP

[中文](README.md)

**Zero-dependency Godot MCP Server plugin** — Let AI directly control the Godot editor.

Built as a C++ GDExtension with no Node.js/Python runtime required. AI tools (Claude, Cursor, etc.) communicate with the Godot editor via the standard [MCP protocol](https://modelcontextprotocol.io/) for scene manipulation, script management, runtime control, and more.

## Features

- **38 MCP tools** covering the full editor + game runtime workflow:
  - Scene CRUD: query scene tree, create/modify/delete nodes (Ctrl+Z undo support)
  - Scene file management: save/open/list/create scenes, instantiate PackedScene
  - Script management: read/write/edit GDScript, attach/detach scripts
  - Project queries: file listing, project settings, resource info
  - Runtime control: run/stop game, capture log output
  - Signal management: query/connect/disconnect node signals
  - UI system: layout presets, theme overrides, StyleBox creation, container config, UI property queries
  - Animation system: create animations, add tracks, keyframe CRUD, animation properties
  - Viewport screenshots: capture editor 2D/3D viewport screenshots (MCP ImageContent)
  - Game bridge: input injection (keyboard/mouse/Action), game viewport capture, bridge status
- **Editor Dock panel**: live connection status, Godot version detection, Start/Stop/Restart controls
- **6 Prompt templates**: create player controller, setup scene structure, debug physics, create UI, build UI layout, setup animation
- **Game bridge**: bidirectional communication with running game via EditorDebuggerPlugin for input injection and screenshot capture
- **Version-adaptive**: detects Godot version at runtime, dynamically enables/disables tools
- **Full MCP compliance**: JSON-RPC 2.0, tools, resources, prompts, ImageContent (spec 2025-03-26)

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

### Scene Operations (v1.0)

| Tool | Description |
|------|-------------|
| `get_scene_tree` | Get current scene tree structure |
| `create_node` | Create a new node (undo support) |
| `set_node_property` | Set node property (undo support) |
| `delete_node` | Delete a node (undo support) |

### Script Management (v1.0)

| Tool | Description |
|------|-------------|
| `read_script` | Read GDScript file |
| `write_script` | Create new GDScript file |
| `edit_script` | Edit existing script |
| `attach_script` | Attach script to node |
| `detach_script` | Detach script from node |

### Project Queries (v1.0)

| Tool | Description |
|------|-------------|
| `list_project_files` | List project file structure |
| `get_project_settings` | Read project settings |
| `get_resource_info` | Query resource info |

### Runtime & Signals (v1.0)

| Tool | Description |
|------|-------------|
| `run_game` | Start game (main/current/custom scene) |
| `stop_game` | Stop game |
| `get_game_output` | Get game log output |
| `get_node_signals` | Query node signals |
| `connect_signal` | Connect signal |
| `disconnect_signal` | Disconnect signal |

### Scene File Management (v1.1)

| Tool | Description |
|------|-------------|
| `save_scene` | Save current scene (supports save-as) |
| `open_scene` | Open a .tscn file |
| `list_open_scenes` | List all open scenes in the editor |
| `create_scene` | Create a new scene and open it |
| `instantiate_scene` | Instantiate a PackedScene as child node |

### UI System (v1.1)

| Tool | Description |
|------|-------------|
| `set_layout_preset` | Set Control layout preset (full_rect, center, etc.) |
| `set_theme_override` | Batch set theme overrides (colors, fonts, sizes) |
| `create_stylebox` | Create StyleBoxFlat and apply to node |
| `get_ui_properties` | Query Control UI properties (anchors, size_flags, etc.) |
| `set_container_layout` | Configure Container layout parameters |
| `get_theme_overrides` | Query all theme overrides on a node |

### Animation System (v1.1)

| Tool | Description |
|------|-------------|
| `create_animation` | Create AnimationPlayer + AnimationLibrary + Animation |
| `add_animation_track` | Add animation track (value/position/rotation/scale) |
| `set_keyframe` | Insert/update/remove keyframes |
| `get_animation_info` | Query animation list, track structure, keyframe data |
| `set_animation_properties` | Set animation duration, loop mode, step |

### Viewport Screenshots (v1.1)

| Tool | Description |
|------|-------------|
| `capture_viewport` | Capture editor 2D/3D viewport screenshot (MCP ImageContent) |

### Game Bridge (v1.1)

| Tool | Description |
|------|-------------|
| `inject_input` | Inject input into running game (keyboard/mouse/Action) |
| `capture_game_viewport` | Capture running game viewport screenshot |
| `get_game_bridge_status` | Query game bridge connection status |

## Requirements

- **Godot 4.3+** (godot-cpp v10 minimum)
- **Windows x86_64** / **Linux x86_64** / **macOS universal**

## Tech Stack

- C++17, godot-cpp v10+
- nlohmann/json 3.12.0
- SCons build system
- GoogleTest unit tests (160 tests)

## License

[MIT](LICENSE)
