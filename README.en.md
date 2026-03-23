# Meow Godot MCP

[中文](README.md)

**Zero-dependency Godot MCP Server plugin** — Let AI directly control the Godot editor.

Built as a C++ GDExtension with no Node.js/Python runtime required. AI tools (Claude, Cursor, etc.) communicate with the Godot editor via the standard [MCP protocol](https://modelcontextprotocol.io/) for scene manipulation, script management, runtime control, and more.

## Features

- **50 MCP tools** covering the full editor + game runtime workflow:
  - Scene CRUD: query scene tree, create/modify/delete nodes (Ctrl+Z undo support)
  - Scene file management: save/open/list/create scenes, instantiate PackedScene
  - Script management: read/write/edit GDScript, attach/detach scripts
  - Project queries: file listing, project settings, resource info
  - Runtime control: run/stop game, capture log output, auto-wait for bridge connection
  - Signal management: query/connect/disconnect node signals
  - UI system: layout presets, theme overrides, StyleBox creation, container config, UI property queries
  - Animation system: create animations, add tracks, keyframe CRUD, animation properties
  - Viewport screenshots: capture editor 2D/3D viewport screenshots (MCP ImageContent)
  - Game bridge: input injection (keyboard/mouse/Action), click UI nodes by path, game viewport capture
  - Runtime queries: read game node properties, execute GDScript expressions, get runtime scene tree
  - Integration testing: batch test step execution with assertions
  - TileMap operations: batch place/erase tiles, query tile info
  - Collision shapes: one-step CollisionShape2D/3D creation with configured shapes
  - Resource properties: load textures/audio via `res://` paths, `new:ClassName()` inline resource creation
- **Editor Dock panel**: live connection status, Godot version detection, Start/Stop/Restart controls, one-click Claude Code MCP setup
- **7 Prompt templates**: create player controller, setup scene structure, debug physics, create UI, build UI layout, setup animation, test game UI
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

1. Download the zip for your platform from [GitHub Releases](https://github.com/MeowMeowZi/meow-godot-mcp/releases) or [Gitee Releases](https://gitee.com/MeowMeowZi/meow-godot-mcp/releases)
2. Extract to your Godot project root (creates `addons/meow_godot_mcp/`)
3. Open Godot → Project Settings → Plugins → Enable "Godot MCP Meow"

### Build from Source

```bash
# Clone with godot-cpp submodule
git clone --recursive https://github.com/MeowMeowZi/meow-godot-mcp.git
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

### Claude Code (Recommended)

1. Click **"Configure Claude Code MCP"** button in the Godot editor dock panel (bottom-right)
2. The setup command is automatically copied to your clipboard
3. Paste and run it in the Claude Code terminal
4. Restart Claude Code to connect

Or manually run:

```bash
claude mcp add --transport stdio --scope project godot -- "path/to/addons/meow_godot_mcp/bin/godot-mcp-bridge"
```

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
| `set_node_property` | Set node property (supports `res://` resource loading and `new:` inline creation) |
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
| `run_game` | Start game (supports auto-wait for bridge connection) |
| `stop_game` | Stop game |
| `get_game_output` | Get game log output (supports level/keyword/since filtering) |
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

### Game Bridge & Input (v1.1 + v1.2)

| Tool | Description |
|------|-------------|
| `inject_input` | Inject input into running game (keyboard/mouse/Action) |
| `capture_game_viewport` | Capture running game viewport screenshot |
| `get_game_bridge_status` | Query game bridge connection status |
| `click_node` | Click a UI Control node in running game by path |
| `get_node_rect` | Get the screen rectangle of a Control node |

### Runtime State Queries (v1.2)

| Tool | Description |
|------|-------------|
| `get_game_node_property` | Read a node property from the running game |
| `eval_in_game` | Execute a GDScript expression in the running game |
| `get_game_scene_tree` | Get the complete scene tree from the running game |

### Integration Testing (v1.2)

| Tool | Description |
|------|-------------|
| `run_test_sequence` | Execute test steps with assertions against the running game |

### TileMap Operations (v1.4)

| Tool | Description |
|------|-------------|
| `set_tilemap_cells` | Batch-place tiles at grid coordinates |
| `erase_tilemap_cells` | Batch-erase tiles at grid coordinates |
| `get_tilemap_cell_info` | Query tile data at specified coordinates |
| `get_tilemap_info` | Query TileMapLayer metadata (TileSet, used cells, bounds) |

### Collision Shapes (v1.4)

| Tool | Description |
|------|-------------|
| `create_collision_shape` | One-step CollisionShape2D/3D creation with configured shape (9 shape types) |

### Editor Control (v1.4)

| Tool | Description |
|------|-------------|
| `restart_editor` | Restart Godot editor (useful after DLL recompilation) |

## Requirements

- **Godot 4.3+** (godot-cpp v10 minimum)
- **Windows x86_64** / **Linux x86_64** / **macOS universal**

## Tech Stack

- C++17, godot-cpp v10+
- nlohmann/json 3.12.0
- SCons build system
- GoogleTest unit tests (82 tests) + Python UAT (23 tests)

## License

[MIT](LICENSE)
