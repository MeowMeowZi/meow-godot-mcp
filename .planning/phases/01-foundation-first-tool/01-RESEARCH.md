# Phase 1: Foundation & First Tool - Research

**Researched:** 2026-03-16
**Domain:** GDExtension (C++/godot-cpp), MCP protocol (JSON-RPC 2.0), bridge-relay architecture, TCP/stdio transport
**Confidence:** MEDIUM-HIGH

## Summary

Phase 1 must prove the novel two-process bridge-relay architecture end-to-end: a lightweight bridge executable handles stdio for AI clients, relays JSON-RPC messages over TCP localhost to a GDExtension running inside the Godot editor, and a single `get_scene_tree` tool demonstrates round-trip capability. No existing Godot MCP server uses this C++ GDExtension pattern -- all 14+ competitors use Node.js/Python + WebSocket/HTTP -- making this the highest-risk validation phase.

The core technical stack is godot-cpp v10 (targeting Godot 4.3 minimum via `api_version`), nlohmann/json 3.12.0 for JSON parsing (header-only, SCons-friendly), and native C++17 sockets for the bridge executable. The GDExtension side should use Godot's built-in `TCPServer`/`StreamPeerTCP` classes for the TCP listener to avoid cross-platform socket complexity inside the Godot process, while the bridge uses raw platform sockets (or a header-only library like kissnet) since it runs as a standalone executable. MCP protocol implementation targets spec 2025-03-26 for the stdio transport, initialize handshake, tools/list, and tools/call -- the core message types needed for Phase 1.

**Primary recommendation:** Build the GDExtension scaffold and bridge executable as two separate compilation targets sharing a common message protocol. Use Godot's `TCPServer` on the GDExtension side (polled in `_process`) and native C++ sockets in the bridge. Port discovery uses a fixed default port (e.g., 9900) with command-line override on the bridge and project settings override in the GDExtension.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| MCP-01 | Bridge executable receives AI client requests via stdio and relays to GDExtension via TCP localhost | Bridge architecture pattern, stdio transport spec, TCP relay design documented in Architecture Patterns |
| MCP-02 | GDExtension implements JSON-RPC 2.0 protocol for MCP messages | nlohmann/json for parsing, JSON-RPC 2.0 message format, error codes documented in Standard Stack and Code Examples |
| MCP-03 | Support MCP initialize/initialized handshake (spec 2025-03-26) | Complete handshake flow with JSON examples documented in Code Examples |
| MCP-04 | IO thread and Godot main thread communicate via queue+promise pattern | Threading model, std::thread + std::mutex queue, _process polling documented in Architecture Patterns |
| DIST-01 | Package as GDExtension addon (addons/godot-mcp-meow/ directory) | Addon structure, .gdextension format, Asset Library conventions documented in Architecture Patterns |
| SCNE-01 | AI can query current scene tree (node names, types, paths, hierarchy) | EditorInterface::get_edited_scene_root(), recursive traversal, JSON serialization documented in Code Examples |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10 (master) | C++ bindings for GDExtension API | Official Godot C++ bindings; v10 supports Godot 4.3+ via `api_version` parameter |
| nlohmann/json | 3.12.0 | JSON parsing/serialization for MCP messages | De facto C++ JSON library; header-only, C++17 compatible, zero build dependencies |
| SCons | (system) | Build system for GDExtension | godot-cpp officially uses SCons; template and all examples use it |
| GoogleTest | 1.17.x | Unit testing for protocol logic | Industry standard C++ testing; 1.17.x requires C++17 matching our target |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Godot TCPServer | built-in | TCP listener inside GDExtension | GDExtension side -- avoids cross-platform socket code in Godot process |
| Godot StreamPeerTCP | built-in | TCP stream I/O inside GDExtension | Reading/writing JSON-RPC messages from bridge connection |
| std::thread / std::mutex | C++17 stdlib | Bridge executable threading | Bridge needs concurrent stdin reading and TCP I/O |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| nlohmann/json | RapidJSON | RapidJSON is faster but SAX-style API is harder to work with for MCP message construction; nlohmann's intuitive syntax wins for correctness-first |
| Godot TCPServer (GDExtension side) | Raw C++ sockets | Raw sockets would require cross-platform #ifdef in the GDExtension shared library; Godot's built-in TCP classes handle this |
| Raw C++ sockets (bridge) | kissnet (header-only) | kissnet wraps platform sockets nicely for C++17, but adds a dependency; raw sockets are simple enough for a localhost TCP client |
| SCons | CMake | CMake has wider IDE support but godot-cpp officially uses SCons; mixing build systems creates friction |
| cpp-mcp / gopher-mcp | Custom MCP impl | Existing C++ MCP SDKs are either spec-outdated (cpp-mcp targets 2024-11-05) or too heavy (gopher-mcp); custom ~500-800 lines is manageable for the subset we need |

**Installation / Setup:**
```bash
# Clone godot-cpp as submodule
git submodule add -b master https://github.com/godotengine/godot-cpp.git godot-cpp
git submodule update --init --recursive

# Download nlohmann/json single header
mkdir -p thirdparty/nlohmann
curl -L https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp -o thirdparty/nlohmann/json.hpp

# Build GDExtension (targeting Godot 4.3)
scons platform=windows api_version=4.3

# Build bridge executable
scons platform=windows target=bridge
```

## Architecture Patterns

### Recommended Project Structure
```
godot-mcp-meow/
├── godot-cpp/                    # Git submodule (official C++ bindings)
├── thirdparty/
│   └── nlohmann/
│       └── json.hpp              # Single-header JSON library
├── src/
│   ├── register_types.cpp        # GDExtension entry point
│   ├── register_types.h
│   ├── mcp_plugin.h              # EditorPlugin subclass (main entry)
│   ├── mcp_plugin.cpp
│   ├── mcp_server.h              # TCP server, JSON-RPC dispatch
│   ├── mcp_server.cpp
│   ├── mcp_protocol.h            # MCP message types, serialization (shared)
│   ├── mcp_protocol.cpp
│   ├── scene_tools.h             # get_scene_tree tool implementation
│   └── scene_tools.cpp
├── bridge/
│   ├── main.cpp                  # Bridge executable entry point
│   ├── stdio_transport.h         # stdin/stdout JSON-RPC framing
│   ├── stdio_transport.cpp
│   ├── tcp_client.h              # TCP connection to GDExtension
│   └── tcp_client.cpp
├── tests/
│   ├── test_protocol.cpp         # JSON-RPC message parsing tests
│   ├── test_scene_tree.cpp       # Scene tree serialization tests
│   └── CMakeLists.txt            # GoogleTest uses CMake (separate from SCons)
├── project/                      # Test Godot project
│   ├── project.godot
│   └── addons/
│       └── godot_mcp_meow/
│           ├── godot_mcp_meow.gdextension
│           ├── plugin.cfg        # Required for EditorPlugin activation
│           └── bin/              # Compiled binaries placed here
│               ├── libgodot_mcp_meow.windows.editor.x86_64.dll
│               └── godot-mcp-bridge.exe
├── SConstruct                    # Main build file
└── doc_classes/                  # XML class documentation
```

### Pattern 1: Two-Process Bridge-Relay Architecture
**What:** AI client spawns the bridge executable as a subprocess (stdio transport). The bridge connects to the GDExtension's TCP server on localhost. JSON-RPC messages flow: AI Client <--stdio--> Bridge <--TCP--> GDExtension <--Godot API--> Editor.
**When to use:** Always -- this is the fundamental architecture for this project.
**Why necessary:** GDExtension is a shared library loaded by Godot -- it cannot be spawned as a subprocess by AI clients, and it shares Godot's stdout. The bridge solves both problems.

```
[AI Client (Claude Desktop)]
        |  stdio (JSON-RPC, newline-delimited)
        v
[Bridge Executable (~50KB)]
        |  TCP localhost:9900
        v
[GDExtension (inside Godot process)]
        |  Godot API calls (main thread only)
        v
[Godot Editor Scene Tree]
```

**Port Discovery Strategy:**
- Default port: 9900
- Bridge: `--port <N>` command-line argument
- GDExtension: Project Settings `mcp/server/port` (integer, default 9900)
- Multiple editor instances: User must configure different ports manually (v1 limitation)
- Bridge config in `claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "godot": {
      "command": "path/to/godot-mcp-bridge",
      "args": ["--port", "9900"]
    }
  }
}
```

### Pattern 2: IO Thread + Main Thread Queue
**What:** The GDExtension runs a TCP listener. Incoming TCP data is read on the main thread via Godot's `_process()` polling (since `TCPServer` and `StreamPeerTCP` are Godot objects that must be used on the main thread). JSON-RPC parsing and response serialization happen on the main thread. For Phase 1, single-threaded polling is sufficient since operations are fast.
**When to use:** Phase 1 implementation. Later phases may need a true IO thread for long operations.

**Important clarification on threading for Phase 1:**
Godot's `TCPServer` and `StreamPeerTCP` are NOT thread-safe -- they must be polled from the main thread. For Phase 1, where operations (`get_scene_tree`) are fast and non-blocking, polling TCP in `_process()` on the main thread is correct and sufficient. The "IO thread + queue" pattern from MCP-04 becomes necessary in later phases when tool calls involve slow operations (e.g., file I/O, compilation). At that point:
- TCP read/parse stays on main thread (Godot constraint)
- Slow tool execution moves to `std::thread`
- Results return to main thread via `std::mutex`-protected `std::queue`
- Main thread `_process()` drains the result queue and sends TCP responses

```cpp
// Phase 1: Simple main-thread polling in _process()
void MCPPlugin::_process(double delta) {
    // Check for new TCP connections
    if (tcp_server->is_connection_available()) {
        client_peer = tcp_server->take_connection();
    }

    // Read available data from connected client
    if (client_peer.is_valid() && client_peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
        client_peer->poll();
        int available = client_peer->get_available_bytes();
        if (available > 0) {
            PackedByteArray data = client_peer->get_data(available);
            String message = String::utf8((const char*)data.ptr(), data.size());
            // Parse JSON-RPC, dispatch to handler, send response
            String response = handle_jsonrpc_message(message);
            client_peer->put_data(response.to_utf8_buffer());
        }
    }
}
```

### Pattern 3: EditorPlugin Registration (GDExtension C++)
**What:** Register the MCP plugin class at `MODULE_INITIALIZATION_LEVEL_EDITOR` so it runs only in the editor, not in exported games.
**When to use:** Always for editor-only functionality.

```cpp
// register_types.cpp
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include "mcp_plugin.h"

using namespace godot;

void initialize_mcp_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        GDREGISTER_CLASS(MCPPlugin);
    }
}

void uninitialize_mcp_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        // Cleanup
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT mcp_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {

    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_mcp_module);
    init_obj.register_terminator(uninitialize_mcp_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_EDITOR);
    return init_obj.init();
}
}
```

### Pattern 4: plugin.cfg for EditorPlugin Activation
**What:** Even though the C++ class is registered via GDExtension, Godot still needs a `plugin.cfg` to show the plugin in Project Settings > Plugins for user activation. A minimal GDScript wrapper may be needed.
**When to use:** Required for the plugin to appear in the Plugins tab.

**Important note:** There is a known complexity here. GDExtension-based EditorPlugins registered via `GDREGISTER_CLASS` are automatically added to the editor without needing plugin.cfg. However, this means they cannot be disabled by the user. If user-toggleable activation is desired, a thin GDScript wrapper with plugin.cfg is the standard pattern. For Phase 1, automatic registration (no plugin.cfg) is simpler and sufficient -- the plugin starts the TCP server on editor load.

### Anti-Patterns to Avoid
- **Calling Godot API from background threads:** Godot scene tree is NOT thread-safe. All `Node`, `SceneTree`, `EditorInterface` calls must happen on the main thread. The queue pattern exists for this reason.
- **Using Godot's Thread class in GDExtension:** Use `std::thread` instead. Godot's `Thread` class and native C++ threads are not compatible and mixing them can cause crashes.
- **Accessing `get_edited_scene_root()` in `_enter_tree()`:** Returns null because the scene is not yet loaded. Connect to the `scene_changed` signal instead, or check for null and handle gracefully.
- **Writing to stdout from GDExtension:** The GDExtension shares Godot's stdout. Any `print()` or `std::cout` from the GDExtension would corrupt the MCP protocol stream if it were using stdio directly. This is precisely why the bridge exists.
- **Hand-rolling JSON-RPC message framing without newline handling:** MCP stdio messages are newline-delimited. Messages MUST NOT contain embedded newlines. Always serialize JSON without pretty-printing for wire format.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON parsing/serialization | Custom JSON parser | nlohmann/json 3.12.0 | Edge cases in number parsing, Unicode escaping, nested structures are deceptively complex |
| Cross-platform TCP in GDExtension | Raw Berkeley/Winsock sockets | Godot's `TCPServer` + `StreamPeerTCP` | Already cross-platform, integrates with Godot's event loop, no #ifdef needed |
| JSON-RPC 2.0 ID tracking | Ad-hoc request/response matching | Structured dispatcher with ID map | MCP requires correlating responses to requests; IDs can be string or integer per spec |
| MCP capability negotiation | Hardcoded if/else chains | Data-driven capability registry | New capabilities should be addable without modifying negotiation logic |
| Scene tree serialization | Manual string building | Recursive traversal with nlohmann/json object construction | Tree can be deep; manual string concatenation is error-prone |

**Key insight:** The MCP protocol subset needed for Phase 1 is small (initialize, tools/list, tools/call) but getting JSON-RPC 2.0 framing and error codes right is critical for interoperability with Claude Desktop and other AI clients. Use the spec examples as test fixtures.

## Common Pitfalls

### Pitfall 1: Null Scene Root on Plugin Init
**What goes wrong:** `EditorInterface::get_singleton()->get_edited_scene_root()` returns null when called during `_enter_tree()` or `_ready()`.
**Why it happens:** The edited scene has not yet been loaded when the plugin initializes. If Godot is launching with the plugin already enabled, the scene loads after plugin init.
**How to avoid:** Always null-check the scene root before traversal. When the tool `get_scene_tree` is called and no scene is open, return a valid JSON response with an empty tree or an error message -- do NOT crash.
**Warning signs:** Null pointer access crash in the editor on startup.

### Pitfall 2: TCP Message Framing
**What goes wrong:** JSON-RPC messages get split across TCP packets or multiple messages arrive in one packet.
**Why it happens:** TCP is a stream protocol, not a message protocol. A single `recv()` call may return a partial message, or multiple messages concatenated together.
**How to avoid:** Buffer incoming TCP data and split on newline boundaries (MCP stdio framing uses newline-delimited JSON). Accumulate data in a string buffer and process complete lines.
**Warning signs:** JSON parse errors on seemingly valid messages; messages "disappearing" or getting garbled.

### Pitfall 3: Bridge-GDExtension Connection Lifecycle
**What goes wrong:** Bridge starts before Godot, or Godot restarts and the bridge loses connection with no recovery.
**Why it happens:** The bridge and GDExtension are independent processes with no startup ordering guarantee.
**How to avoid:** Bridge should retry TCP connection with exponential backoff. GDExtension should accept new connections gracefully even after a previous client disconnected. Handle `ECONNREFUSED` in the bridge without crashing.
**Warning signs:** "Connection refused" errors; bridge hangs waiting for Godot to start.

### Pitfall 4: Blocking the Godot Main Thread
**What goes wrong:** Editor becomes unresponsive while processing MCP requests.
**Why it happens:** TCP polling or tool execution takes too long in `_process()`.
**How to avoid:** For Phase 1, `get_scene_tree` is fast enough for synchronous execution. But set a maximum processing time per frame (e.g., 5ms) and defer remaining work. For large scene trees, consider chunked serialization.
**Warning signs:** Editor frame rate drops when MCP client is connected.

### Pitfall 5: GDExtension Hot Reload Issues
**What goes wrong:** Recompiling the GDExtension while Godot is running causes crashes or the plugin fails to reload.
**Why it happens:** Godot's hot reload for GDExtensions has known issues, especially with EditorPlugin classes.
**How to avoid:** Set `reloadable = true` in the `.gdextension` file. Close and reopen the project when hot reload fails. During development, expect to restart the editor frequently.
**Warning signs:** Crashes after recompilation; plugin state lost after reload.

### Pitfall 6: MCP Protocol Version Mismatch
**What goes wrong:** Claude Desktop (or other client) expects a different MCP spec version than what the server implements.
**Why it happens:** The spec has evolved through 2024-11-05, 2025-03-26, 2025-06-18, and 2025-11-25 versions. Clients may negotiate different versions.
**How to avoid:** Implement version negotiation in the initialize handshake. The server responds with the version it supports (2025-03-26). If the client requests a version the server does not support, respond with the server's supported version per spec. The client decides whether to proceed or disconnect.
**Warning signs:** Initialize handshake fails; client disconnects immediately after connecting.

## Code Examples

Verified patterns from official sources:

### MCP Initialize Handshake (spec 2025-03-26)
```cpp
// Source: https://modelcontextprotocol.io/specification/2025-03-26/basic/lifecycle

// Step 1: Client sends initialize request (received via bridge stdio -> TCP)
// {
//   "jsonrpc": "2.0",
//   "id": 1,
//   "method": "initialize",
//   "params": {
//     "protocolVersion": "2025-03-26",
//     "capabilities": { "roots": { "listChanged": true }, "sampling": {} },
//     "clientInfo": { "name": "Claude Desktop", "version": "1.0.0" }
//   }
// }

// Step 2: Server responds with capabilities
nlohmann::json create_initialize_response(int id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", {
                {"tools", {{"listChanged", false}}}
            }},
            {"serverInfo", {
                {"name", "godot-mcp-meow"},
                {"version", "0.1.0"}
            }}
        }}
    };
}

// Step 3: Client sends initialized notification (no response needed)
// { "jsonrpc": "2.0", "method": "notifications/initialized" }
```

### MCP tools/list Response
```cpp
// Source: https://modelcontextprotocol.io/specification/2025-03-26/server/tools

nlohmann::json create_tools_list_response(int id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"tools", {
                {
                    {"name", "get_scene_tree"},
                    {"description", "Get the current scene tree structure including node names, types, and paths"},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", nlohmann::json::object()},
                        {"required", nlohmann::json::array()}
                    }}
                }
            }}
        }}
    };
}
```

### MCP tools/call Response (get_scene_tree)
```cpp
// Source: https://modelcontextprotocol.io/specification/2025-03-26/server/tools

nlohmann::json create_tool_result(int id, const nlohmann::json& scene_data) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", {
                {
                    {"type", "text"},
                    {"text", scene_data.dump()}
                }
            }},
            {"isError", false}
        }}
    };
}

// Error case: tool not found
nlohmann::json create_tool_not_found_error(int id, const std::string& tool_name) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", -32602},
            {"message", "Unknown tool: " + tool_name}
        }}
    };
}
```

### JSON-RPC Error Codes
```cpp
// Source: https://www.jsonrpc.org/specification + MCP spec
enum JsonRpcError {
    PARSE_ERROR      = -32700,  // Invalid JSON
    INVALID_REQUEST  = -32600,  // Not a valid JSON-RPC request
    METHOD_NOT_FOUND = -32601,  // Method does not exist
    INVALID_PARAMS   = -32602,  // Invalid method parameters
    INTERNAL_ERROR   = -32603,  // Internal server error
};
```

### Scene Tree Recursive Traversal
```cpp
// Source: Godot 4.3 API - Node::get_child_count(), Node::get_child()
// EditorInterface::get_edited_scene_root()

nlohmann::json serialize_node(Node* node) {
    nlohmann::json result;
    result["name"] = std::string(node->get_name().utf8().get_data());
    result["type"] = std::string(node->get_class().utf8().get_data());
    result["path"] = std::string(node->get_path().operator String().utf8().get_data());

    nlohmann::json children = nlohmann::json::array();
    for (int i = 0; i < node->get_child_count(); i++) {
        children.push_back(serialize_node(node->get_child(i)));
    }
    result["children"] = children;

    return result;
}

nlohmann::json get_scene_tree() {
    Node* root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (root == nullptr) {
        return {{"error", "No scene is currently open in the editor"}};
    }
    return serialize_node(root);
}
```

### .gdextension File Format
```ini
; Source: https://docs.godotengine.org/en/4.3/tutorials/scripting/gdextension/gdextension_cpp_example.html

[configuration]
entry_symbol = "mcp_library_init"
compatibility_minimum = "4.3"
reloadable = true

[libraries]
; Debug builds
linux.debug.x86_64 = "res://addons/godot_mcp_meow/bin/libgodot_mcp_meow.linux.template_debug.x86_64.so"
windows.debug.x86_64 = "res://addons/godot_mcp_meow/bin/libgodot_mcp_meow.windows.template_debug.x86_64.dll"
macos.debug = "res://addons/godot_mcp_meow/bin/libgodot_mcp_meow.macos.template_debug.framework"

; Release builds
linux.release.x86_64 = "res://addons/godot_mcp_meow/bin/libgodot_mcp_meow.linux.template_release.x86_64.so"
windows.release.x86_64 = "res://addons/godot_mcp_meow/bin/libgodot_mcp_meow.windows.template_release.x86_64.dll"
macos.release = "res://addons/godot_mcp_meow/bin/libgodot_mcp_meow.macos.template_release.framework"
```

### Bridge Executable Stdio Transport
```cpp
// Source: MCP spec stdio transport
// https://modelcontextprotocol.io/specification/2025-03-26/basic/transports

// Messages are newline-delimited JSON on stdin/stdout
// Server MUST NOT write non-MCP data to stdout
// Server MAY write logging to stderr

#include <iostream>
#include <string>

// Read one JSON-RPC message from stdin
std::string read_message() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return "";  // EOF -- client closed connection
    }
    return line;
}

// Write one JSON-RPC message to stdout
void write_message(const std::string& json) {
    std::cout << json << "\n" << std::flush;
}

// Log to stderr (safe -- does not interfere with MCP protocol)
void log_debug(const std::string& msg) {
    std::cerr << "[godot-mcp-bridge] " << msg << "\n" << std::flush;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| godot-cpp branch-per-Godot-version (4.2, 4.3, 4.4) | godot-cpp v10 independent versioning with `api_version` | 2025 (v10 release) | Single build can target Godot 4.3+ with forward compatibility |
| MCP spec 2024-11-05 (HTTP+SSE) | MCP spec 2025-03-26 (Streamable HTTP, tool annotations) | March 2025 | stdio transport unchanged; tool annotations available but optional |
| MCP spec 2025-03-26 | MCP spec 2025-11-25 (Tasks, Extensions, enhanced OAuth) | November 2025 | Latest spec version; stdio transport and core tools/list, tools/call unchanged; new features (Tasks, Elicitation) are additive and optional |
| Node.js/Python Godot MCP servers | Still dominant pattern (14+ implementations) | 2024-2026 | Zero C++ GDExtension implementations exist; this project is novel |

**Deprecated/outdated:**
- MCP spec 2024-11-05: HTTP+SSE transport replaced by Streamable HTTP. Stdio transport unchanged.
- JSON-RPC batching: Added in 2025-03-26, removed in 2025-06-18. Do NOT implement batching.
- godot-cpp branch-per-version: Still works but v10 independent versioning is the recommended path forward.

**MCP spec version note:** The project targets 2025-03-26. The latest spec is 2025-11-25 which adds Tasks, Extensions, and enhanced OAuth -- all optional features not needed for Phase 1. The core stdio transport, initialize handshake, tools/list, and tools/call are unchanged across all versions. Targeting 2025-03-26 is safe and forward-compatible.

## Open Questions

1. **plugin.cfg vs automatic registration**
   - What we know: GDExtension EditorPlugin classes registered via `GDREGISTER_CLASS` at `MODULE_INITIALIZATION_LEVEL_EDITOR` are automatically added to the editor. Alternatively, a `plugin.cfg` with a GDScript wrapper allows user-toggleable activation.
   - What's unclear: Whether automatic registration (always-on) is acceptable for Phase 1, or if users expect to enable/disable the MCP server from Project Settings > Plugins.
   - Recommendation: Start with automatic registration (no plugin.cfg) for Phase 1 simplicity. The TCP server starts on editor load. Add plugin.cfg toggle in Phase 4 (Editor Integration) when the dock panel provides start/stop controls.

2. **Bridge executable distribution**
   - What we know: The bridge must be a standalone executable that AI clients can spawn. It needs to be in a known location for claude_desktop_config.json.
   - What's unclear: Should the bridge be compiled into the `addons/godot_mcp_meow/bin/` directory alongside the GDExtension library? Or should it be a separate installation?
   - Recommendation: Place the bridge executable in `addons/godot_mcp_meow/bin/` alongside the shared library. Users reference the full path in their AI client config. This keeps everything in one addon directory.

3. **Multiple editor instance support**
   - What we know: Each Godot editor instance needs a unique TCP port. The default is 9900.
   - What's unclear: No automatic port discovery mechanism has been designed. Users must manually configure different ports.
   - Recommendation: v1 limitation -- document that users must set different ports for multiple instances. Consider auto-port-selection (bind to port 0, write chosen port to a file) in a future phase.

4. **TCP message framing between bridge and GDExtension**
   - What we know: MCP stdio transport uses newline-delimited JSON. The bridge-to-GDExtension TCP connection also needs a framing protocol.
   - What's unclear: Should the same newline-delimited framing be used on the TCP side, or should we use a length-prefixed protocol?
   - Recommendation: Use newline-delimited JSON on TCP as well (same framing as stdio). This makes the bridge a thin pass-through relay rather than a protocol translator. Simpler implementation, easier debugging.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.x |
| Config file | tests/CMakeLists.txt (separate from SCons build) |
| Quick run command | `cd tests && cmake --build build && ctest --output-on-failure` |
| Full suite command | `cd tests && cmake --build build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| MCP-01 | Bridge relays stdin to TCP and TCP to stdout | integration | Manual (requires bridge + GDExtension running) | -- Wave 0 |
| MCP-02 | JSON-RPC 2.0 parsing, dispatch, error responses | unit | `ctest -R test_protocol` | -- Wave 0 |
| MCP-03 | Initialize handshake produces correct JSON | unit | `ctest -R test_protocol` | -- Wave 0 |
| MCP-04 | Queue drains correctly between threads | unit | `ctest -R test_threading` | -- Wave 0 |
| DIST-01 | Addon directory structure valid | manual-only | Visual inspection of directory layout | N/A |
| SCNE-01 | Scene tree serialization produces correct JSON | unit | `ctest -R test_scene_tree` | -- Wave 0 |

**Manual-only justification:** MCP-01 (integration) requires running both processes and an actual TCP connection. DIST-01 is a structural/packaging concern verified by inspection.

### Sampling Rate
- **Per task commit:** `cd tests && cmake --build build && ctest --output-on-failure`
- **Per wave merge:** Full suite + manual integration test (bridge + Godot editor)
- **Phase gate:** All unit tests green + successful end-to-end demo (Claude Desktop calls get_scene_tree)

### Wave 0 Gaps
- [ ] `tests/CMakeLists.txt` -- GoogleTest setup with FetchContent
- [ ] `tests/test_protocol.cpp` -- JSON-RPC parsing, initialize handshake, tools/list, tools/call, error codes
- [ ] `tests/test_scene_tree.cpp` -- Scene tree JSON serialization format
- [ ] `tests/test_threading.cpp` -- Thread-safe queue push/drain (if IO thread pattern used)

## Sources

### Primary (HIGH confidence)
- [MCP Specification 2025-03-26 - Lifecycle](https://modelcontextprotocol.io/specification/2025-03-26/basic/lifecycle) - Initialize handshake, version negotiation, shutdown
- [MCP Specification 2025-03-26 - Transports](https://modelcontextprotocol.io/specification/2025-03-26/basic/transports) - Stdio transport framing, message format
- [MCP Specification 2025-03-26 - Tools](https://modelcontextprotocol.io/specification/2025-03-26/server/tools) - tools/list, tools/call, error handling
- [godot-cpp GitHub](https://github.com/godotengine/godot-cpp) - v10 versioning, api_version parameter, build setup
- [GDExtension C++ Example (Godot 4.3)](https://docs.godotengine.org/en/4.3/tutorials/scripting/gdextension/gdextension_cpp_example.html) - register_types.cpp, .gdextension file, SConstruct
- [Godot TCPServer docs](https://docs.godotengine.org/en/stable/classes/class_tcpserver.html) - listen(), take_connection() API
- [Godot Threading docs](https://docs.godotengine.org/en/stable/tutorials/performance/using_multiple_threads.html) - Thread safety rules
- [nlohmann/json GitHub](https://github.com/nlohmann/json) - Version 3.12.0, header-only integration

### Secondary (MEDIUM confidence)
- [godot-cpp-template](https://github.com/godotengine/godot-cpp-template) - Project structure, SConstruct boilerplate
- [nathanfranke/gdextension template](https://github.com/nathanfranke/gdextension) - Asset Library addon structure conventions
- [Godot Forum: EditorPlugin scene root](https://forum.godotengine.org/t/how-to-get-edited-scene-tree-in-editorplugin/47478) - get_edited_scene_root() null issue
- [Godot Forum: Thread in GDExtension](https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547) - std::thread recommendation
- [MCP Specification 2025-11-25 changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog) - Latest spec version context
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification) - Error codes, message format

### Tertiary (LOW confidence)
- [godot-cpp issue #1549](https://github.com/godotengine/godot-cpp/issues/1549) - C++ EditorPlugin add_tool_menu_item issues (suggests potential EditorPlugin gotchas)
- [Godot issue #98752](https://github.com/godotengine/godot/issues/98752) - _exit_tree not called properly in editor mode (Godot 4.3)
- [kissnet](https://github.com/Ybalrid/kissnet) - Header-only C++17 socket library (alternative for bridge TCP)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - godot-cpp v10, nlohmann/json, SCons are all well-documented and widely used
- Architecture (bridge-relay): MEDIUM-HIGH - Pattern is proven by Node.js/Python implementations, but C++ GDExtension variant is novel
- MCP protocol: HIGH - Official spec with complete JSON examples for all needed operations
- Threading model: MEDIUM - Godot threading constraints well-documented, but GDExtension-specific edge cases exist
- Pitfalls: HIGH - Documented through official Godot issues and community reports

**Research date:** 2026-03-16
**Valid until:** 2026-04-16 (30 days - stable technologies, MCP spec unlikely to change in this window)
