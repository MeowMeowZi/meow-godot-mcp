---
phase: 2
slug: scene-crud
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-16
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (via CMake FetchContent) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd tests/build && ctest --output-on-failure` |
| **Full suite command** | `cd tests && mkdir -p build && cd build && HTTPS_PROXY="http://127.0.0.1:7897" cmake .. -G "MinGW Makefiles" && cmake --build . && ctest --output-on-failure` |
| **Estimated runtime** | ~10 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd tests/build && ctest --output-on-failure`
- **After every plan wave:** Run full suite command
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | SCNE-02 | unit | `ctest -R test_scene_mutation` | ❌ W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | SCNE-03 | unit | `ctest -R test_scene_mutation` | ❌ W0 | ⬜ pending |
| 02-01-03 | 01 | 1 | SCNE-04 | unit | `ctest -R test_scene_mutation` | ❌ W0 | ⬜ pending |
| 02-02-01 | 02 | 1 | SCNE-06 | unit | `ctest -R test_variant_parser` | ❌ W0 | ⬜ pending |
| 02-03-01 | 03 | 2 | SCNE-05 | manual | N/A (EditorUndoRedoManager) | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_scene_mutation.cpp` — stubs for create/set_property/delete node operations
- [ ] `tests/test_variant_parser.cpp` — stubs for string-to-Variant parsing
- [ ] `tests/CMakeLists.txt` — add new test targets

*Existing GoogleTest infrastructure from Phase 1 covers framework installation.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Undo/Redo support | SCNE-05 | EditorUndoRedoManager requires running Godot editor | Create node via MCP, Ctrl+Z to undo, verify node removed, Ctrl+Y to redo |
| Node appears in editor | SCNE-02 | Visual editor state requires running Godot | Create node via MCP, verify it shows in Scene dock |
| Property changes visible | SCNE-03 | Inspector panel requires running Godot | Set property via MCP, verify Inspector shows new value |

*All manual verifications covered by UAT testing at phase end.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
