# Research Summary: v1.6 MCP Detail Optimizations

**Domain:** C++ GDExtension MCP Server Plugin -- reliability and persistence
**Researched:** 2026-03-31
**Overall confidence:** HIGH

## Executive Summary

v1.6 is a reliability and persistence milestone with zero new dependencies and zero new tools. All six features (port persistence, tool disable persistence, IO thread timeout, deferred request timeout, dual-panel logging, dead code cleanup) modify existing code rather than adding new modules. The research found that every required API is already available in the project's existing stack: `ProjectSettings::save()` for persistence, `std::condition_variable_any::wait_for()` for timeouts, and `UtilityFunctions::push_error()` for dual-panel logging.

The most architecturally significant finding is that the IO thread's `response_cv.wait()` call (mcp_server.cpp line 278) has no timeout at all -- if the main thread hangs or a deferred response never arrives, the IO thread blocks forever and the MCP connection is effectively dead. This is a violation of MCP spec 2025-03-26 which states implementations SHOULD establish timeouts for all sent requests. The fix is a straightforward replacement with `wait_for()` using a 30-second timeout.

The port auto-increment logic is confirmed as a desync bug, not a feature. The bridge executable is configured with a fixed `--port` at AI client setup time. When the GDExtension silently increments to a different port, bridge and server are permanently mismatched with no obvious error. Research confirms the correct fix is fail-fast with `push_error()` rather than any form of port negotiation.

All changes touch only `mcp_plugin.cpp`, `mcp_server.cpp`, `game_bridge.h/cpp`, and `error_enrichment.cpp`. No new source files are needed. The tool registry stays pure C++ without Godot dependencies, preserving unit testability.

## Key Findings

**Stack:** Zero new dependencies. All APIs already in godot-cpp and C++17 standard library.
**Architecture:** Five targeted modifications to existing files. No new modules needed.
**Critical pitfall:** IO thread infinite wait + deferred request infinite wait = two separate timeout gaps that must both be addressed.

## Implications for Roadmap

Based on research, suggested phase structure:

1. **Persistence + Port Fix** - Port persistence, tool disable persistence, remove auto-increment
   - Addresses: Port lost on restart, tool toggles lost on restart, bridge/plugin desync
   - Avoids: Auto-increment desync pitfall
   - Risk: LOW -- all main-thread ProjectSettings operations
   - Files: `mcp_plugin.cpp` only

2. **Timeout Safety** - IO thread wait_for + deferred game bridge timeout
   - Addresses: Infinite IO thread block, hung deferred requests
   - Avoids: MCP connection death on main thread hang
   - Risk: MEDIUM -- threading change (IO thread) + state machine change (game bridge)
   - Files: `mcp_server.h/cpp`, `game_bridge.h/cpp`

3. **Cleanup** - Dual logging + stale TOOL_PARAM_HINTS removal
   - Addresses: Errors only in Output panel, dead code for removed tools
   - Risk: LOW -- additive logging + deletion only
   - Files: `mcp_plugin.cpp`, `mcp_server.cpp`, `error_enrichment.cpp`

**Phase ordering rationale:**
- Phase 1 first because persistence is prerequisite for removing auto-increment (user needs a way to set and keep a non-default port)
- Phase 2 second because timeout changes are the highest-risk items and need the most testing
- Phase 3 last because it is independent cleanup with no dependencies

**Research flags for phases:**
- Phase 2: Needs careful testing of the IO thread timeout -- specifically the race condition where a response arrives at the exact moment timeout fires. Also needs to verify that late responses after timeout do not corrupt the next request's response.
- Phase 1 and 3: Standard patterns, unlikely to need research.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Zero new deps confirmed. All APIs verified in local godot-cpp headers and C++17 standard. |
| Features | HIGH | All six features are well-scoped bugfixes/improvements to existing code. No ambiguity in requirements. |
| Architecture | HIGH | Direct codebase analysis of all 6 affected files. Integration points are precise and localized. |
| Pitfalls | HIGH | Threading pitfalls (stale response after timeout, deadline race) identified with specific mitigations. |

## Gaps to Address

- **Late response handling after IO timeout:** When the IO thread times out and sends an error, the main thread may later push the actual response to the queue. This stale response would be sent as a reply to the NEXT request. Mitigation: tag responses with request IDs and skip mismatches. This is a design detail for phase-level planning, not a research gap.
- **ProjectSettings::save() behavior with default values:** Godot may skip saving values that match `set_initial_value()`. Need runtime verification that `meow_mcp/server/port=6800` actually appears in project.godot when the user explicitly sets it to 6800 (the default). If not, the workaround is to not call `set_initial_value()` for this setting.

## Sources

- Codebase: `mcp_server.h/cpp`, `mcp_plugin.h/cpp`, `game_bridge.h/cpp`, `mcp_tool_registry.h/cpp`, `error_enrichment.h/cpp`
- [Thread Safety in GDExtension](https://vorlac.github.io/gdextension-docs/advanced_topics/thread-safety/)
- [std::condition_variable::wait_for](https://en.cppreference.com/w/cpp/thread/condition_variable/wait_for)
- [ProjectSettings & EditorSettings](https://deepwiki.com/godotengine/godot/3.3-projectsettings-and-editorsettings)
- [ProjectSettings docs](https://docs.godotengine.org/en/stable/classes/class_projectsettings.html)
