# Roadmap: Godot MCP Meow

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-18)
- **v1.1 UI & Editor Expansion** -- Phases 6-11 (in progress)

## Phases

<details>
<summary>v1.0 MVP (Phases 1-5) -- SHIPPED 2026-03-18</summary>

- [x] Phase 1: Foundation & First Tool (4/4 plans) -- completed 2026-03-16
- [x] Phase 2: Scene CRUD (3/3 plans) -- completed 2026-03-16
- [x] Phase 3: Script & Project Management (3/3 plans) -- completed 2026-03-17
- [x] Phase 4: Editor Integration (2/2 plans) -- completed 2026-03-18
- [x] Phase 5: Runtime, Signals & Distribution (3/3 plans) -- completed 2026-03-18

**31/31 requirements | 18 tools | 132 unit tests | 5 UAT suites**

See `.planning/milestones/v1.0-ROADMAP.md` for full details.

</details>

### v1.1 UI & Editor Expansion (In Progress)

**Milestone Goal:** AI can operate Godot's UI system (Control nodes, Themes, layouts), edit animations, manage scene files, capture viewport screenshots, and inject input into running games -- upgrading from "basic editor assistant" to "full editor assistant."

- [x] **Phase 6: Scene File Management** - AI can save, open, create, and manage scene files
- [x] **Phase 7: UI System** - AI can build and style Control node hierarchies with themes and layouts
- [x] **Phase 8: Animation System** - AI can create and edit animations with tracks and keyframes
- [x] **Phase 9: Editor Viewport Screenshots** - AI can capture editor viewport images for visual feedback
- [x] **Phase 10: Running Game Bridge** - AI can inject input and capture screenshots from a running game (completed 2026-03-18)
- [ ] **Phase 11: Prompt Templates** - AI gets curated workflow templates for UI building and animation setup

## Phase Details

### Phase 6: Scene File Management
**Goal**: AI can persist, load, and organize scene files without manual intervention
**Depends on**: Phase 5 (v1.0 complete)
**Requirements**: SCNF-01, SCNF-02, SCNF-03, SCNF-04, SCNF-05, SCNF-06
**Success Criteria** (what must be TRUE):
  1. AI can save the current scene to disk and the file appears on the filesystem
  2. AI can open an existing .tscn file and it becomes the active edited scene
  3. AI can list all currently open scenes in the editor and get their file paths
  4. AI can create a brand-new scene with a specified root node type and it opens in the editor
  5. AI can instantiate a PackedScene (.tscn) as a child node in the current scene
**Plans:** 3 plans
Plans:
- [ ] 06-01-PLAN.md -- Register 5 scene file tool definitions in MCP tool registry + unit tests
- [ ] 06-02-PLAN.md -- Implement scene_file_tools module + wire into MCP server dispatch
- [ ] 06-03-PLAN.md -- Create Phase 6 UAT test script for all SCNF requirements

### Phase 7: UI System
**Goal**: AI can construct, layout, and style Control node UIs including theme customization
**Depends on**: Phase 6
**Requirements**: UISYS-01, UISYS-02, UISYS-03, UISYS-04, UISYS-05, UISYS-06
**Success Criteria** (what must be TRUE):
  1. AI can set a Control node's layout preset (e.g., full rect, center, top-wide) and the node repositions correctly
  2. AI can apply per-control theme overrides (colors, fonts, font sizes, styleboxes) and they render visually
  3. AI can create a StyleBoxFlat with rounded corners, borders, and background color and apply it to a Control
  4. AI can query a Control node's UI-specific properties (anchors, size flags, minimum size, focus neighbors)
  5. AI can configure Container layout parameters (separation, alignment) and child size flags for correct flow
**Plans:** 2/3 plans executed
Plans:
- [ ] 07-01-PLAN.md -- Register 6 UI tool definitions in MCP tool registry + unit tests
- [ ] 07-02-PLAN.md -- Implement ui_tools module + wire into MCP server dispatch
- [ ] 07-03-PLAN.md -- Create Phase 7 UAT test script for all UISYS requirements

### Phase 8: Animation System
**Goal**: AI can create animations with tracks and keyframes for any animatable property
**Depends on**: Phase 6
**Requirements**: ANIM-01, ANIM-02, ANIM-03, ANIM-04, ANIM-05
**Success Criteria** (what must be TRUE):
  1. AI can create an AnimationPlayer with an AnimationLibrary and named Animation resources
  2. AI can add typed tracks (value, position3D, rotation3D) to an Animation with correct node paths
  3. AI can insert, modify, and remove keyframes on any track at specified times
  4. AI can query an AnimationPlayer and get back its animation list, track structure, and keyframe data
  5. AI can set animation properties (duration, loop mode, step) and the animation plays correctly in-editor
**Plans:** 3 plans
Plans:
- [ ] 08-01-PLAN.md -- Register 5 animation tool definitions in MCP tool registry + unit tests
- [ ] 08-02-PLAN.md -- Implement animation_tools module + wire into MCP server dispatch
- [ ] 08-03-PLAN.md -- Create Phase 8 UAT test script for all ANIM requirements

### Phase 9: Editor Viewport Screenshots
**Goal**: AI can see what the editor viewport looks like via captured images
**Depends on**: Phase 6
**Requirements**: VWPT-01, VWPT-02, VWPT-03
**Success Criteria** (what must be TRUE):
  1. AI can request a screenshot of the editor 2D viewport and receive a base64 PNG image
  2. AI can request a screenshot of the editor 3D viewport and receive a base64 PNG image
  3. Screenshots are returned as MCP ImageContent (not text-embedded base64) and render in AI clients
**Plans:** 2/3 plans executed
Plans:
- [ ] 09-01-PLAN.md -- Register capture_viewport tool + create_image_tool_result protocol builder + unit tests
- [ ] 09-02-PLAN.md -- Implement viewport_tools module + wire into MCP server dispatch
- [ ] 09-03-PLAN.md -- Create Phase 9 UAT test script for all VWPT requirements

### Phase 10: Running Game Bridge
**Goal**: AI can interact with a running game by injecting input and capturing its viewport
**Depends on**: Phase 9 (ImageContent infrastructure)
**Requirements**: BRDG-01, BRDG-02, BRDG-03, BRDG-04, BRDG-05
**Success Criteria** (what must be TRUE):
  1. When a game launches, a companion autoload is automatically injected and establishes communication with the editor plugin
  2. AI can inject keyboard key events (press and release) into the running game and the game responds to them
  3. AI can inject mouse events (move, click, scroll) into the running game at specified coordinates
  4. AI can inject Input Action events (action_press/release) into the running game
  5. AI can capture a screenshot of the running game's viewport and receive it as MCP ImageContent
**Plans:** 3/3 plans complete
Plans:
- [ ] 10-01-PLAN.md -- Register 3 game bridge tool definitions in MCP tool registry + unit tests
- [ ] 10-02-PLAN.md -- Implement game_bridge module (EditorDebuggerPlugin + companion GDScript) + deferred response + wire dispatch
- [ ] 10-03-PLAN.md -- Create Phase 10 UAT test script for all BRDG requirements

### Phase 11: Prompt Templates
**Goal**: AI gets guided workflow templates for common v1.1 tasks
**Depends on**: Phase 10 (all tool names finalized)
**Requirements**: PMPT-01, PMPT-02
**Success Criteria** (what must be TRUE):
  1. AI can use a UI building prompt template that guides creation of a complete UI layout (e.g., main menu, HUD)
  2. AI can use an animation setup prompt template that guides creation of a complete animation (e.g., walk cycle, UI transition)
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 6 -> 7 -> 8 -> 9 -> 10 -> 11
(Phases 7, 8, 9 are independent of each other but all depend on Phase 6. Ordered by complexity.)

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Foundation & First Tool | v1.0 | 4/4 | Complete | 2026-03-16 |
| 2. Scene CRUD | v1.0 | 3/3 | Complete | 2026-03-16 |
| 3. Script & Project Management | v1.0 | 3/3 | Complete | 2026-03-17 |
| 4. Editor Integration | v1.0 | 2/2 | Complete | 2026-03-18 |
| 5. Runtime, Signals & Distribution | v1.0 | 3/3 | Complete | 2026-03-18 |
| 6. Scene File Management | v1.1 | 3/3 | Complete | 2026-03-18 |
| 7. UI System | v1.1 | 3/3 | Complete | 2026-03-18 |
| 8. Animation System | v1.1 | 3/3 | Complete | 2026-03-18 |
| 9. Editor Viewport Screenshots | v1.1 | 3/3 | Complete | 2026-03-18 |
| 10. Running Game Bridge | 3/3 | Complete   | 2026-03-18 | - |
| 11. Prompt Templates | v1.1 | 0/0 | Not started | - |
