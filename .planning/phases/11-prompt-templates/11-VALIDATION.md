---
phase: 11
slug: prompt-templates
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-19
---

# Phase 11 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (existing) + Python UAT (existing) |
| **Config file** | `tests/CMakeLists.txt` (unit tests) |
| **Quick run command** | `cd tests/build && ctest --output-on-failure -C Debug -R prompt` |
| **Full suite command** | `cd tests/build && ctest --output-on-failure -C Debug` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick command
- **After every plan wave:** Run full suite
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 11-01-01 | 01 | 1 | PMPT-01,02 | compile+unit | `scons -j8 && ctest` | ✅ | ⬜ pending |
| 11-02-01 | 02 | 2 | PMPT-01,02 | syntax | `python -c "import ast; ..."` | ❌ W0 | ⬜ pending |

---

## Wave 0 Requirements

- [ ] `tests/uat_phase11.py` — UAT test stubs for PMPT-01..02

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| UI template guides complete layout creation | PMPT-01 | Requires AI client following steps | Use prompt via MCP, follow steps |
| Animation template guides complete setup | PMPT-02 | Requires AI client following steps | Use prompt via MCP, follow steps |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
