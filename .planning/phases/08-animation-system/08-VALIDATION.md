---
phase: 8
slug: animation-system
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-18
---

# Phase 8 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (existing) + Python UAT (existing) |
| **Config file** | `tests/CMakeLists.txt` (unit tests) |
| **Quick run command** | `cd tests/build && ctest --output-on-failure -C Debug -R animation` |
| **Full suite command** | `cd tests/build && ctest --output-on-failure -C Debug` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd tests/build && ctest --output-on-failure -C Debug -R animation`
- **After every plan wave:** Run `cd tests/build && ctest --output-on-failure -C Debug`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 08-01-01 | 01 | 1 | ANIM-01..05 | unit | `ctest -R test_tool_registry` | ✅ | ⬜ pending |
| 08-02-01 | 02 | 2 | ANIM-01..05 | compile | `scons -j8` | ✅ | ⬜ pending |
| 08-03-01 | 03 | 3 | ANIM-01..05 | syntax | `python -c "import ast; ..."` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/uat_phase8.py` — UAT test stubs for ANIM-01..05

*Existing GoogleTest infrastructure covers unit test needs.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Animation plays in editor | ANIM-05 | Requires running Godot editor | Create animation via MCP, click play in AnimationPlayer |
| Track node paths resolve | ANIM-02 | Requires scene with actual nodes | Create node + animation with track, verify track path |
| Keyframe values interpolate | ANIM-03 | Requires editor preview | Insert keyframes, scrub timeline, verify interpolation |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
