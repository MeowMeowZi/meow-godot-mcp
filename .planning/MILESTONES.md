# Milestones

## v1.6 MCP Detail Optimizations (Shipped: 2026-04-01)

**Phases completed:** 3 phases, 3 plans, 7 tasks

**Key accomplishments:**

- Port and disabled-tools persistence via ProjectSettings::save(), with fail-fast port conflict errors replacing silent auto-increment
- 30s IO thread timeout and 15s game bridge deferred timeout with stale response discard via request ID tracking
- push_error() replacing printerr() for TCP/input errors, and TOOL_PARAM_HINTS cleaned from 50 to 30 entries matching current registry

---

## v1.5 AI Workflow Enhancement (Shipped: 2026-03-24)

**Phases completed:** 4 phases, 8 plans, 12 tasks

**Key accomplishments:**

- Smart error handling: MCP isError protocol, Levenshtein fuzzy matching for node/class names, 10-category error classification with recovery guidance, 55 unit tests
- Enriched MCP Resources: scene tree with inline scripts/signals/properties, 3 URI resource templates (node/script/signals), project file metadata (size/type/mtime)
- 5 composite tools (55 total): find_nodes, batch_set_property, create_character, create_ui_panel, duplicate_node — all with atomic UndoRedo
- 8 new prompt templates (15 total): tool composition guide, debugging workflows, game-building workflows (platformer/top-down/parameterized)

---

## v1.4 2D Game Development Core (Shipped: 2026-03-22)

**Phases completed:** 3 phases (19-21), direct commit (no GSD planning)

**Key accomplishments:**

- Resource property support: res:// path loading, new:ClassName() inline creation
- TileMap operations: batch place/erase tiles, query tile data
- Collision shape quick-create: one-step CollisionShape2D/3D with configured shape
- restart_editor tool for DLL recompilation workflow
- 6 new tools (44 → 50), 82 unit tests + 23 UAT tests

---

## v1.3 Developer Experience Polish (Shipped: 2026-03-21)

**Phases completed:** 3 phases, 5 plans, 0 tasks

**Key accomplishments:**

- (none recorded)

---

## v1.2 Runtime Interaction Enhancement (Shipped: 2026-03-19)

**Phases completed:** 4 phases, 8 plans, 0 tasks

**Key accomplishments:**

- (none recorded)

---

## v1.1 UI & Editor Expansion (Shipped: 2026-03-19)

**Phases completed:** 6 phases, 16 plans, 0 tasks

**Key accomplishments:**

- (none recorded)

---

## v1.0 MVP (Shipped: 2026-03-18)

**Phases completed:** 5 phases, 15 plans | **Timeline:** 5 days (2026-03-14 → 2026-03-18)
**C++ LOC:** 3,496 (src/) + 2,418 (tests/) | **Commits:** 81 total (19 feat)

**Key accomplishments:**

- Two-process MCP architecture: stdio bridge (~50KB) + GDExtension TCP relay, zero external dependencies
- 18 MCP tools across 5 categories: scene CRUD, script management, project queries, runtime control, signal wiring
- Full MCP spec 2025-03-26 compliance: JSON-RPC 2.0, tools, resources, prompts with 4 workflow templates
- Editor dock panel with live connection status, version detection, Start/Stop/Restart controls
- 132 GoogleTest unit tests + 5 automated UAT suites covering all 31 requirements
- Cross-platform CI/CD: GitHub Actions for Windows/Linux/macOS builds

**31/31 v1 requirements satisfied** across 7 categories (MCP Transport, Scene Ops, Script Mgmt, Runtime & Debug, Project & Resources, Editor Integration, Distribution)

---
