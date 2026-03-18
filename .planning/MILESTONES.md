# Milestones

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

