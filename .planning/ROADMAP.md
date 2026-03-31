# Roadmap: Godot MCP Meow

## Milestones

- v1.0 MVP -- Phases 1-5 (shipped 2026-03-18)
- v1.1 UI & Editor Expansion -- Phases 6-11 (shipped 2026-03-19)
- v1.2 Runtime Interaction Enhancement -- Phases 12-15 (shipped 2026-03-20)
- v1.3 Developer Experience Polish -- Phases 16-18 (shipped 2026-03-22)
- v1.4 2D Game Development Core -- Phases 19-21 (shipped 2026-03-22)
- v1.5 AI Workflow Enhancement -- Phases 22-25 (shipped 2026-03-24)
- v1.6 MCP Detail Optimizations -- Phases 26-28 (in progress)

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

<details>
<summary>v1.5 AI Workflow Enhancement (Phases 22-25) -- SHIPPED 2026-03-24</summary>

- [x] Phase 22: Smart Error Handling (2/2 plans) -- completed 2026-03-23
- [x] Phase 23: Enriched Resources (2/2 plans) -- completed 2026-03-24
- [x] Phase 24: Composite Tools (2/2 plans) -- completed 2026-03-24
- [x] Phase 25: Prompt Templates (2/2 plans) -- completed 2026-03-24

**19/19 requirements | 5 composite tools (55->30 after consolidation) | 55 unit tests | 15 prompts**

</details>

### v1.6 MCP Detail Optimizations (In Progress)

**Milestone Goal:** Reliability and persistence -- the plugin remembers user settings across restarts, fails fast on errors instead of silently degrading, and keeps its codebase clean.

- [ ] **Phase 26: Settings Persistence** - Port and tool-disable settings survive editor restart; port conflicts fail fast instead of silently desyncing
- [ ] **Phase 27: Timeout Safety** - IO thread and game bridge requests have bounded wait times; stale responses cannot corrupt subsequent requests
- [ ] **Phase 28: Logging & Cleanup** - Errors surface in both Output and Debugger panels; dead code from tool consolidation is removed

## Phase Details

### Phase 26: Settings Persistence
**Goal**: User's Dock panel configuration (port number, disabled tools) survives editor restarts, and port conflicts produce immediate visible errors instead of silent desync
**Depends on**: Phase 25 (v1.5 complete)
**Requirements**: PERSIST-01, PERSIST-02, PERSIST-03
**Success Criteria** (what must be TRUE):
  1. User sets a custom port in the Dock panel, restarts the editor, and the Dock panel shows the same port number without any manual re-entry
  2. User disables several tools via Dock checkboxes, restarts the editor, and those same tools remain disabled (not re-enabled to defaults)
  3. When the configured port is already in use by another application, the plugin shows an error message in both the Dock panel and Output log, and does NOT silently start on a different port
  4. The bridge executable and the GDExtension always use the same port number (no desync scenario possible)
**Plans:** 1 plan
Plans:
- [ ] 26-01-PLAN.md -- Port/tool persistence + fail-fast port conflict

### Phase 27: Timeout Safety
**Goal**: MCP tool calls and game bridge requests have bounded response times -- the IO thread never blocks forever, and late responses from timed-out requests cannot corrupt the next request
**Depends on**: Phase 26
**Requirements**: TIMEOUT-01, TIMEOUT-02, TIMEOUT-03
**Success Criteria** (what must be TRUE):
  1. When the Godot main thread hangs or is extremely slow, the AI client receives a JSON-RPC error response (not silence) within 30 seconds of sending a tool call
  2. When the running game does not respond to a deferred request (viewport capture, eval_in_game), the AI client receives a timeout error within 15 seconds instead of waiting forever
  3. If a response arrives after timeout has already fired and the error has been sent, that stale response is discarded and does not appear as the result of the next tool call
**Plans**: TBD

### Phase 28: Logging & Cleanup
**Goal**: Plugin errors are visible in both Godot output panels, and dead code left over from the 59-to-30 tool consolidation is removed
**Depends on**: Phase 26
**Requirements**: LOG-01, CLEAN-01
**Success Criteria** (what must be TRUE):
  1. When the plugin logs an error (e.g., port conflict, tool failure), the message appears in both the Output panel and the Debugger > Errors tab simultaneously
  2. The TOOL_PARAM_HINTS map in error_enrichment.cpp contains entries only for the current 30 tools -- no references to tools that were removed during the v1.5 consolidation
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 26 -> 27 -> 28

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
| 22. Smart Error Handling | v1.5 | 2/2 | Complete | 2026-03-23 |
| 23. Enriched Resources | v1.5 | 2/2 | Complete | 2026-03-24 |
| 24. Composite Tools | v1.5 | 2/2 | Complete | 2026-03-24 |
| 25. Prompt Templates | v1.5 | 2/2 | Complete | 2026-03-24 |
| 26. Settings Persistence | v1.6 | 0/1 | Not started | - |
| 27. Timeout Safety | v1.6 | 0/? | Not started | - |
| 28. Logging & Cleanup | v1.6 | 0/? | Not started | - |
