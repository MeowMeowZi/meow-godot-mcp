---
phase: 1
slug: foundation-first-tool
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-16
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.x |
| **Config file** | tests/CMakeLists.txt (separate from SCons build) |
| **Quick run command** | `cd tests && cmake --build build && ctest --output-on-failure` |
| **Full suite command** | `cd tests && cmake --build build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd tests && cmake --build build && ctest --output-on-failure`
- **After every plan wave:** Run `cd tests && cmake --build build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 0 | MCP-02 | unit | `ctest -R test_protocol` | -- Wave 0 | ⬜ pending |
| 01-01-02 | 01 | 0 | MCP-03 | unit | `ctest -R test_protocol` | -- Wave 0 | ⬜ pending |
| 01-01-03 | 01 | 0 | SCNE-01 | unit | `ctest -R test_scene_tree` | -- Wave 0 | ⬜ pending |
| 01-01-04 | 01 | 0 | MCP-04 | unit | `ctest -R test_threading` | -- Wave 0 | ⬜ pending |
| 01-xx-xx | xx | x | MCP-01 | integration | Manual (bridge + GDExtension) | N/A | ⬜ pending |
| 01-xx-xx | xx | x | DIST-01 | manual-only | Visual inspection | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/CMakeLists.txt` — GoogleTest setup with FetchContent
- [ ] `tests/test_protocol.cpp` — JSON-RPC parsing, initialize handshake, tools/list, tools/call, error codes
- [ ] `tests/test_scene_tree.cpp` — Scene tree JSON serialization format
- [ ] `tests/test_threading.cpp` — Thread-safe queue push/drain (if IO thread pattern used)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Bridge relays stdin to TCP and TCP to stdout | MCP-01 | Integration requires running both processes and actual TCP connection | 1. Start Godot with plugin 2. Run bridge 3. Send initialize via stdin 4. Verify response |
| Addon directory structure valid | DIST-01 | Structural/packaging concern | Verify `addons/godot_mcp_meow/` contains plugin.cfg, .gdextension, bin/ |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
