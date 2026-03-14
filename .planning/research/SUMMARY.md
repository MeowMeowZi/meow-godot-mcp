# Project Research Summary

**Project:** Godot MCP Meow - GDExtension MCP Server Plugin
**Domain:** C++ GDExtension embedding an MCP server inside the Godot editor process
**Researched:** 2026-03-14
**Confidence:** MEDIUM

## Executive Summary

Godot MCP Meow is a C++ GDExtension that embeds an MCP server directly inside the Godot editor process, eliminating the external Node.js/Python runtime that every existing Godot MCP server (7+ implementations) requires. The recommended approach uses godot-cpp v10 with C++17, nlohmann/json for protocol handling, SCons for building, and a custom MCP protocol implementation targeting spec version 2025-03-26. The project ships two native artifacts: the GDExtension shared library and a tiny (~50KB) bridge executable. No package manager, no runtime dependencies beyond Godot itself.

The fundamental architectural challenge is the subprocess problem. MCP's stdio transport requires the AI client to spawn the server as a subprocess, but a GDExtension is a shared library loaded into Godot's process -- it cannot be spawned independently. The solution is a lightweight bridge executable: a ~100-line C++ binary that the AI client launches, which relays stdio over TCP localhost to the GDExtension running inside Godot. The GDExtension holds all protocol logic and Godot API access; the bridge is a dumb relay. This architecture also solves the stdout contamination problem (Godot's own console output would corrupt MCP's JSON-RPC stream if they shared stdout) and allows Godot to start before or after the AI client connects.

The second critical risk is threading. MCP requests arrive on a background IO thread (via TCP from the bridge), but Godot's scene tree is NOT thread-safe -- Godot 4.1+ enforces thread guards that crash on violations. The architecture must use a producer-consumer pattern: the IO thread queues requests, the main thread processes them in `_process()` using `std::promise`/`std::future` for synchronization. All existing GDExtension threading pitfalls (constructor crashes, implicit `@tool` behavior, cross-version ABI breaks) are well-documented and avoidable with known patterns.

## Key Findings

### Recommended Stack

The stack is intentionally minimal: two dependencies total. godot-cpp (git submodule) provides C++ bindings for the GDExtension API, targeting Godot 4.3+ via the `api_version` parameter for forward compatibility through 4.4/4.5/4.6. nlohmann/json (single vendored header file, v3.12.0) handles all JSON parsing and serialization for the MCP protocol. SCons is the build system, matching godot-cpp's official tooling and avoiding known CMake MSVC flag issues.

**Core technologies:**
- **godot-cpp v10+ (C++17):** GDExtension bindings -- official, independently versioned, targets Godot 4.3+ with forward compatibility
- **nlohmann/json 3.12.0:** JSON-RPC 2.0 parsing/serialization -- header-only, de facto C++ standard, superior ergonomics for protocol work
- **SCons 4.5+:** Build system -- official godot-cpp tooling, GitHub Actions CI templates included, avoids MSVC `/MT` vs `/MD` edge case
- **Custom MCP implementation:** ~500-800 lines targeting spec 2025-03-26 -- all three C++ MCP SDKs are unsuitable (spec-outdated, enterprise-bloated, or architecturally incompatible)
- **TCP localhost:** Bridge-to-GDExtension IPC -- cross-platform consistent, debuggable, no library needed
- **GoogleTest 1.15.x:** Unit testing the protocol layer independently of Godot

### Expected Features

**Must have (table stakes):**
- Scene tree query (read hierarchy with node types, names, paths)
- Node CRUD (create, modify, delete nodes and properties)
- Script read/create/edit and attachment to nodes
- Project structure query (file listing, project settings)
- MCP stdio transport with JSON-RPC 2.0 (via bridge relay)
- Editor plugin UI dock (connection status, start/stop controls)
- Run/stop game and debug output capture
- Cross-platform support (Windows, Linux, macOS)
- Godot 4.3+ compatibility with version detection

**Should have (differentiators):**
- Native GDExtension with zero external dependencies -- the core competitive advantage
- Undo/redo integration for all scene mutations (only Godot MCP Pro has this)
- Signal management (query, connect, disconnect) -- central to Godot workflow
- Smart type parsing (auto-convert "Vector2(100,200)", "#ff0000" to proper Godot types)
- MCP Resources and Prompts (most competitors only implement tools, not the full spec)
- Resource file management (.tres/.res)
- Version-adaptive API (runtime detection, conditional tool availability)

**Defer (v2+):**
- Batch operations (useful but not critical early)
- Screenshot/viewport capture (high complexity, though market is moving toward it -- revisit)
- Input simulation (depends on stable run/debug foundation)
- HTTP/SSE transport (no demand signal for v1; stdio works with all major AI clients)
- Domain-specific tools (AnimationTree, TileMap, Shader, Particle, Navigation)

### Architecture Approach

The architecture has five major components inside the Godot editor process, plus one external bridge executable. The bridge relays stdio to TCP localhost. Inside the GDExtension: TcpTransport accepts the bridge connection on a background thread; McpProtocol parses JSON-RPC 2.0 and manages the MCP lifecycle state machine; ToolRegistry maps tool names to handler functions; GodotBridge marshals operations from the IO thread to the main thread using a queue-and-promise pattern; and McpMeowPlugin (EditorPlugin subclass) owns all components and provides the dock UI.

**Major components:**
1. **Bridge Executable** -- tiny native binary (~50KB) spawned by AI client; relays stdin/stdout to TCP localhost; zero protocol awareness
2. **TcpTransport** -- background IO thread; TCP accept/read/write on localhost; newline-delimited JSON message framing
3. **McpProtocol** -- JSON-RPC 2.0 state machine; MCP lifecycle (initialize, capability negotiation, ready, shutdown); request routing
4. **ToolRegistry** -- maps tool names to handler functions; provides `tools/list` response; thread-safe (register on main, lookup on IO)
5. **GodotBridge** -- the critical component; queues operations for main thread via `std::promise`/`std::future`; all Godot API access goes through here
6. **McpMeowPlugin** -- EditorPlugin at `MODULE_INITIALIZATION_LEVEL_EDITOR`; owns all sub-components; dock UI for status and controls

### Critical Pitfalls

1. **Stdio ownership problem** -- A GDExtension shares Godot's stdout. Writing MCP JSON-RPC to stdout corrupts the protocol stream with engine log output. **Avoid:** Use the bridge executable architecture; the GDExtension never touches stdin/stdout directly, communicating only over TCP localhost.

2. **Thread-unsafe Godot API access** -- Calling scene tree methods from the IO thread crashes with thread guard violations (Godot 4.1+). **Avoid:** All Godot API calls go through GodotBridge's main-thread queue. Never call any Godot API from the network thread -- not even `String::utf8()`.

3. **Constructor and initialization crashes** -- GDExtension constructors run before the object is integrated into Godot's system; `EditorHelp` instantiates every class for documentation. **Avoid:** Trivial constructors only. Move all initialization to `_enter_tree()`. Never start servers or open sockets in constructors.

4. **Cross-version ABI breaks** -- GDExtension ABI has broken at nearly every minor version boundary before 4.3. **Avoid:** Target Godot 4.3+ minimum with `compatibility_minimum = "4.3"` (always quoted). Use godot-cpp v10's `api_version=4.3` for forward compatibility.

5. **Stdout pollution in bridge** -- Any stray `printf` or library output to stdout in the bridge binary corrupts the MCP stream. **Avoid:** All logging to stderr from day one. Audit third-party libraries for stdout output. Never use `std::cout` or `printf` in the bridge.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Foundation and Transport

**Rationale:** De-risks the highest-risk architectural question: can the bridge + GDExtension + TCP relay architecture establish an MCP connection with real AI clients? Everything else depends on this working.
**Delivers:** A GDExtension that loads in the editor, a bridge executable that relays stdio to TCP, successful MCP `initialize`/`initialized` handshake with Claude Desktop or Cursor.
**Addresses:** MCP stdio transport, GDExtension scaffold, bridge executable, TcpTransport, McpProtocol (handshake only), `.gdextension` configuration, SCons build for both artifacts.
**Avoids:** Pitfall 1 (stdio ownership -- solved by bridge architecture), Pitfall 4 (cross-version -- 4.3+ minimum set from start), Pitfall 5 (constructor crashes -- trivial constructors established), Pitfall 15 (.gdextension quoting bug).

### Phase 2: Thread Bridge and First Tool

**Rationale:** De-risks the second highest risk: cross-thread marshalling between the IO thread and Godot's main thread. Proves the entire pipeline end-to-end with one real tool.
**Delivers:** GodotBridge with queue-and-promise pattern, ToolRegistry with `tools/list` and `tools/call` routing, `get_scene_tree` as the first functional tool. AI client can query the scene tree of an open project.
**Addresses:** GodotBridge implementation, ToolRegistry, scene query tool, `_process()` draining of pending operations.
**Avoids:** Pitfall 3 (thread-unsafe API access -- GodotBridge pattern proven here), Pitfall 7 (JSON-RPC compliance -- strict protocol tests written alongside).

### Phase 3: Core Scene CRUD

**Rationale:** Delivers the core value proposition -- AI can read AND write to the Godot scene tree. This is the minimum viable product that users would install.
**Delivers:** Node creation, property modification, node deletion, node path queries. The plugin becomes usable for basic AI-assisted scene building.
**Addresses:** Node CRUD tools (`create_node`, `set_node_property`, `delete_node`, `rename_node`), richer scene queries (`get_node_properties`, `get_node_by_path`).
**Avoids:** Pitfall 12 (STL vs Godot containers -- boundary conversion patterns established).

### Phase 4: Editor Integration and Expansion

**Rationale:** Adds polish, trust features (undo/redo), and expands tool coverage to scripts and project info. Makes the plugin feel production-ready.
**Delivers:** Dock UI panel (status, controls), script management tools, project info tools, undo/redo integration for all mutations, version detection with conditional tool availability.
**Addresses:** Editor plugin UI, script read/create/edit, project structure query, undo/redo wrapping, Godot version detection, smart type parsing.
**Avoids:** Pitfall 2 (implicit @tool -- UI lifecycle properly managed), Pitfall 10 (hot-reload -- robust socket cleanup with SO_REUSEADDR).

### Phase 5: Advanced Features and Runtime Loop

**Rationale:** Adds the autonomous build-test-fix loop (run game, capture output, iterate) and Godot-specific differentiators (signals, resources). Depends on stable foundations from Phases 1-4.
**Delivers:** Run/stop game control, debug output capture, signal management, resource file management, MCP Resources and Prompts per spec.
**Addresses:** Run/debug tools, signal management, resource management, MCP Resources (structured data), MCP Prompts (workflow templates).
**Avoids:** Pitfall 8 (Windows GUI/console -- cross-platform validation here), Pitfall 14 (UTF-8 encoding -- explicit encoding setup for Windows).

### Phase Ordering Rationale

- **Risk-first:** Phase 1 tackles the novel bridge architecture, Phase 2 tackles thread marshalling. These are the two unknowns where this project diverges from established patterns. If either fails, better to know immediately.
- **Dependency-driven:** Tools depend on transport (Phase 1 before 2). CRUD depends on scene queries (Phase 2 before 3). Script/project tools depend on stable CRUD patterns (Phase 3 before 4). Runtime features depend on everything (Phase 5 last).
- **Value-incremental:** Phase 3 is the first "shippable" milestone -- a user could install this and get value. Each subsequent phase widens the value.
- **Pitfall-aware:** The most dangerous pitfalls (stdio, threading, constructors, ABI) are all addressed in Phases 1-2 before the project accumulates complexity.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** The bridge-to-GDExtension TCP relay is novel -- no existing project uses this exact pattern. Needs prototype validation with at least Claude Desktop and Cursor. Port discovery/configuration needs design.
- **Phase 5:** Run/debug control requires interfacing with Godot's debugger/process launch APIs (`EditorInterface::play_main_scene()`, output capture). Sparse documentation for doing this from GDExtension C++. Screenshot capture (if reconsidered) requires engine viewport internals.

Phases with standard patterns (skip research-phase):
- **Phase 2:** Thread marshalling with `std::promise`/`std::future` and `_process()` queue draining is well-documented. JSON-RPC 2.0 parsing with nlohmann/json is straightforward.
- **Phase 3:** Scene tree manipulation via `EditorInterface::get_edited_scene_root()` and standard Node APIs is thoroughly documented in Godot docs.
- **Phase 4:** EditorPlugin dock UI, UndoRedo system, and script/resource APIs all have official Godot documentation and examples.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | godot-cpp is official, nlohmann/json is industry standard, SCons is the documented path. All three C++ MCP SDKs evaluated and rejected with clear rationale. |
| Features | HIGH | 14+ competitors analyzed including commercial offerings. Table stakes and differentiators well-established by market evidence. |
| Architecture | MEDIUM | Individual components (TCP, threading, GDExtension, MCP protocol) are well-understood. The combined architecture (bridge + GDExtension + TCP relay) is novel -- no existing project uses this exact pattern. |
| Pitfalls | HIGH | 15 pitfalls identified with official Godot issue references. The critical ones (stdio, threading, constructors, ABI) are thoroughly documented with proven prevention strategies. |

**Overall confidence:** MEDIUM -- The architecture is novel in its combination, even though each piece is well-understood. Phase 1 exists specifically to validate this combination.

### Gaps to Address

- **Port management:** How the bridge discovers which port the GDExtension is listening on. Options: fixed default port (6680), config file written by GDExtension, command-line argument to bridge. Needs design decision in Phase 1.
- **Multiple editor instances:** If a user has two Godot editor instances open, each with the plugin, they need different ports. No research found on how to handle this gracefully.
- **Screenshot capture scope decision:** PROJECT.md marks it out of scope, but market research shows it becoming table stakes. The complexity is real (viewport capture in C++ requires engine internals), but competitors are shipping it. Revisit after Phase 4.
- **Performance at scale:** No data on serializing scene trees with 1000+ nodes. May need depth limits or pagination. Test during Phase 2-3.
- **macOS code signing:** Distributing native binaries on macOS requires code signing and notarization. Not addressed in research. Relevant for Phase 5+ distribution.

## Sources

### Primary (HIGH confidence)
- [godot-cpp GitHub (v10)](https://github.com/godotengine/godot-cpp) -- independent versioning, api_version, Godot 4.3+ support
- [Godot 4.4 GDExtension C++ docs](https://docs.godotengine.org/en/4.4/tutorials/scripting/gdextension/gdextension_cpp_example.html) -- official tutorial
- [MCP Specification 2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26/changelog) -- target protocol version
- [MCP Transports Specification](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports) -- stdio transport (stable across all versions)
- [Godot Thread-safe APIs](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html) -- threading constraints
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification) -- protocol foundation
- [nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0) -- JSON library

### Secondary (MEDIUM confidence)
- [Godot MCP Pro](https://godot-mcp.abyo.net/) -- 162 tools, most feature-rich competitor, feature landscape reference
- [cpp-mcp (hkr04)](https://github.com/hkr04/cpp-mcp) -- C++ MCP SDK evaluation (rejected: spec-outdated)
- [GDExtension threading forum](https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547) -- std::thread works in GDExtension
- [SCons vs CMake MSVC issue (godot-cpp #1459)](https://github.com/godotengine/godot-cpp/issues/1459) -- build system decision
- [Godot thread guards (#83900)](https://github.com/godotengine/godot/issues/83900) -- thread safety enforcement
- [godot-cpp-template](https://github.com/godotengine/godot-cpp-template) -- official project scaffolding

### Tertiary (LOW confidence)
- Multiple existing Godot MCP repos (Coding-Solo, ee0pdt, slangwald, tomyud1, bradypp) -- architecture reference, but all use Node.js/Python patterns that do not directly apply
- [GDExtension hot-reload issue (#66231)](https://github.com/godotengine/godot/issues/66231) -- development workflow impact

---
*Research completed: 2026-03-14*
*Ready for roadmap: yes*
