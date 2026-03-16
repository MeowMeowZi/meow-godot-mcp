# Roadmap: Godot MCP Meow

## Overview

This roadmap delivers a C++ GDExtension MCP server for Godot that lets AI tools directly manipulate the editor. We start by proving the novel bridge-relay architecture end-to-end (the highest-risk unknown), then build scene manipulation capabilities (the core value), expand to scripts and project management, add editor UI polish, and finish with runtime control, signals, and cross-platform distribution. Phase 1 validates the architecture; Phase 2 delivers the MVP value proposition; Phases 3-5 widen capability toward a complete v1 release.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Foundation & First Tool** - GDExtension scaffold, bridge executable, MCP transport, and scene tree query proving the full pipeline end-to-end
- [ ] **Phase 2: Scene CRUD** - Node creation, modification, deletion with undo/redo and Godot type parsing
- [ ] **Phase 3: Script & Project Management** - GDScript read/write/attach, project structure query, and resource management
- [ ] **Phase 4: Editor Integration** - Dock panel UI with connection status/controls, version-adaptive tools, and MCP prompt templates
- [ ] **Phase 5: Runtime, Signals & Distribution** - Game run/stop control, debug output capture, signal management, and cross-platform packaging

## Phase Details

### Phase 1: Foundation & First Tool
**Goal**: AI client can connect to a Godot editor instance via MCP and query the scene tree -- proving the bridge-relay architecture works end-to-end
**Depends on**: Nothing (first phase)
**Requirements**: MCP-01, MCP-02, MCP-03, DIST-01, SCNE-01
**Success Criteria** (what must be TRUE):
  1. The GDExtension loads in Godot 4.3+ editor without errors and the plugin appears in Project Settings
  2. The bridge executable can be configured in Claude Desktop (or similar AI client) as an MCP server and completes the initialize/initialized handshake
  3. AI client can call a `get_scene_tree` tool and receive the current scene's node hierarchy (names, types, paths)
  4. The plugin addon directory structure follows Godot Asset Library conventions (`addons/godot_mcp_meow/`)
**Plans:** 4 plans

Plans:
- [x] 01-01-PLAN.md -- Project scaffold, build system, GDExtension registration, and test infrastructure
- [x] 01-02-PLAN.md -- GDExtension MCP server: JSON-RPC protocol, TCP server, scene tree tool, EditorPlugin
- [x] 01-03-PLAN.md -- Bridge executable: stdio transport, TCP client, relay loop
- [x] 01-04-PLAN.md -- End-to-end integration build and human verification (8/8 UAT passed)

### Phase 2: Scene CRUD
**Goal**: AI can create, modify, and delete nodes in the Godot scene tree with full undo/redo support
**Depends on**: Phase 1
**Requirements**: SCNE-02, SCNE-03, SCNE-04, SCNE-05, SCNE-06
**Success Criteria** (what must be TRUE):
  1. AI can create a new node of any built-in type (e.g., Sprite2D, CharacterBody3D) under a specified parent node
  2. AI can modify node properties (transform, name, visibility, custom properties) and the changes appear immediately in the editor
  3. AI can delete a node by path and the scene tree updates accordingly
  4. All scene modifications made by AI can be undone with Ctrl+Z and redone with Ctrl+Y in the editor
  5. Property values specified as strings (e.g., "Vector2(100,200)", "#ff0000") are automatically parsed into correct Godot types
**Plans**: TBD

Plans:
- [ ] 02-01: TBD
- [ ] 02-02: TBD

### Phase 3: Script & Project Management
**Goal**: AI can read, write, and attach GDScript files, query project structure, and manage resource files
**Depends on**: Phase 2
**Requirements**: SCRP-01, SCRP-02, SCRP-03, SCRP-04, PROJ-01, PROJ-02, PROJ-03, PROJ-04, MCP-04
**Success Criteria** (what must be TRUE):
  1. AI can read the content of any GDScript file in the project and create new .gd files with specified content
  2. AI can edit existing GDScript files and attach/detach scripts to/from nodes
  3. AI can list the project file structure (directories and files under res://) and read project.godot settings
  4. AI can query and operate on .tres/.res resource files
  5. Scene tree structure and project file listing are available as MCP Resources (structured read-only data per MCP spec)
  6. IO thread + queue/promise pattern ensures cross-thread safety for concurrent MCP requests (MCP-04, deferred from Phase 1)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

### Phase 4: Editor Integration
**Goal**: The plugin provides a polished editor experience with status visibility, controls, version awareness, and workflow templates
**Depends on**: Phase 3
**Requirements**: EDIT-01, EDIT-02, EDIT-03, EDIT-04
**Success Criteria** (what must be TRUE):
  1. A dock panel in the Godot editor displays the current MCP connection status (connected, disconnected, waiting) in real-time
  2. The dock panel provides buttons to start, stop, and restart the MCP server without reloading the plugin
  3. The plugin detects the running Godot version at startup and only exposes MCP tools compatible with that version
  4. Pre-built MCP Prompt templates are available for common workflows (e.g., "create a player controller", "set up a TileMap", "debug physics")
**Plans**: TBD

Plans:
- [ ] 04-01: TBD
- [ ] 04-02: TBD

### Phase 5: Runtime, Signals & Distribution
**Goal**: AI can run/debug the game, manage signals between nodes, and the plugin ships as cross-platform binaries
**Depends on**: Phase 4
**Requirements**: RNTM-01, RNTM-02, RNTM-03, RNTM-04, RNTM-05, RNTM-06, DIST-02, DIST-03
**Success Criteria** (what must be TRUE):
  1. AI can start the game in debug mode and stop a running game instance through MCP tools
  2. AI can capture stdout/stderr log output from the running game and use it for iterative debugging
  3. AI can query signals defined on a node, create new signal connections between nodes, and disconnect existing connections
  4. Pre-compiled binaries are available for Windows (x86_64), Linux (x86_64), and macOS (universal) with a working CI/CD pipeline
  5. The plugin works on Godot 4.3, 4.4, 4.5, and 4.6 using godot-cpp compatibility layer
**Plans**: TBD

Plans:
- [ ] 05-01: TBD
- [ ] 05-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation & First Tool | 4/4 | Complete | 2026-03-16 |
| 2. Scene CRUD | 0/? | Not started | - |
| 3. Script & Project Management | 0/? | Not started | - |
| 4. Editor Integration | 0/? | Not started | - |
| 5. Runtime, Signals & Distribution | 0/? | Not started | - |
