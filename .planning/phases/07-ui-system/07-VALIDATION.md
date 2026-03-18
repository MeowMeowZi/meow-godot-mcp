---
phase: 7
slug: ui-system
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-18
---

# Phase 7 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (existing) + Python UAT (existing) |
| **Config file** | `tests/CMakeLists.txt` (unit tests), `tests/uat_phase6.py` (UAT pattern) |
| **Quick run command** | `cd tests/build && ctest --output-on-failure -C Debug -R ui_tools` |
| **Full suite command** | `cd tests/build && ctest --output-on-failure -C Debug` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd tests/build && ctest --output-on-failure -C Debug -R ui_tools`
- **After every plan wave:** Run `cd tests/build && ctest --output-on-failure -C Debug`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 07-01-01 | 01 | 1 | UISYS-01..06 | unit | `ctest -R test_tool_registry` | ✅ | ⬜ pending |
| 07-02-01 | 02 | 2 | UISYS-01..06 | compile | `scons -j8` | ✅ | ⬜ pending |
| 07-03-01 | 03 | 3 | UISYS-01..06 | syntax | `python -c "import ast; ..."` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/uat_phase7.py` — UAT test stubs for UISYS-01..06

*Existing GoogleTest infrastructure covers unit test needs.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Layout preset repositions Control | UISYS-01 | Requires running Godot editor | Set preset via MCP, verify anchor values |
| Theme overrides render visually | UISYS-02 | Requires editor viewport | Apply override via MCP, check inspector |
| StyleBoxFlat renders correctly | UISYS-03 | Visual verification needed | Create stylebox via MCP, inspect Control |
| Container layout flows correctly | UISYS-05 | Requires editor with child nodes | Configure container via MCP, check layout |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
