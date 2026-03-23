# Roadmap: Godot MCP Meow

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-18)
- v1.1 UI & Editor Expansion -- Phases 6-11 (shipped 2026-03-19)
- v1.2 Runtime Interaction Enhancement -- Phases 12-15 (shipped 2026-03-20)
- v1.3 Developer Experience Polish -- Phases 16-18 (shipped 2026-03-22)
- v1.4 2D Game Development Core -- Phases 19-21 (shipped 2026-03-22)
- v1.5 AI Workflow Enhancement -- Phases 22-25 (in progress)

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

<details>
<summary>v1.1 UI & Editor Expansion (Phases 6-11) -- SHIPPED 2026-03-19</summary>

- [x] Phase 6: Scene File Management (3/3 plans) -- completed 2026-03-18
- [x] Phase 7: UI System (3/3 plans) -- completed 2026-03-18
- [x] Phase 8: Animation System (3/3 plans) -- completed 2026-03-18
- [x] Phase 9: Editor Viewport Screenshots (3/3 plans) -- completed 2026-03-18
- [x] Phase 10: Running Game Bridge (3/3 plans) -- completed 2026-03-18
- [x] Phase 11: Prompt Templates (1/1 plan) -- completed 2026-03-19

**27/27 requirements | 20 new tools (38 total) | 160 unit tests | 6 UAT suites**

See `.planning/milestones/v1.1-ROADMAP.md` for full details.

</details>

<details>
<summary>v1.2 Runtime Interaction Enhancement (Phases 12-15) -- SHIPPED 2026-03-20</summary>

- [x] Phase 12: Input Injection Enhancement (2/2 plans) -- completed 2026-03-20
- [x] Phase 13: Runtime State Query (2/2 plans) -- completed 2026-03-20
- [x] Phase 14: Game Output Enhancement (2/2 plans) -- completed 2026-03-20
- [x] Phase 15: Integration Testing Toolkit (2/2 plans) -- completed 2026-03-20

**12/12 requirements | 6 new tools (44 total) | 1 new prompt (7 total) | 4 UAT suites**

See `.planning/milestones/v1.2-ROADMAP.md` for full details.

</details>

<details>
<summary>v1.3 Developer Experience Polish (Phases 16-18) -- SHIPPED 2026-03-22</summary>

- [x] Phase 16: Game Bridge Auto-Wait (2/2 plans) -- completed 2026-03-22
- [x] Phase 17: Reliable Game Output (2/2 plans) -- completed 2026-03-21
- [x] Phase 18: Tool Ergonomics (1/1 plan) -- completed 2026-03-22

See `.planning/milestones/v1.3-ROADMAP.md` for full details.

</details>

<details>
<summary>v1.4 2D Game Development Core (Phases 19-21) -- SHIPPED 2026-03-22</summary>

- [x] Phase 19-21: Resource properties, TileMap ops, collision shape quick-create, restart_editor

**6 new tools (50 total) | 82 unit tests + 23 UAT tests**

</details>

### v1.5 AI Workflow Enhancement (In Progress)

**Milestone Goal:** From toolbox to intelligent workbench -- AI gets richer context, smarter error recovery, composite operations, and guided workflows.

- [ ] **Phase 22: Smart Error Handling** - Tool errors become diagnostic-rich: isError flag, did-you-mean suggestions, recovery guidance
- [ ] **Phase 23: Enriched Resources** - MCP Resources deliver scene context automatically: scripts, signals, node details, file metadata
- [ ] **Phase 24: Composite Tools** - Multi-step operations as single tools: find_nodes, batch_set_property, create_character, create_ui_panel, duplicate_node
- [ ] **Phase 25: Prompt Templates** - 8 workflow-oriented prompt templates for game building, debugging, and tool composition

## Phase Details

### Phase 22: Smart Error Handling
**Goal**: AI can self-correct from tool errors using diagnostic information, suggestions, and recovery guidance baked into every error response
**Depends on**: Phase 21 (v1.4 complete)
**Requirements**: ERR-01, ERR-02, ERR-03, ERR-04, ERR-05, ERR-06, ERR-07, ERR-08
**Success Criteria** (what must be TRUE):
  1. Every tool error response carries `isError: true` in the MCP result, distinguishing errors from successful empty results
  2. When AI references a node that does not exist, the error message suggests similar node names and lists the parent's children
  3. When a tool fails due to missing preconditions (no scene open, game not running, missing parameter), the error tells the AI exactly what to do next (which tool to call, what format to use)
  4. Script parse errors include the offending line number and line content so the AI can fix the exact problem
  5. Every error response includes a `suggested_tools` list the AI can use to recover
**Plans**: TBD

### Phase 23: Enriched Resources
**Goal**: AI automatically receives rich scene context (scripts, signals, properties, file metadata) through MCP Resources without calling individual query tools
**Depends on**: Phase 22
**Requirements**: RES-01, RES-02, RES-03
**Success Criteria** (what must be TRUE):
  1. Reading the scene_tree resource returns node entries that include script paths, signal connections, and key property values inline
  2. AI can query a single node's full details via `godot://node/{path}` resource template, and a script's content via `godot://script/{path}`
  3. The project files resource includes file size, type classification (scene/script/resource/image), and modification timestamps
**Plans**: TBD

### Phase 24: Composite Tools
**Goal**: AI can perform multi-step scene operations in a single tool call with atomic undo, eliminating tedious step-by-step workflows for common tasks
**Depends on**: Phase 22
**Requirements**: COMP-01, COMP-02, COMP-03, COMP-04, COMP-05
**Success Criteria** (what must be TRUE):
  1. AI can search the scene tree by type, name pattern, or property value using `find_nodes` and get matching results
  2. AI can set the same property on multiple nodes in one call using `batch_set_property` (by path list or type filter)
  3. AI can create a complete character (CharacterBody + CollisionShape + visual node) with `create_character`, and Ctrl+Z undoes the entire operation in one step
  4. AI can create a UI panel from a declarative JSON spec using `create_ui_panel`, producing container + children + styling in one step
  5. AI can deep-copy a node subtree to a new parent using `duplicate_node`, preserving all children and properties
**Plans**: TBD

### Phase 25: Prompt Templates
**Goal**: AI has workflow-oriented prompt templates that guide it through complex multi-tool tasks like building games, debugging crashes, and composing tools effectively
**Depends on**: Phase 24
**Requirements**: PROMPT-01, PROMPT-02, PROMPT-03, PROMPT-04, PROMPT-05, PROMPT-06, PROMPT-07, PROMPT-08
**Success Criteria** (what must be TRUE):
  1. AI can retrieve a tool composition guide that maps common tasks to specific tool sequences with parameter examples
  2. AI can follow a structured debug workflow (crash diagnosis, physics issues) that references the correct diagnostic and runtime tools
  3. AI can follow a complete game-building workflow (platformer, top-down, or parameterized by genre) from empty project to playable prototype
  4. All prompt templates reference only tools that actually exist in the tool registry (validated by unit test)
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 22 -> 23 -> 24 -> 25

| Phase | Milestone | Plans | Status | Completed |
|-------|-----------|-------|--------|-----------|
| 1. Foundation & First Tool | v1.0 | 4/4 | Complete | 2026-03-16 |
| 2. Scene CRUD | v1.0 | 3/3 | Complete | 2026-03-16 |
| 3. Script & Project Management | v1.0 | 3/3 | Complete | 2026-03-17 |
| 4. Editor Integration | v1.0 | 2/2 | Complete | 2026-03-18 |
| 5. Runtime, Signals & Distribution | v1.0 | 3/3 | Complete | 2026-03-18 |
| 6. Scene File Management | v1.1 | 3/3 | Complete | 2026-03-18 |
| 7. UI System | v1.1 | 3/3 | Complete | 2026-03-18 |
| 8. Animation System | v1.1 | 3/3 | Complete | 2026-03-18 |
| 9. Editor Viewport Screenshots | v1.1 | 3/3 | Complete | 2026-03-18 |
| 10. Running Game Bridge | v1.1 | 3/3 | Complete | 2026-03-18 |
| 11. Prompt Templates | v1.1 | 1/1 | Complete | 2026-03-19 |
| 12. Input Injection Enhancement | v1.2 | 2/2 | Complete | 2026-03-20 |
| 13. Runtime State Query | v1.2 | 2/2 | Complete | 2026-03-20 |
| 14. Game Output Enhancement | v1.2 | 2/2 | Complete | 2026-03-20 |
| 15. Integration Testing Toolkit | v1.2 | 2/2 | Complete | 2026-03-20 |
| 16. Game Bridge Auto-Wait | v1.3 | 2/2 | Complete | 2026-03-22 |
| 17. Reliable Game Output | v1.3 | 2/2 | Complete | 2026-03-21 |
| 18. Tool Ergonomics | v1.3 | 1/1 | Complete | 2026-03-22 |
| 19-21. 2D Game Dev Core | v1.4 | -- | Complete | 2026-03-22 |
| 22. Smart Error Handling | v1.5 | 0/TBD | Not started | - |
| 23. Enriched Resources | v1.5 | 0/TBD | Not started | - |
| 24. Composite Tools | v1.5 | 0/TBD | Not started | - |
| 25. Prompt Templates | v1.5 | 0/TBD | Not started | - |
