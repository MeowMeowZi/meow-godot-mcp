---
phase: 6
slug: scene-file-management
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-18
---

# Phase 6 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (existing) + Python UAT (existing) |
| **Config file** | `tests/CMakeLists.txt` (unit tests), `tests/uat_phase3.py` (UAT pattern) |
| **Quick run command** | `cd build && ctest --output-on-failure -R scene_file` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --output-on-failure -R scene_file`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 1 | SCNF-01 | unit | `ctest -R scene_file_save` | ❌ W0 | ⬜ pending |
| 06-01-02 | 01 | 1 | SCNF-02 | unit | `ctest -R scene_file_open` | ❌ W0 | ⬜ pending |
| 06-01-03 | 01 | 1 | SCNF-03 | unit | `ctest -R scene_file_list` | ❌ W0 | ⬜ pending |
| 06-01-04 | 01 | 1 | SCNF-04 | unit | `ctest -R scene_file_create` | ❌ W0 | ⬜ pending |
| 06-01-05 | 01 | 1 | SCNF-05 | unit | `ctest -R scene_file_save` | ❌ W0 | ⬜ pending |
| 06-01-06 | 01 | 1 | SCNF-06 | unit | `ctest -R scene_file_instantiate` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_scene_file_tools.cpp` — unit test stubs for SCNF-01..06
- [ ] Update `tests/CMakeLists.txt` — register new test file

*Existing GoogleTest infrastructure covers framework needs.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Scene opens in editor tab | SCNF-02 | Requires running Godot editor | Open .tscn via MCP, verify tab appears |
| Created scene opens in editor | SCNF-04 | Requires running Godot editor | Create scene via MCP, verify tab and root node |
| Instantiated scene appears in tree | SCNF-06 | Requires running Godot editor | Instantiate via MCP, verify child node in scene tree |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
