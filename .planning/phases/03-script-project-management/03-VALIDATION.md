---
phase: 3
slug: script-project-management
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-17
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd tests/build && ctest --output-on-failure` |
| **Full suite command** | `cd tests/build && cmake --build . && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd tests/build && ctest --output-on-failure`
- **After every plan wave:** Run `cd tests/build && cmake --build . && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | SCRP-01 | unit (mock) | `ctest -R test_protocol --output-on-failure` | Wave 0 | ⬜ pending |
| 03-01-02 | 01 | 1 | SCRP-02 | unit (mock) | `ctest -R test_protocol --output-on-failure` | Wave 0 | ⬜ pending |
| 03-01-03 | 01 | 1 | SCRP-03 | unit | `ctest -R test_script_tools --output-on-failure` | Wave 0 | ⬜ pending |
| 03-01-04 | 01 | 1 | SCRP-04 | manual | Manual UAT -- requires running Godot editor | N/A | ⬜ pending |
| 03-02-01 | 02 | 1 | PROJ-01 | manual | Manual UAT -- requires res:// filesystem | N/A | ⬜ pending |
| 03-02-02 | 02 | 1 | PROJ-02 | manual | Manual UAT -- requires Godot ProjectSettings | N/A | ⬜ pending |
| 03-02-03 | 02 | 1 | PROJ-03 | manual | Manual UAT -- requires Godot ResourceLoader | N/A | ⬜ pending |
| 03-02-04 | 02 | 2 | PROJ-04 | unit | `ctest -R test_protocol --output-on-failure` | Wave 0 | ⬜ pending |
| 03-02-05 | 02 | 2 | MCP-04 | unit | `ctest -R test_protocol --output-on-failure` | Wave 0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_script_tools.cpp` — stubs for SCRP-03 line editing logic (pure C++ string manipulation)
- [ ] `tests/test_protocol.cpp` — extend with resources/list and resources/read response format tests (PROJ-04)
- [ ] `tests/test_protocol.cpp` — extend with new tool schema validation for Phase 3 tools (SCRP-01, SCRP-02, MCP-04)
- [ ] `tests/CMakeLists.txt` — add test_script_tools executable if new test file created

*Existing infrastructure covers automated requirements. Wave 0 adds test stubs for new functionality.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| attach/detach script to/from node | SCRP-04 | Requires running Godot editor with scene tree | 1. Open test scene 2. Call attach_script via MCP 3. Verify node has script in inspector |
| list_project_files returns flat file list | PROJ-01 | Requires Godot res:// filesystem | 1. Open project 2. Call list_project_files via MCP 3. Verify file list matches FileSystem dock |
| get_project_settings returns settings | PROJ-02 | Requires Godot ProjectSettings singleton | 1. Open project 2. Call get_project_settings via MCP 3. Verify settings match Project Settings editor |
| get_resource_info loads and inspects resource | PROJ-03 | Requires Godot ResourceLoader | 1. Create .tres resource 2. Call get_resource_info via MCP 3. Verify type and properties returned |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
