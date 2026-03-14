# Technology Stack

**Project:** Godot MCP Meow - GDExtension MCP Server Plugin
**Researched:** 2026-03-14
**Overall Confidence:** MEDIUM - The GDExtension + native MCP Server combination is novel territory; no existing implementation uses this approach. Individual components are well-understood.

## Executive Summary

This project builds something genuinely new: a C++ GDExtension that embeds an MCP Server directly inside the Godot editor process. Every existing Godot MCP server (7+ implementations found) uses a three-layer architecture: `AI Client <--stdio--> Node.js/Python process <--WebSocket/HTTP/TCP--> GDScript editor plugin`. Our approach collapses the middle two layers into a single C++ GDExtension, eliminating the external runtime dependency (Node.js/Python). This is the project's core differentiator and its primary technical risk.

**The fundamental architectural constraint:** MCP's stdio transport requires the AI client to launch the MCP server as a subprocess. A GDExtension is a shared library loaded into Godot's process -- it cannot be spawned as a subprocess. Therefore, a lightweight bridge executable is needed: a tiny native binary (~50KB) that the AI client spawns, which relays stdio to the GDExtension over TCP localhost. The GDExtension holds ALL protocol logic and Godot API access. The bridge is a dumb relay.

## Recommended Stack

### Core Framework

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| godot-cpp | v10.0.0+ (master) | C++ bindings for GDExtension API | Official bindings, independently versioned since v10, supports Godot 4.3+ via `api_version` parameter. Forward-compatible: build for 4.3, works on 4.4/4.5/4.6. | HIGH |
| Godot Engine | 4.3+ (target `api_version=4.3`) | Host engine | 4.3 is the minimum version godot-cpp v10 supports. Building against 4.3 API gives maximum user coverage while having EditorPlugin, threading, and all needed APIs. Forward-compatible to 4.4/4.5/4.6. | HIGH |
| C++17 | std=c++17 | Language standard | godot-cpp requires C++17 minimum. Gives us `std::optional`, `std::filesystem`, structured bindings, `if constexpr`, `std::thread`, `std::mutex`. No need for C++20 -- C++17 is sufficient and maximizes compiler compatibility. | HIGH |

### Build System

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| SCons | 4.5+ | Primary build system | Official godot-cpp build system. Most mature, best documented for GDExtension. CI/CD templates from godot-cpp-template use SCons. Avoid CMake's MSVC /MT vs /MD edge case (known godot-cpp issue #1459). | HIGH |
| Python | 3.8+ | SCons dependency | SCons requires Python. Already needed to build godot-cpp. | HIGH |

**Why not CMake?** CMake has feature parity in godot-cpp v10, but SCons is what the official template, CI workflows, and documentation use. The known MSVC `/MT` vs `/MD` flag discrepancy between SCons and CMake builds (godot-cpp issue #1459) makes SCons the safer default. Developers who prefer CMake can add it later -- godot-cpp supports both.

### JSON Library

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| nlohmann/json | 3.12.0 | JSON parsing/serialization for MCP protocol | De facto standard C++ JSON library. Header-only (single file). MCP is entirely JSON-RPC 2.0 -- this library handles it naturally. `json["key"] = value` syntax. Released April 2025, current. | HIGH |

**Why not rapidjson?** Inferior API ergonomics. Performance irrelevant at MCP message volumes (tens of messages/sec, not thousands).

**Why not Godot's built-in JSON?** Returns Variant types requiring constant casting for strict JSON-RPC 2.0 compliance. nlohmann provides native typed access that maps directly to JSON-RPC message structures.

### MCP Protocol Implementation

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Custom implementation | Target MCP spec 2025-03-26 | MCP protocol layer (JSON-RPC 2.0 + MCP semantics) | Build from scratch. See detailed rationale below. | MEDIUM |

**Why build MCP protocol handling from scratch?**

Three C++ MCP SDKs exist. None are suitable:

1. **cpp-mcp (hkr04/cpp-mcp)** -- 234 stars, MIT. Lightweight, header-only deps. **Problem:** Stuck on MCP spec 2024-11-05 (3 versions behind). Issue #10 confirms maintainer is too busy to update (last response Oct 2025). Users get "Unsupported protocol version" errors with modern clients. Also bundles cpp-httplib for HTTP transport we don't need. Stdio implementation is tightly coupled to its event loop.

2. **gopher-mcp (GopherSecurity)** -- 88 stars, Apache 2.0. Claims MCP 2025-06-18 support. **Problem:** Requires libevent 2.1+ and optionally OpenSSL -- heavy dependencies painful to integrate into a GDExtension SCons build. Enterprise-focused with connection pooling, circuit breaker, rate limiting we don't need. Architecture mismatch.

3. **mcp_server (peppemas)** -- Plugin architecture with dynamically loaded DLLs. **Problem:** Designed as standalone server, not embeddable in another process. Architecture mismatch with GDExtension.

**The MCP stdio protocol is straightforward to implement directly:**
- Read newline-delimited JSON from stdin, write newline-delimited JSON to stdout
- Parse JSON-RPC 2.0 messages (request with id+method+params, notification without id, response with result/error)
- Implement MCP lifecycle: initialize, initialized, tool calls, resource reads, shutdown
- Estimated ~500-800 lines of focused C++ for the protocol layer
- Full control over spec version negotiation

### MCP Spec Version Strategy

| Target Spec | Why | Confidence |
|-------------|-----|------------|
| 2025-03-26 for v1 | The 2024-11-05 spec is 16+ months old and causes "Unsupported protocol version" errors with Claude Desktop and Cursor. 2025-03-26 adds tool annotations (readOnly/destructive) which are useful, and is widely supported by current AI clients. | MEDIUM |

**Why not 2025-11-25 (latest)?** Adds async Tasks, enterprise OAuth, extensions framework, CIMD -- none relevant for a local stdio editor plugin. Core tools/resources API stable since 2025-03-26. Can upgrade later.

**Key fact:** Stdio transport is unchanged across ALL MCP spec versions. Message format (newline-delimited JSON-RPC over stdin/stdout) has been stable since the first spec. What changes between versions is HTTP transport, auth, and higher-level features.

### Bridge Executable (stdio relay)

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Native C++ bridge | Compile with project | Tiny executable: relays stdio to TCP localhost | The AI client (Claude Desktop, Cursor) spawns this as the "MCP server" subprocess. It reads JSON-RPC from stdin, forwards over TCP to the GDExtension listener, and relays responses to stdout. ~100 lines of code, ~50KB binary, zero dependencies. | MEDIUM |
| TCP localhost | Platform sockets | IPC between bridge and GDExtension | Simpler and more debuggable than named pipes. Cross-platform consistent (Winsock2 on Windows, POSIX sockets on Linux/macOS). No library needed. | MEDIUM |

**Why a bridge is needed:** MCP's stdio spec says "The client launches the MCP server as a subprocess." A GDExtension is a shared library inside Godot -- it cannot be that subprocess. The bridge solves this with a compiled native binary instead of requiring Node.js/Python runtime like every other solution.

### Editor Integration

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| godot-cpp EditorPlugin | via godot-cpp | Editor UI panel for MCP status/control | Register at `MODULE_INITIALIZATION_LEVEL_EDITOR`. Use `add_control_to_dock()` for status panel. Build UI programmatically in C++ -- no .tscn file dependency. | HIGH |
| godot-cpp SceneTree API | via godot-cpp | Scene query/manipulation | Core value. Access via `EditorInterface::get_edited_scene_root()` and scene tree traversal. | HIGH |

### Threading Model

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| std::thread | C++17 | Background TCP listener thread | TCP accept/read must be off main thread. C++ native threads work well in GDExtension -- confirmed by community. Don't use Godot's Thread class (incompatible with std::thread). | HIGH |
| std::mutex / std::condition_variable | C++17 | Thread synchronization | Bridge between TCP listener thread and Godot main thread. Queue MCP requests from TCP thread, process on main thread (Godot APIs are NOT thread-safe), send responses back. | HIGH |
| call_deferred() | via godot-cpp | Main thread dispatch | Use Godot's `call_deferred()` or `Callable.call_deferred()` to safely dispatch Godot API calls from background thread context to main thread. | HIGH |

**Critical constraint:** Most Godot APIs are NOT thread-safe. All scene tree access, node manipulation, and editor operations MUST happen on the main thread. The TCP listener thread receives requests, queues them, and the main thread processes them (via `_process()` or `call_deferred()`).

### Testing

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| GoogleTest | 1.15.x | Unit testing protocol layer | Protocol parsing, JSON-RPC handling, tool argument validation can all be tested independently of Godot. Standard C++ testing framework. | HIGH |
| Manual editor testing | N/A | Integration testing | No established C++ GDExtension test framework exists. Test by loading plugin in Godot and sending MCP commands via a test client script. | LOW |

### Project Scaffolding

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| godot-cpp-template | latest (Nov 2025) | Project structure baseline | Official quickstart from godotengine. Includes SConstruct boilerplate, GitHub Actions CI, .gdextension file template, proper directory layout. | HIGH |

## Supporting Libraries

| Library | Version | Purpose | When to Use | Confidence |
|---------|---------|---------|-------------|------------|
| nlohmann/json | 3.12.0 | All JSON operations | Every MCP message parse/serialize | HIGH |
| godot-cpp | v10.0.0+ | All Godot API interaction | Every editor operation | HIGH |

**Intentionally minimal.** Two dependencies total: godot-cpp (git submodule), nlohmann/json (single vendored header file). No package manager needed.

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| MCP SDK | Custom impl | cpp-mcp (hkr04) | Stuck on 2024-11-05 spec, maintainer inactive, causes version errors with modern clients |
| MCP SDK | Custom impl | gopher-mcp | Requires libevent+OpenSSL, enterprise overkill, hard to embed in GDExtension |
| MCP SDK | Custom impl | mcp_server (peppemas) | Standalone server design, not embeddable in another process |
| JSON | nlohmann/json | rapidjson | Worse API ergonomics, performance irrelevant at MCP volumes |
| JSON | nlohmann/json | Godot built-in JSON | Variant-typed returns require constant casting; poor for protocol work |
| Build | SCons | CMake | SCons is official, fewer edge cases (MSVC /MT vs /MD issue), official CI templates |
| Build | SCons | Meson | Not supported by godot-cpp at all |
| Bridge IPC | TCP localhost | Named pipes | Different APIs per platform (CreateNamedPipe vs mkfifo), harder to debug |
| Bridge IPC | TCP localhost | Shared memory | Overkill for message-based protocol, complex synchronization |
| Bridge IPC | TCP localhost | Unix domain sockets | Not available on Windows without WSL |
| Language | C++17 | C++20 | No compelling features needed; C++17 is godot-cpp baseline, maximizes compiler compat |
| Language | C++ GDExtension | GDScript + Node.js | This is what everyone else does. Our value prop IS the native approach. |
| Language | C++ GDExtension | Rust (godot-rust) | godot-cpp is official, more mature, better documented. Rust adds build complexity |
| MCP Spec | 2025-03-26 | 2024-11-05 | Causes "Unsupported protocol version" with modern AI clients |
| MCP Spec | 2025-03-26 | 2025-11-25 | Adds enterprise features irrelevant for local stdio plugin; can upgrade later |
| Godot API target | 4.3 (via api_version) | 4.1 or 4.0 | godot-cpp v10 minimum is 4.3. Earlier versions not supported. |
| Testing | GoogleTest | Catch2 | GoogleTest more widely used, better IDE integration |
| Packaging | vcpkg/conan | Direct vendor | Only 2 deps, package manager is overhead |

## Installation

```bash
# Clone with submodule
git clone --recurse-submodules https://github.com/user/godot-mcp-meow.git
cd godot-mcp-meow

# Or if already cloned
git submodule update --init --recursive

# Download nlohmann/json single header (one-time)
mkdir -p thirdparty/nlohmann
curl -L -o thirdparty/nlohmann/json.hpp \
  https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp

# Build GDExtension (Windows)
scons platform=windows target=template_debug

# Build GDExtension (Linux)
scons platform=linux target=template_debug

# Build GDExtension (macOS)
scons platform=macos target=template_debug

# Build bridge executable (compiled alongside GDExtension by SConstruct)
# Output: bin/godot-mcp-bridge.exe (Windows) or bin/godot-mcp-bridge (Linux/macOS)
```

### Project Structure

```
godot-mcp-meow/
  godot-cpp/                 # git submodule (godotengine/godot-cpp master)
  thirdparty/
    nlohmann/
      json.hpp               # vendored single header (v3.12.0)
  src/
    register_types.cpp/.h    # GDExtension entry point
    mcp/
      protocol.cpp/.h        # MCP protocol (JSON-RPC 2.0 + MCP lifecycle)
      server.cpp/.h          # MCP server (tool registry, request dispatch)
      transport.cpp/.h       # TCP listener for bridge connection
    editor/
      mcp_plugin.cpp/.h      # EditorPlugin subclass (dock UI, lifecycle)
    tools/
      scene_tools.cpp/.h     # MCP tools: scene query/manipulation
      node_tools.cpp/.h      # MCP tools: node CRUD
      script_tools.cpp/.h    # MCP tools: script management
  bridge/
    main.cpp                 # Bridge executable: stdio <-> TCP relay
  project/                   # Demo Godot project for testing
    addons/godot_mcp_meow/
      plugin.cfg
      godot_mcp_meow.gd      # Thin GDScript wrapper (@tool extends McpEditorPlugin)
      bin/
        godot_mcp_meow.gdextension
  SConstruct                 # Build script (GDExtension + bridge)
  tests/
    test_protocol.cpp        # GoogleTest: JSON-RPC parsing
    test_tools.cpp           # GoogleTest: tool argument validation
```

## Version Compatibility Matrix

| godot-cpp | api_version | Godot Engine Supported | MCP Spec | Status |
|-----------|-------------|----------------------|----------|--------|
| v10.0.0+ | 4.3 | 4.3, 4.4, 4.5, 4.6 | 2025-03-26 | Target for v1 |
| v10.0.0+ | 4.5 | 4.5, 4.6+ | 2025-11-25 | Future (if needed) |

## Platform Support

| Platform | Compiler | Library Extension | Bridge Extension | Priority |
|----------|----------|-------------------|-----------------|----------|
| Windows x86_64 | MSVC 2019+ | .dll | .exe | P0 (primary dev) |
| Linux x86_64 | GCC 9+ / Clang 10+ | .so | (no extension) | P1 |
| macOS x86_64 + arm64 | Apple Clang (Xcode) | .dylib | (no extension) | P1 |

## What NOT to Use

| Technology | Why Not |
|------------|---------|
| Node.js / Python MCP server | Eliminates core value prop: zero external runtime dependency |
| cpp-httplib | Only needed for HTTP MCP transport; we use stdio via bridge |
| Boost | Massive dependency for minimal benefit; C++17 std library sufficient |
| gRPC / Protobuf | MCP uses JSON-RPC, not protobuf |
| Qt | Heavy GUI framework; Godot EditorPlugin provides all UI |
| vcpkg / conan | Two dependencies don't justify a package manager |
| GDScript for protocol logic | C++ is already the chosen stack; GDScript can't do background I/O |
| Godot's Thread class | Incompatible with std::thread; use native C++ threads for I/O |
| cpp-mcp (any version) | Spec-outdated, maintenance-stalled, architecture mismatch |

## Competitive Landscape Context

All 7+ existing Godot MCP servers require Node.js or Python. Our approach ships as a self-contained Godot addon with a tiny native bridge executable. No runtime dependencies beyond Godot.

| Existing Solution | External Dependency | Setup Complexity | Our Approach |
|-------------------|---------------------|-----------------|-------------|
| Coding-Solo/godot-mcp | Node.js 18+ | Medium | None (native binary) |
| ee0pdt/Godot-MCP | Node.js + npm | Medium | None |
| bradypp/godot-mcp | Node.js | Medium | None |
| Dokujaa/Godot-MCP | Python 3+ | Medium | None |
| slangwald/godot-mcp | Python 3+ | Medium | None |
| tomyud1/godot-mcp | GDScript only | Low | C++ (full API access, better perf) |
| GDAI MCP | Commercial, closed | Low | Open source |

## Sources

### Official / Authoritative
- [godot-cpp GitHub](https://github.com/godotengine/godot-cpp) - v10.0.0-rc1, independent versioning, api_version supports 4.3+
- [Godot 4.4 GDExtension C++ example](https://docs.godotengine.org/en/4.4/tutorials/scripting/gdextension/gdextension_cpp_example.html)
- [godot-cpp-template](https://github.com/godotengine/godot-cpp-template) - Official project scaffolding
- [MCP Specification 2025-03-26 Changelog](https://modelcontextprotocol.io/specification/2025-03-26/changelog)
- [MCP Specification 2025-06-18 Transports](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports)
- [MCP Specification 2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25) - Latest
- [nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0) - April 2025
- [EditorPlugin API](https://docs.godotengine.org/en/stable/classes/class_editorplugin.html)
- [Godot C++ usage guidelines](https://docs.godotengine.org/en/4.4/contributing/development/cpp_usage_guidelines.html) - C++17 standard
- [Godot Thread-safe APIs](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html)

### C++ MCP SDK Evaluation
- [cpp-mcp (hkr04)](https://github.com/hkr04/cpp-mcp) - 234 stars, stuck on 2024-11-05 spec
- [cpp-mcp Issue #10](https://github.com/hkr04/cpp-mcp/issues/10) - Protocol version mismatch, maintainer unavailable since Oct 2025
- [gopher-mcp](https://github.com/GopherSecurity/gopher-mcp) - Enterprise C++ SDK, requires libevent+OpenSSL
- [mcp_server (peppemas)](https://github.com/peppemas/mcp_server) - Plugin architecture, not embeddable

### Existing Godot MCP Implementations
- [Coding-Solo/godot-mcp](https://github.com/Coding-Solo/godot-mcp) - Node.js
- [ee0pdt/Godot-MCP](https://github.com/ee0pdt/Godot-MCP) - Node.js
- [bradypp/godot-mcp](https://github.com/bradypp/godot-mcp) - Node.js
- [tomyud1/godot-mcp](https://github.com/tomyud1/godot-mcp) - GDScript, 32 tools
- [slangwald/godot-mcp](https://github.com/slangwald/godot-mcp) - Python, TCP bridge
- [GDAI MCP](https://gdaimcp.com/) - Commercial

### Build System & Threading
- [SCons vs CMake MSVC issue (godot-cpp #1459)](https://github.com/godotengine/godot-cpp/issues/1459) - /MT vs /MD flag difference
- [GDExtension threading forum discussion](https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547) - C++ native threads work fine
- [MCP upgrade guide 2024-11-05 to 2025-03-26](https://hexdocs.pm/hermes_mcp/0.7.0/protocol_upgrade_2025_03_26.html)
- [MCP release notes (Speakeasy)](https://www.speakeasy.com/mcp/release-notes) - All version summaries
