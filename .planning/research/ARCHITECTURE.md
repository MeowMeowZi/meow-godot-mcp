# Architecture Patterns

**Domain:** Godot GDExtension MCP Server Plugin (C++)
**Researched:** 2026-03-14

## Overview

This project is architecturally novel: it is an MCP server whose protocol logic and Godot API access both live **inside** the Godot editor process as a GDExtension. All existing Godot MCP implementations (7+ found) use a three-layer architecture: `AI Client <--stdio--> Node.js/Python process <--WebSocket--> GDScript plugin`. This project eliminates the middle layer entirely.

The core architectural challenge is twofold:

1. **Subprocess problem:** MCP's stdio transport requires the AI client to launch the MCP server as a subprocess. A GDExtension is a shared library loaded into Godot -- it is not independently launchable. A lightweight bridge executable is needed to relay stdio to the GDExtension.

2. **Threading problem:** The bridge relay and MCP protocol run on a background thread, but Godot's scene tree APIs must be called from the main thread. The architecture must safely marshal between these worlds.

## Recommended Architecture

### High-Level Component Diagram

```
Claude Desktop / AI Client
        |
        | (launches as subprocess, stdio pipes)
        |
+-------v-----------------------+
|  Bridge Executable (~50KB)     |
|  - Reads stdin, writes stdout  |
|  - Relays JSON-RPC over TCP    |
|    to localhost:PORT            |
+-------+------+----------------+
        |      ^
   TCP send  TCP recv
        |      |
        v      |
+-------v------+---------------------------------------+
|  Godot Editor Process (with GDExtension loaded)       |
|                                                       |
|  +--------------------------------------------------+ |
|  |  McpMeowPlugin (EditorPlugin)                    | |
|  |  - Owns all components                           | |
|  |  - Lifecycle management (_enter_tree/_exit_tree)  | |
|  |  - UI dock panel (status display, controls)      | |
|  +------+------------------------------------+------+ |
|         |                                    |        |
|         v                                    v        |
|  +------------------+          +--------------------+ |
|  | TcpTransport     |          | ToolRegistry       | |
|  | (background      |          | - Registers MCP    | |
|  |  thread)          |          |   tools            | |
|  | - TCP accept/read |          | - Maps tool name   | |
|  | - Receives JSON   |          |   to handler       | |
|  | - Sends responses |          +--------+-----------+ |
|  +--------+----------+                   |            |
|           |                              |            |
|           v                              v            |
|  +--------------------------------------------------+ |
|  | McpProtocol (JSON-RPC 2.0 engine)                | |
|  | - Parses JSON-RPC requests/notifications         | |
|  | - MCP lifecycle (initialize -> ready -> shutdown) | |
|  | - Capability negotiation                         | |
|  | - Routes tools/list, tools/call, etc.            | |
|  | - Serializes responses                           | |
|  +------------------------+-------------------------+ |
|                            |                          |
|                            v                          |
|  +--------------------------------------------------+ |
|  | GodotBridge (thread-safe Godot API access)       | |
|  | - Queues operations for main thread              | |
|  | - Uses call_deferred / callable_mp               | |
|  | - Reads scene tree, node properties              | |
|  | - Creates/modifies/deletes nodes                 | |
|  | - Accesses EditorInterface                       | |
|  +--------------------------------------------------+ |
+-------------------------------------------------------+
```

### Why a Bridge Executable?

MCP's stdio spec says: "The client launches the MCP server as a subprocess." A GDExtension is a `.dll`/`.so`/`.dylib` loaded into Godot's process -- it cannot be that subprocess. Two options exist:

**Option A: Bridge Executable (Recommended)**
A tiny native binary (~100 lines, ~50KB) that the AI client spawns. It relays stdio to the GDExtension over TCP localhost. The GDExtension holds ALL protocol logic. The bridge is a dumb pipe relay.

- Advantage: Clean separation. No stdout contamination from Godot. Bridge is trivial to implement and debug.
- Advantage: Godot can be running before or after the AI client connects.
- Advantage: Bridge can reconnect if Godot restarts.
- Tradeoff: Two executables to distribute (bridge binary + GDExtension).

**Option B: Godot-as-subprocess (Alternative)**
The AI client launches Godot itself as the subprocess. The GDExtension reads/writes Godot's stdin/stdout directly.

- Advantage: Single process, no bridge needed.
- Risk: Godot prints engine messages, warnings, and GDScript `print()` output to stdout. ANY non-JSON-RPC line on stdout breaks the MCP protocol. Suppressing all Godot stdout is extremely difficult and fragile.
- Risk: Godot startup is slow (splash screen, editor init). MCP client may time out.
- Risk: If Godot crashes, MCP client loses connection with no recovery.

**Recommendation:** Option A (bridge executable) is architecturally cleaner and avoids the stdout contamination problem entirely. The bridge is small enough to compile alongside the GDExtension in the same SConstruct.

### Component Boundaries

| Component | Responsibility | Communicates With | Thread |
|-----------|---------------|-------------------|--------|
| **Bridge executable** | Stdio relay: stdin/stdout <-> TCP localhost | AI Client (stdio), GDExtension (TCP) | Own process |
| **McpMeowPlugin** | EditorPlugin lifecycle, UI panel, owns all sub-components | TcpTransport, ToolRegistry, GodotBridge | Main |
| **TcpTransport** | TCP accept/read/write on localhost port, receives/sends JSON-RPC | McpProtocol | Background (IO thread) |
| **McpProtocol** | JSON-RPC 2.0 parsing, MCP lifecycle (initialize, capability negotiation), request routing | TcpTransport, ToolRegistry | Background (IO thread) |
| **ToolRegistry** | Registers MCP tools, maps tool names to handler functions, provides tools/list response | McpProtocol, GodotBridge | Shared (registration on main, lookup on IO) |
| **GodotBridge** | Thread-safe interface to Godot editor APIs, queues scene tree operations for main thread | ToolRegistry handlers, Godot Engine | Both (called from IO, defers to main) |

### Data Flow

#### Request Flow (AI Client sends a tool call)

```
1. AI Client writes JSON-RPC to bridge's stdin
        |
2. Bridge forwards over TCP to GDExtension
        |
3. TcpTransport (IO thread) reads message from TCP socket
        |
4. McpProtocol (IO thread) parses JSON-RPC, identifies method
        |
5. If tools/call: ToolRegistry looks up handler by tool name
        |
6. Handler calls GodotBridge method
        |
7. GodotBridge queues operation for main thread via call_deferred
        |  (IO thread blocks on std::future waiting for result)
        |
8. Main thread executes Godot API call (e.g., get scene tree)
        |  (Main thread fulfills std::promise with result)
        |
9. GodotBridge returns result to handler (IO thread resumes)
        |
10. Handler constructs MCP tool result (content array)
         |
11. McpProtocol serializes JSON-RPC response
         |
12. TcpTransport sends response over TCP to bridge
         |
13. Bridge writes response to stdout for AI client
```

#### Lifecycle Flow

```
1. User opens Godot project with GDExtension installed
        |
2. Godot loads .gdextension, calls entry point at EDITOR level
        |
3. McpMeowPlugin::_enter_tree() fires
        |  - Creates TcpTransport (starts listening on localhost:PORT)
        |  - Creates McpProtocol
        |  - Registers all tools via ToolRegistry
        |  - Creates GodotBridge
        |  - Starts IO thread for TCP accept
        |
4. AI Client launches bridge executable
        |  - Bridge connects to GDExtension's TCP port
        |
5. AI Client sends "initialize" through bridge
        |
6. McpProtocol responds with server info + capabilities
        |
7. AI Client sends "initialized" notification
        |
8. Normal tool calling begins
```

## Key Architecture Decisions

### Decision 1: In-Process Protocol Logic with External Bridge Relay

**What:** All MCP protocol logic (JSON-RPC parsing, tool dispatch, Godot API access) lives in the GDExtension. A tiny bridge executable handles only stdio relay to TCP localhost.

**Why:** Eliminates the Node.js/Python dependency entirely. The bridge is a compiled native binary (~50KB, zero dependencies). The GDExtension has direct C++ access to the Godot API. No WebSocket library needed -- plain TCP on localhost is sufficient for same-machine JSON message relay.

**Tradeoff:** Two artifacts to distribute (bridge + GDExtension shared library). But both are compiled native binaries, no runtime to install.

### Decision 2: TCP Localhost for Bridge-to-GDExtension IPC

**What:** The bridge communicates with the GDExtension over TCP on localhost (127.0.0.1).

**Why:** Cross-platform consistent (Winsock2 on Windows, POSIX sockets on Linux/macOS). More debuggable than named pipes (can use telnet/nc for manual testing). No library dependency -- platform socket APIs are sufficient. Well-understood, predictable behavior.

**Alternatives considered:**
- Named pipes: Different APIs per platform (CreateNamedPipe vs mkfifo), harder to debug.
- Unix domain sockets: Not available on Windows without WSL.
- Shared memory: Overkill for a message-based protocol.

### Decision 3: Background IO Thread + Main Thread Bridge

**What:** A dedicated `std::thread` handles TCP accept/read/write. Godot API calls are marshalled to the main thread via `call_deferred` with `std::promise`/`std::future` synchronization.

**Why:** Godot's scene tree is NOT thread-safe. Godot 4.1+ enforces thread guards that crash on violations. TCP reads block. These constraints mandate a two-thread architecture. Using `std::thread` directly is more reliable in GDExtension than Godot's Thread class (confirmed by community experience).

**Pattern:**
```cpp
// IO thread context:
json handle_tool_call(const std::string& tool_name, const json& params) {
    // Create promise/future pair for cross-thread result
    auto promise = std::make_shared<std::promise<json>>();
    auto future = promise->get_future();

    // Queue work for main thread
    godot_bridge->queue_operation([=]() -> json {
        // This lambda runs on main thread
        return execute_tool(tool_name, params);
    }, promise);

    // Block IO thread until main thread completes
    return future.get();
}
```

### Decision 4: nlohmann/json for JSON-RPC

**What:** Use the nlohmann/json header-only library for all JSON parsing and serialization.

**Why:** Industry standard C++ JSON library (header-only, zero build complexity). Used by cpp-mcp and TinyMCP reference implementations. Integrates trivially with SCons -- just add the include path. Avoids Godot's Dictionary/Variant for protocol-level parsing where strict JSON-RPC 2.0 compliance matters. Single vendored header file (`json.hpp`), no package manager needed.

### Decision 5: EditorPlugin at EDITOR Initialization Level

**What:** Register the plugin class at `MODULE_INITIALIZATION_LEVEL_EDITOR`.

**Why:** Required for any GDExtension extending EditorPlugin. The class must be registered after editor classes are available. Using SCENE level crashes because EditorPlugin does not exist at that point. This is the most common registration mistake in GDExtension EditorPlugin development.

### Decision 6: Custom MCP Protocol Implementation

**What:** Implement the MCP protocol layer from scratch (~500-800 lines) rather than embedding an existing C++ MCP SDK.

**Why:** Three C++ MCP SDKs exist, none are suitable: cpp-mcp is stuck on spec 2024-11-05 (causes version errors with modern clients), gopher-mcp requires libevent+OpenSSL (enterprise overkill), mcp_server is a standalone server design that cannot be embedded. The MCP stdio protocol is straightforward: newline-delimited JSON-RPC 2.0 with a defined lifecycle (initialize, tools, resources). Full control over spec version negotiation. Target MCP spec 2025-03-26 for v1.

## Component Details

### Bridge Executable

A minimal native binary (~100 lines of C++) that relays between stdio and TCP.

**Responsibilities:**
- Read lines from stdin, forward over TCP to GDExtension
- Read lines from TCP socket, write to stdout
- Exit cleanly when stdin closes (EOF) or TCP connection drops
- No protocol awareness -- pure byte relay

**Key design:**
```cpp
// bridge/main.cpp (simplified)
int main(int argc, char* argv[]) {
    int port = parse_port(argc, argv);
    int sock = tcp_connect("127.0.0.1", port);

    // Relay thread: TCP -> stdout
    std::thread relay([&]() {
        char buf[8192];
        while (auto n = recv(sock, buf, sizeof(buf), 0) > 0) {
            fwrite(buf, 1, n, stdout);
            fflush(stdout);
        }
    });

    // Main: stdin -> TCP
    std::string line;
    while (std::getline(std::cin, line)) {
        line += "\n";
        send(sock, line.c_str(), line.size(), 0);
    }

    close(sock);
    relay.join();
    return 0;
}
```

**Claude Desktop configuration:**
```json
{
  "mcpServers": {
    "godot-meow": {
      "command": "path/to/godot-mcp-bridge",
      "args": ["--port", "6680"]
    }
  }
}
```

### McpMeowPlugin (EditorPlugin)

The root component. Extends `godot::EditorPlugin` via `GDCLASS`. Registered at `MODULE_INITIALIZATION_LEVEL_EDITOR`.

**Responsibilities:**
- Create and own all sub-components in `_enter_tree()`
- Destroy and clean up in `_exit_tree()`
- Add a dock panel showing MCP server status (listening/connected/disconnected, request count, last tool called)
- Provide enable/disable toggle for the MCP server
- Listen for `_edited_scene_changed()` to track scene switches
- Process pending GodotBridge operations in `_process()` or via deferred calls

**Key API surface:**
```cpp
class McpMeowPlugin : public EditorPlugin {
    GDCLASS(McpMeowPlugin, EditorPlugin)

    TcpTransport* transport;
    McpProtocol* protocol;
    ToolRegistry* registry;
    GodotBridge* bridge;
    Control* status_panel;

    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;
    void _edited_scene_changed(Node* scene_root);

    void start_server();
    void stop_server();
};
```

### TcpTransport

TCP listener/handler on localhost. Runs on a background thread.

**Responsibilities:**
- Listen on a configurable localhost port (default: 6680)
- Accept one client connection (single-client, matching MCP stdio semantics)
- Read newline-delimited JSON messages from the TCP socket
- Write newline-delimited JSON responses to the TCP socket
- Signal disconnect/reconnect events
- Provide thread-safe send interface

**Key design:**
```cpp
class TcpTransport {
    std::thread io_thread;
    std::mutex write_mutex;
    std::atomic<bool> running;
    int server_socket;
    int client_socket;
    int port;

    // Callback for received messages
    std::function<void(const std::string&)> on_message;
    std::function<void()> on_disconnect;

    void start(int port);
    void stop();
    void send(const std::string& json_line);

private:
    void io_loop();  // Accept + read loop on background thread
};
```

### McpProtocol

MCP protocol state machine. Parses JSON-RPC 2.0, manages MCP lifecycle.

**Responsibilities:**
- Parse incoming JSON-RPC 2.0 messages (requests, notifications)
- Manage MCP lifecycle state: `uninitialized -> initializing -> ready -> shutdown`
- Handle `initialize` request: return server info, protocol version (`2025-03-26`), capabilities
- Handle `initialized` notification: transition to ready state
- Route `tools/list` to ToolRegistry
- Route `tools/call` to appropriate tool handler
- Handle `resources/list`, `resources/read` if resources are exposed
- Serialize JSON-RPC 2.0 responses (result or error)
- Echo back request IDs correctly

**Capabilities to advertise:**
```json
{
  "capabilities": {
    "tools": { "listChanged": true },
    "resources": { "subscribe": false, "listChanged": true }
  }
}
```

### ToolRegistry

Maps MCP tool names to handler callables. Pure data structure with thread-safe read access.

**Responsibilities:**
- Register tools at startup (name, description, input schema, handler)
- Provide `tools/list` response payload
- Look up handler by tool name for `tools/call`
- Support dynamic tool registration/unregistration (for version-adaptive features)
- Thread-safe: tools registered on main thread at init, looked up on IO thread during operation

**Tool categories for this project:**

| Category | Tools | Priority |
|----------|-------|----------|
| Scene Query | `get_scene_tree`, `get_node_properties`, `get_node_by_path` | Phase 1 (MVP) |
| Node CRUD | `create_node`, `set_node_property`, `delete_node`, `rename_node` | Phase 1 (MVP) |
| Script Mgmt | `get_script`, `set_script`, `create_script` | Phase 2 |
| Project Info | `get_project_info`, `list_files`, `get_project_settings` | Phase 2 |
| Resource Mgmt | `list_resources`, `get_resource_info` | Phase 2 |
| Run/Debug | `run_project`, `stop_project`, `get_debug_output` | Phase 3 |
| Signal Mgmt | `list_signals`, `connect_signal`, `disconnect_signal` | Phase 3 |
| Export | `export_project`, `list_export_presets` | Phase 4 |

### GodotBridge

The critical bridge component. Translates tool handler requests into Godot API calls, marshalling between threads.

**Responsibilities:**
- Provide thread-safe wrappers for Godot editor operations
- Queue operations for main thread execution via a pending operations queue
- Wait for main thread completion and return results via promise/future
- Access EditorInterface singleton for editor-specific operations
- Handle error cases (null scene root, invalid node paths, missing nodes, etc.)

**Thread bridge pattern:**
```cpp
class GodotBridge : public Object {
    GDCLASS(GodotBridge, Object)

public:
    // Called from IO thread, blocks until main thread completes
    json get_scene_tree();
    json get_node_properties(const std::string& node_path);
    json create_node(const std::string& parent_path,
                     const std::string& type,
                     const std::string& name);
    json set_node_property(const std::string& path,
                           const std::string& property,
                           const json& value);
    json delete_node(const std::string& path);

    // Called from main thread (_process) to drain the queue
    void process_pending();

    // Generic queue method: callable runs on main thread, result returned to caller
    template<typename F>
    json run_on_main_thread(F&& operation);

private:
    struct PendingOperation {
        std::function<json()> operation;
        std::shared_ptr<std::promise<json>> promise;
    };

    std::queue<PendingOperation> pending_ops;
    std::mutex ops_mutex;
};
```

**Key insight:** `call_deferred()` cannot return values. The GodotBridge uses a `std::queue` of pending operations that the main thread drains in `_process()`. Each operation has an associated `std::promise` that the main thread fulfills, allowing the IO thread to block on `std::future::get()` until the result is available. Since MCP is sequential (one request at a time), having only one operation in flight is perfectly adequate.

## Patterns to Follow

### Pattern 1: Request-Response with Thread Marshalling

**What:** Every MCP tool call follows a strict request-response pattern where the IO thread receives the request, marshals execution to the main thread, waits for the result, and sends the response.

**When:** Every tools/call invocation.

**Why:** MCP is inherently synchronous (request-response). Godot scene tree is main-thread-only. The IO thread must block while the main thread processes.

**Example:**
```cpp
// IO thread receives tools/call for "get_scene_tree"
json handle_get_scene_tree(const json& params) {
    return bridge->run_on_main_thread([&]() -> json {
        Node* root = EditorInterface::get_singleton()->get_edited_scene_root();
        if (!root) {
            return {{"error", "No scene is currently open"}};
        }
        return serialize_node_tree(root);
    });
}
```

### Pattern 2: Node Tree Serialization

**What:** Recursively serialize a Godot node tree into JSON for MCP responses.

**When:** `get_scene_tree` tool, scene queries.

**Example:**
```cpp
json serialize_node(Node* node, int max_depth = 10, int depth = 0) {
    json j;
    j["name"] = node->get_name().utf8().get_data();
    j["type"] = node->get_class().utf8().get_data();
    j["path"] = String(node->get_path()).utf8().get_data();

    if (depth < max_depth) {
        json children = json::array();
        for (int i = 0; i < node->get_child_count(); i++) {
            children.push_back(
                serialize_node(node->get_child(i), max_depth, depth + 1));
        }
        j["children"] = children;
    } else {
        j["children_truncated"] = node->get_child_count();
    }

    return j;
}
```

### Pattern 3: MCP Tool Definition with JSON Schema

**What:** Each tool is defined with a name, description, and JSON Schema for its input parameters, following the MCP specification exactly.

**When:** Tool registration, tools/list responses.

**Example:**
```cpp
json define_create_node_tool() {
    return {
        {"name", "create_node"},
        {"description", "Create a new node in the scene tree"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"parent_path", {
                    {"type", "string"},
                    {"description", "NodePath to the parent (e.g. '/root/Main')"}
                }},
                {"node_type", {
                    {"type", "string"},
                    {"description", "Godot node class (e.g. 'Node2D', 'Sprite2D')"}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Name for the new node"}
                }}
            }},
            {"required", {"parent_path", "node_type", "name"}}
        }}
    };
}
```

### Pattern 4: Graceful Error Responses

**What:** Tool handlers always return well-formed MCP responses, even on error. Never crash, never return malformed JSON.

**When:** Every tool handler, especially for user-facing edge cases.

**Example:**
```cpp
json handle_get_node_properties(const json& params) {
    if (!params.contains("node_path")) {
        return make_error_response("Missing required parameter: node_path");
    }

    return bridge->run_on_main_thread([&]() -> json {
        Node* root = EditorInterface::get_singleton()->get_edited_scene_root();
        if (!root) {
            return make_tool_result("No scene is currently open in the editor.");
        }

        String path = String(params["node_path"].get<std::string>().c_str());
        Node* node = root->get_node_or_null(NodePath(path));
        if (!node) {
            return make_tool_result("Node not found at path: " + path);
        }

        return serialize_node_properties(node);
    });
}
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Accessing Scene Tree from IO Thread

**What:** Calling `get_child()`, `get_node()`, `set()`, `add_child()` etc. directly from the background IO thread.

**Why bad:** Godot 4.1+ has thread guards that crash with "This function can only be accessed from either the main thread or a thread group." Even in older versions, this causes data races.

**Instead:** Always use GodotBridge to marshal operations to the main thread.

### Anti-Pattern 2: Using Godot Variant/Dictionary for JSON-RPC

**What:** Parsing MCP JSON-RPC messages using Godot's built-in `JSON::parse()` and `Dictionary` types.

**Why bad:** Godot's JSON parser returns Variant types that need constant casting. JSON-RPC 2.0 has strict requirements (integer IDs, exact field names) that are cumbersome with Variant. The protocol layer should be Godot-independent for testability (can unit test without running Godot).

**Instead:** Use nlohmann/json for all protocol-level JSON handling. Convert to/from Godot types only at the GodotBridge boundary.

### Anti-Pattern 3: Monolithic Tool Handlers

**What:** Putting all tool logic in a single giant switch statement or if-else chain.

**Why bad:** Unmaintainable as tool count grows (30+ tools planned). Hard to test individual tools.

**Instead:** Use the ToolRegistry pattern. Group related tools by file (scene_tools.cpp, node_tools.cpp). Each tool handler is a standalone function.

### Anti-Pattern 4: Hardcoding Godot Version Assumptions

**What:** Calling APIs that only exist in specific Godot versions without checking.

**Why bad:** The project targets Godot 4.3+ (godot-cpp v10 minimum). APIs may differ between 4.3, 4.4, 4.5. Calling a non-existent method crashes the editor.

**Instead:** Use `compatibility_minimum = "4.3"` in `.gdextension`. For version-specific features, check at runtime using `Engine::get_singleton()->get_version_info()` and conditionally enable tools.

### Anti-Pattern 5: Blocking Main Thread

**What:** Performing expensive operations synchronously in `_process()` or deferred calls.

**Why bad:** Godot's main thread drives the editor UI. Blocking it freezes the editor.

**Instead:** Keep main-thread operations fast (scene tree reads are fast). For expensive operations (large file reads, deep recursion), consider chunking or setting depth limits.

## Project File Structure

```
godot-mcp-meow/
|
+-- src/                              # GDExtension C++ source code
|   +-- register_types.h
|   +-- register_types.cpp            # Class registration (EDITOR level)
|   +-- mcp/
|   |   +-- protocol.h                # MCP protocol (JSON-RPC 2.0 + lifecycle)
|   |   +-- protocol.cpp
|   |   +-- server.h                  # MCP server (tool registry, request dispatch)
|   |   +-- server.cpp
|   |   +-- transport.h               # TCP listener for bridge connection
|   |   +-- transport.cpp
|   +-- editor/
|   |   +-- mcp_plugin.h              # EditorPlugin subclass (dock UI, lifecycle)
|   |   +-- mcp_plugin.cpp
|   +-- tools/
|   |   +-- scene_tools.h             # Scene query/manipulation tools
|   |   +-- scene_tools.cpp
|   |   +-- node_tools.h              # Node CRUD tools
|   |   +-- node_tools.cpp
|   |   +-- script_tools.h            # Script management tools
|   |   +-- script_tools.cpp
|   |   +-- project_tools.h           # Project info tools
|   |   +-- project_tools.cpp
|   +-- bridge/
|       +-- godot_bridge.h            # Thread-safe Godot API wrapper
|       +-- godot_bridge.cpp
|
+-- bridge/
|   +-- main.cpp                      # Bridge executable: stdio <-> TCP relay
|
+-- thirdparty/
|   +-- nlohmann/
|       +-- json.hpp                  # Single-header JSON library (v3.12.0)
|
+-- godot-cpp/                        # Git submodule (official C++ bindings, v10+)
|
+-- project/                          # Demo/test Godot project
|   +-- project.godot
|   +-- addons/
|   |   +-- godot_mcp_meow/
|   |       +-- plugin.cfg            # Editor plugin config
|   |       +-- bin/
|   |           +-- godot_mcp_meow.gdextension
|
+-- tests/
|   +-- test_protocol.cpp             # Unit tests: JSON-RPC parsing
|   +-- test_tools.cpp                # Unit tests: tool argument validation
|
+-- SConstruct                        # Build: GDExtension + bridge executable
```

## Scalability Considerations

| Concern | At MVP (5 tools) | At Feature Complete (30+ tools) | Future (community extensions) |
|---------|-------------------|-------------------------------|-------------------------------|
| Tool dispatch | Simple map lookup, O(1) | Same map lookup, O(1) | Consider plugin-based tool loading |
| JSON parsing | nlohmann/json sufficient | Same | Same |
| Thread model | 1 IO thread + main thread | Same | Same (MCP is sequential per connection) |
| Memory | Minimal overhead | Cached scene tree snapshots may grow for large scenes | Depth limits, pagination |
| TCP connections | Single client | Single client | Could support multiple clients with connection pool |
| Startup time | Instant | Tool registration loop, still fast | Lazy tool registration if needed |

## Suggested Build Order (Phase Dependencies)

```
Phase 1: Foundation
  register_types.cpp + McpMeowPlugin (empty EditorPlugin shell)
  --> Bridge executable (stdio <-> TCP relay, test independently)
  --> TcpTransport (TCP listener, verify bridge can connect)
  --> McpProtocol (initialize/initialized handshake only)
  --> Verify: AI client can connect via bridge and see server capabilities

Phase 2: Thread Bridge + First Tool
  --> GodotBridge (thread marshalling infrastructure)
  --> ToolRegistry + tools/list, tools/call routing
  --> scene_tools: get_scene_tree (first real tool, proves entire pipeline)
  --> Verify: AI can query scene tree of open project

Phase 3: Core CRUD
  --> node_tools: create_node, set_node_property, delete_node
  --> Richer scene_tools: get_node_properties, get_node_by_path
  --> Verify: AI can read AND write to the scene

Phase 4: Editor Integration + Expansion
  --> UI dock panel (status display, enable/disable toggle)
  --> script_tools (read, create, edit GDScript)
  --> project_tools (file listing, project settings)
  --> Version detection + conditional tool availability

Phase 5: Advanced Features
  --> Run/debug control (launch game, capture output)
  --> Signal management (query, connect, disconnect)
  --> Resource management (.tres/.res files)
  --> MCP Resources (expose data per spec, not just tools)
```

**Build order rationale:**
- Phase 1 de-risks the highest-risk question: can the bridge+GDExtension architecture work with real MCP clients?
- Phase 2 de-risks the second highest risk: cross-thread marshalling between IO and main thread
- Phase 3 delivers the core value proposition (AI can modify Godot scenes)
- Phase 4 adds polish and expands capability
- Phase 5 adds advanced features that depend on stable foundations

Each phase produces a testable, deployable increment. A user could ship Phase 3 as a useful product.

## Sources

- [MCP Architecture Overview](https://modelcontextprotocol.io/docs/learn/architecture) - Official MCP architecture documentation
- [MCP Transports Specification](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports) - Stdio transport spec (newline-delimited JSON-RPC)
- [MCP Tools Specification](https://modelcontextprotocol.io/specification/2025-11-25/server/tools) - Tool definition and invocation spec
- [GDExtension C++ Example (Godot 4.4 docs)](https://docs.godotengine.org/en/4.4/tutorials/scripting/gdextension/gdextension_cpp_example.html) - Official GDExtension tutorial
- [godot-cpp GitHub](https://github.com/godotengine/godot-cpp) - Official C++ bindings, v10 independent versioning
- [godot-cpp Issue #418](https://github.com/godotengine/godot-cpp/issues/418) - Version detection discussion
- [EditorInterface API (stable)](https://docs.godotengine.org/en/stable/classes/class_editorinterface.html) - Editor API reference
- [Godot Thread-safe APIs](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html) - Threading rules
- [Using Thread in GDExtension (forum)](https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547) - std::thread in GDExtension works
- [Godot Thread Guards (Issue #83900)](https://github.com/godotengine/godot/issues/83900) - Thread safety enforcement in 4.1+
- [cpp-mcp GitHub](https://github.com/hkr04/cpp-mcp) - C++ MCP SDK reference (spec 2024-11-05, outdated)
- [TinyMCP GitHub](https://github.com/Qihoo360/TinyMCP) - C++ MCP server with lifecycle management
- [nlohmann/json GitHub](https://github.com/nlohmann/json) - Header-only C++ JSON library
- [tomyud1/godot-mcp](https://github.com/tomyud1/godot-mcp) - Existing Godot MCP (Node.js + WebSocket, 32 tools)
- [ee0pdt/Godot-MCP](https://github.com/ee0pdt/Godot-MCP) - Existing Godot MCP (Node.js + stdio)
- [Coding-Solo/godot-mcp](https://github.com/Coding-Solo/godot-mcp) - Most popular existing Godot MCP (2.3k stars)
- [MCP 2025-11-25 Spec Update](https://workos.com/blog/mcp-2025-11-25-spec-update) - Latest protocol changes
- [GDExtension EditorPlugin Issues](https://github.com/godotengine/godot/issues/85268) - Known GDExtension EditorPlugin caveats
- [Edited scene root null on init (forum)](https://forum.godotengine.org/t/how-to-get-edited-scene-tree-in-editorplugin/47478) - Scene root timing issue
