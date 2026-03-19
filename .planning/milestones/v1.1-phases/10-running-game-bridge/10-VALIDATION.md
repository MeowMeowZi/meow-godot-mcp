---
phase: 10
slug: running-game-bridge
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-18
---

# Phase 10 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (existing) + Python UAT (existing) |
| **Config file** | `tests/CMakeLists.txt` (unit tests) |
| **Quick run command** | `cd tests/build && ctest --output-on-failure -C Debug -R game_bridge` |
| **Full suite command** | `cd tests/build && ctest --output-on-failure -C Debug` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick command
- **After every plan wave:** Run full suite
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 10-01-01 | 01 | 1 | BRDG-01 | file check | `test -f companion script` | ❌ W0 | ⬜ pending |
| 10-02-01 | 02 | 2 | BRDG-01..05 | unit | `ctest -R test_tool_registry` | ✅ | ⬜ pending |
| 10-03-01 | 03 | 3 | BRDG-01..05 | compile | `scons -j8` | ✅ | ⬜ pending |
| 10-04-01 | 04 | 4 | BRDG-01..05 | syntax | `python -c "import ast; ..."` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `addons/meow_godot_mcp/companion/meow_mcp_bridge.gd` — companion autoload script
- [ ] `tests/uat_phase10.py` — UAT test stubs for BRDG-01..05

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Autoload connects on game launch | BRDG-01 | Requires running Godot game | Launch game, check bridge status |
| Keyboard input reaches game | BRDG-02 | Requires running game | Inject key, verify game responds |
| Mouse input at coordinates | BRDG-03 | Requires running game | Inject mouse click, verify position |
| Action input triggers game logic | BRDG-04 | Requires running game with action map | Inject action, verify response |
| Game viewport screenshot | BRDG-05 | Requires running game | Capture viewport, verify PNG |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
