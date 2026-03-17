---
phase: 5
slug: runtime-signals-distribution
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-17
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 + Python UAT scripts |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd tests/build && ctest --output-on-failure` |
| **Full suite command** | `cd tests/build && cmake .. && cmake --build . && ctest --output-on-failure` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd tests/build && ctest --output-on-failure`
- **After every plan wave:** Run `cd tests/build && cmake .. && cmake --build . && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | RNTM-01 | unit | `ctest -R test_tool_registry --output-on-failure` | ❌ W0 | ⬜ pending |
| 05-01-02 | 01 | 1 | RNTM-02 | unit | `ctest -R test_tool_registry --output-on-failure` | ❌ W0 | ⬜ pending |
| 05-01-03 | 01 | 1 | RNTM-03 | UAT | `python tests/uat_phase5.py` | ❌ W0 | ⬜ pending |
| 05-02-01 | 02 | 1 | RNTM-04 | UAT | `python tests/uat_phase5.py` | ❌ W0 | ⬜ pending |
| 05-02-02 | 02 | 1 | RNTM-05 | UAT | `python tests/uat_phase5.py` | ❌ W0 | ⬜ pending |
| 05-02-03 | 02 | 1 | RNTM-06 | UAT | `python tests/uat_phase5.py` | ❌ W0 | ⬜ pending |
| 05-03-01 | 03 | 2 | DIST-02 | CI | GitHub Actions pipeline | ❌ W0 | ⬜ pending |
| 05-03-02 | 03 | 2 | DIST-03 | manual | Manual: load plugin in Godot 4.3/4.4/4.5/4.6 | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_tool_registry.cpp` — add entries for run_game, stop_game, get_game_output, get_node_signals, connect_signal, disconnect_signal
- [ ] `tests/uat_phase5.py` — UAT covering runtime tools + signal tools against live Godot
- [ ] `.github/workflows/builds.yml` — CI pipeline for cross-platform builds

*Existing test infrastructure (GoogleTest + CMake) covers framework needs.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Binary works on Godot 4.3-4.6 | DIST-03 | Requires multiple Godot installations | 1. Download Godot 4.3, 4.4, 4.5, 4.6 2. Load plugin in each version 3. Verify MCP server starts and tools respond |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
