# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.0 — MVP

**Shipped:** 2026-03-18
**Phases:** 5 | **Plans:** 15 | **Timeline:** 5 days

### What Was Built
- Two-process MCP architecture (stdio bridge + GDExtension TCP relay) with zero external dependencies
- 18 MCP tools across scene CRUD, script management, project queries, runtime control, signal wiring
- Editor dock panel with live connection status, version detection, controls
- Full MCP spec 2025-03-26 compliance (tools, resources, prompts)
- 132 unit tests + 5 automated UAT suites
- Cross-platform CI/CD (Windows/Linux/macOS)

### What Worked
- Risk-first phase ordering: proving the novel bridge-relay architecture in Phase 1 eliminated the biggest unknown early
- TDD-style workflow (RED then GREEN commits) kept each plan focused and verifiable
- Separate C++ unit tests (CMake/GoogleTest) + Python UAT (TCP client) provided fast iteration on protocol logic without needing Godot running
- `ifdef GODOT_MCP_MEOW_GODOT_ENABLED` pattern allowed comprehensive unit testing of Godot-free code paths
- Wave-based parallel execution kept phases independent and context-efficient

### What Was Inefficient
- Phase 4 Plan 01 (dock panel) was previously executed in a prior session; re-discovery took time
- UAT Phase 5 Test 4 (get_game_output) required 3 iterations to diagnose Windows file locking behavior
- Some SUMMARY.md files lack one-liner fields, making automated extraction harder at milestone completion

### Patterns Established
- Bridge uses separate SCons Environment() (zero godot-cpp dependency)
- MCPServer is plain C++ class (not Godot Object), protocol layer kept Godot-free
- ToolDef registry with GodotVersion-based filtering for forward compatibility
- Two-layer variant parser: pure C++ hint-based + Godot-dependent fallback
- Signal/runtime tools follow same module pattern: GODOT_ENABLED guard, standalone functions

### Key Lessons
1. ResourceLoader::load() crashes on newly created .gd files in Godot — use manual GDScript construction (instantiate + set_source_code + set_path + reload)
2. Windows file locking is aggressive — always handle "cannot open" gracefully when game process may hold file handles
3. StreamPeer::get_data() returns Array (not PackedByteArray) in godot-cpp — need Array index access
4. EditorPlugin activation requires plugin.cfg + thin GDScript wrapper; GDExtension class alone is insufficient
5. UndoRedo actions should set properties AFTER add_child/set_owner within the same action for correctness

### Cost Observations
- Model mix: ~80% opus (execution), ~15% sonnet (verification), ~5% haiku (none used)
- Average plan execution: 8 min
- Total execution time: ~2.1 hours across 15 plans
- Notable: Plans with checkpoints (UAT) took 2-3x longer due to human verification wait time

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Timeline | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | 5 days | 5 | Initial — established wave execution, TDD commits, automated UAT |

### Cumulative Quality

| Milestone | Unit Tests | UAT Suites | Tools |
|-----------|-----------|------------|-------|
| v1.0 | 132 | 5 (51 tests) | 18 |

### Top Lessons (Verified Across Milestones)

1. Risk-first ordering: prove novel architecture before building features on it
2. Keep protocol/logic layers Godot-free for unit testability
3. Automated UAT catches real integration issues that unit tests miss (e.g., file locking, scene state timing)
