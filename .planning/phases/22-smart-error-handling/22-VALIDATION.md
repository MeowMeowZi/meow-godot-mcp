---
phase: 22
slug: smart-error-handling
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-24
---

# Phase 22 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest (C++ unit tests) + Python UAT (end-to-end) |
| **Config file** | `tests/CMakeLists.txt` (unit) / `tests/uat_v15_errors.py` (UAT) |
| **Quick run command** | `cd build && ctest --output-on-failure -R error` |
| **Full suite command** | `cd build && ctest --output-on-failure && python tests/uat_v15_errors.py` |
| **Estimated runtime** | ~15 seconds (unit) + ~30 seconds (UAT) |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --output-on-failure -R error`
- **After every plan wave:** Run `cd build && ctest --output-on-failure && python tests/uat_v15_errors.py`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 45 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 22-01-01 | 01 | 1 | ERR-01 | unit | `ctest -R error_result` | ❌ W0 | ⬜ pending |
| 22-01-02 | 01 | 1 | ERR-07 | unit | `ctest -R suggested_tools` | ❌ W0 | ⬜ pending |
| 22-02-01 | 02 | 1 | ERR-02 | unit | `ctest -R fuzzy_match` | ❌ W0 | ⬜ pending |
| 22-02-02 | 02 | 1 | ERR-06 | unit | `ctest -R type_hint` | ❌ W0 | ⬜ pending |
| 22-03-01 | 03 | 2 | ERR-03,ERR-04,ERR-05 | unit | `ctest -R enrichment` | ❌ W0 | ⬜ pending |
| 22-03-02 | 03 | 2 | ERR-08 | unit+manual | `ctest -R script_error` | ❌ W0 | ⬜ pending |
| 22-04-01 | 04 | 3 | ERR-01~08 | e2e | `python tests/uat_v15_errors.py` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_error_enrichment.cpp` — unit test stubs for error enrichment, fuzzy matching, type hints
- [ ] `tests/uat_v15_errors.py` — UAT test stubs for all 8 ERR requirements

*Existing GoogleTest infrastructure covers C++ unit tests. Python UAT infrastructure exists from prior phases.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Script parse error line context | ERR-08 | GDScript::reload() error capture needs runtime verification | 1. Write malformed .gd script via MCP. 2. Attach to node. 3. Verify error response includes line number. |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 45s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
