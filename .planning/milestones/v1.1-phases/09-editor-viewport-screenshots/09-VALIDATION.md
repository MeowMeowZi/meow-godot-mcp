---
phase: 9
slug: editor-viewport-screenshots
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-18
---

# Phase 9 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (existing) + Python UAT (existing) |
| **Config file** | `tests/CMakeLists.txt` (unit tests) |
| **Quick run command** | `cd tests/build && ctest --output-on-failure -C Debug -R viewport` |
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
| 09-01-01 | 01 | 1 | VWPT-01..03 | unit | `ctest -R test_tool_registry` | ✅ | ⬜ pending |
| 09-02-01 | 02 | 2 | VWPT-03 | compile | `scons -j8` | ✅ | ⬜ pending |
| 09-02-02 | 02 | 2 | VWPT-01..02 | compile | `scons -j8` | ✅ | ⬜ pending |
| 09-03-01 | 03 | 3 | VWPT-01..03 | syntax | `python -c "import ast; ..."` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/uat_phase9.py` — UAT test stubs for VWPT-01..03

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 2D viewport screenshot captures actual scene | VWPT-01 | Requires running Godot with 2D scene | Call capture_viewport via MCP, verify PNG decodes |
| 3D viewport screenshot captures actual scene | VWPT-02 | Requires running Godot with 3D scene | Call capture_viewport via MCP, verify PNG decodes |
| ImageContent renders in AI client | VWPT-03 | Requires Claude/Cursor client | Verify image appears in client UI |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
