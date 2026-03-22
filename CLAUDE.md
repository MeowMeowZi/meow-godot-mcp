# Godot MCP Meow - Project Instructions

## MCP Tools

This project has a Godot MCP server connected. When building or modifying Godot scenes, **prefer Godot MCP tools over writing .tscn files directly**:

- `mcp__godot__create_scene` — Create new scenes with root node
- `mcp__godot__create_node` — Add nodes to scene tree
- `mcp__godot__set_node_property` — Set node properties (position, size, color, etc.)
- `mcp__godot__write_script` / `mcp__godot__edit_script` — Create/edit GDScript files
- `mcp__godot__attach_script` — Attach scripts to nodes
- `mcp__godot__save_scene` — Save scene to disk
- `mcp__godot__set_layout_preset` — Set Control node layout (full_rect, center, etc.)
- `mcp__godot__set_theme_override` — Set theme overrides (font_color, font_size, etc.)
- `mcp__godot__create_stylebox` — Create StyleBoxFlat for panel styling
- `mcp__godot__set_container_layout` — Configure container alignment/separation
- `mcp__godot__connect_signal` — Connect signals between nodes
- `mcp__godot__run_game` / `mcp__godot__stop_game` — Run/stop game for testing
- `mcp__godot__capture_viewport` — Screenshot editor viewport
- `mcp__godot__capture_game_viewport` — Screenshot running game

Benefits: MCP tools interact with the live editor, changes are immediately visible, and scene tree is properly constructed through the editor API.

Fall back to direct file writing (Write tool for .gd, .tscn) only when MCP tools are unavailable or for bulk operations where MCP would be too slow.

## Code Style

- C++17, godot-cpp v10+
- GDScript follows Godot conventions (snake_case, signal-based)
- nlohmann/json for JSON handling
