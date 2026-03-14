# Domain Pitfalls

**Domain:** GDExtension (C++/godot-cpp) MCP Server Plugin for Godot Editor
**Researched:** 2026-03-14

---

## Critical Pitfalls

Mistakes that cause rewrites, architectural dead-ends, or fundamental design failures.

---

### Pitfall 1: The Stdio Ownership Problem -- GDExtension Shares the Editor Process

**What goes wrong:** A GDExtension is a shared library (.dll / .so / .dylib) loaded directly into the Godot editor process. It does NOT run as a separate process -- it shares the editor's address space, including its stdin/stdout/stderr file descriptors. MCP's stdio transport expects the server to be a standalone process whose stdin/stdout are exclusively used for JSON-RPC messages. When the GDExtension writes to stdout, those bytes go to the Godot editor's stdout, which is already used by the engine for its own logging (`print()`, warnings, errors). The MCP client will receive a mix of Godot engine output and MCP JSON-RPC messages, immediately corrupting the protocol stream.

**Why it happens:** Developers assume "I'm writing a C++ MCP server, I just read from stdin and write to stdout." This works for standalone binaries but is fundamentally incompatible with a dynamically-loaded library running inside another application's process.

**Consequences:** Complete protocol failure. The MCP client (Claude Desktop, etc.) cannot parse responses. The project is architecturally broken from day one.

**Prevention:** There are two viable architectural patterns:

1. **Proxy process pattern (what all existing Godot MCP projects use):** The GDExtension does NOT implement stdio transport itself. Instead, a small standalone binary (written in C++ or any language) acts as the MCP stdio server. The MCP client launches this proxy. The proxy communicates with the GDExtension running inside Godot via a side channel (TCP socket, WebSocket, named pipe, or local UDP). Every existing Godot MCP project uses this pattern: Coding-Solo uses Node.js + CLI invocation, slangwald uses Python + TCP (ports 9500/9501), Dokujaa uses Python + HTTP, etc.

2. **File descriptor redirection (fragile, not recommended):** In theory, the GDExtension could dup2() the process's stdout to a dedicated pipe before the MCP client connects, isolating MCP traffic. This is platform-specific, fragile, conflicts with the editor's own console output expectations, and would break on Windows GUI builds where stdout may not even be allocated.

**Detection:** If your early design has the GDExtension reading from `std::cin` and writing to `std::cout`, stop. This is the wrong architecture.

**Confidence:** HIGH -- verified via Godot docs, godot-cpp shared library behavior, all existing Godot MCP implementations, and OS-level file descriptor semantics.

**Phase relevance:** Phase 1 (core architecture). Must be resolved before any code is written.

---

### Pitfall 2: GDExtension Classes Always Run in the Editor (Implicit @tool)

**What goes wrong:** Unlike GDScript classes (which only run in the editor when marked `@tool`), GDExtension classes are treated like core engine classes. Their `_ready()`, `_process()`, `_physics_process()`, and other callbacks execute in the editor viewport, not just at game runtime. If these callbacks access nodes that don't exist in the editor's scene tree, or initialize heavy resources like network servers, the editor will crash on startup or behave unexpectedly.

**Why it happens:** This is a deliberate Godot design decision -- GDExtension classes are registered in ClassDB like built-in classes. Developers coming from GDScript expect "my code only runs when I hit Play." With GDExtension, your code runs as soon as the editor loads the scene.

**Consequences:** Editor crashes on project open. MCP server starts when the user just opens the Godot editor without intending to use AI tools. Resource leaks from network listeners opened in the editor. Possible "zombie" MCP servers from multiple editor instances.

**Prevention:**
- Guard every lifecycle callback with `Engine::get_singleton()->is_editor_hint()`.
- Use `GDREGISTER_RUNTIME_CLASS()` (available since Godot 4.3) instead of `GDREGISTER_CLASS()` for classes that should only run in-game. However, since this is an editor plugin, the MCP server intentionally runs in the editor, so the real fix is explicit lifecycle management: don't auto-start the server in `_ready()`, require an explicit user action (button click in the editor panel) to start/stop the MCP service.
- Register at `MODULE_INITIALIZATION_LEVEL_EDITOR` so the extension only loads when the editor is running, not in exported builds.

**Detection:** If the Godot editor crashes on startup after adding the plugin, or if test network connections appear immediately on project open, this pitfall is active.

**Confidence:** HIGH -- documented in Godot GitHub issue #54999, godot-cpp docs, and multiple community reports.

**Phase relevance:** Phase 1 (core plugin skeleton). Must be handled from the first EditorPlugin implementation.

---

### Pitfall 3: Godot API Calls from Non-Main Threads Cause Crashes

**What goes wrong:** MCP requests arrive asynchronously (from the proxy process via TCP/WebSocket). A natural implementation reads requests on a background thread and immediately calls Godot APIs (scene tree manipulation, node creation, property access) from that thread. Godot's scene tree and most Node APIs are NOT thread-safe. This causes intermittent crashes, data corruption, or mutex deadlocks -- especially the dreaded `std::system_error` / `__pthread_tpp_change_priority` crash documented in Godot issue #77818.

**Why it happens:** C++ developers are accustomed to free threading with mutex protection. Godot's internal architecture has undocumented shared state, global registries, and internal caches that make even mutex-protected access unsafe. The documentation explicitly states: "Interacting with the active scene tree is NOT thread safe."

**Consequences:** Random crashes under load. Race conditions that only reproduce in specific timing windows. Impossible-to-debug deadlocks between Godot's internal mutexes and your mutexes.

**Prevention:**
- Use a producer-consumer pattern: background thread receives MCP requests and enqueues them. Main thread dequeues and processes them during `_process()` or via `call_deferred()`.
- Use `std::thread` + `std::mutex` + `std::condition_variable` for the networking thread (Godot's `Thread` class has known issues in GDExtension context).
- Never call ANY Godot API from the network thread -- not even seemingly-innocent calls like `String::utf8()` or `ResourceLoader::load()`. Queue everything.
- Return MCP responses asynchronously: main thread processes the request, puts the response in an output queue, background thread sends it.

**Detection:** If you see intermittent crashes with stack traces involving `Mutex`, `ClassDB`, or `SceneTree`, or if your plugin works fine with one request but crashes under rapid successive requests, this pitfall is active.

**Confidence:** HIGH -- verified in Godot threading documentation and godot-cpp GitHub issues.

**Phase relevance:** Phase 1-2 (core architecture, first tools implementation). The request/response pipeline design must be correct from the start.

---

### Pitfall 4: Cross-Version Compatibility is More Broken Than Advertised

**What goes wrong:** The project targets "Godot 4.0+" but GDExtension ABI compatibility has been broken at nearly every minor version boundary:
- Godot 4.0.x: No cross-version compatibility at all -- even patch versions break each other.
- 4.0 to 4.1: Breaking change. `compatibility_minimum` key introduced (required from 4.1+).
- 4.1 to 4.2: Breaking change confirmed by multiple plugins (godot-git-plugin, GodotSteam).
- 4.2 to 4.3: Resource serialization regression for GDExtension classes.
- 4.3 to 4.4: API additions but forward-compatible.
- 4.4 patch levels: Even 4.4.0 to 4.4.1 reported as incompatible.

The stated goal of "build once, run on all 4.x" is not achievable. At best, you can target a minimum version (e.g., 4.3) and have forward compatibility to newer versions.

**Why it happens:** GDExtension is still marked "experimental" by the Godot team. The ABI is evolving. godot-cpp v10 introduced independent versioning with an `api_version` parameter, but this only works for Godot 4.3+.

**Consequences:** Users on older Godot versions get silent load failures or crashes. Supporting "Godot 4.0+" requires maintaining multiple build targets -- potentially 4-5 separate compiled binaries. CI/CD complexity explodes.

**Prevention:**
- Set a realistic minimum version. Godot 4.3 is the pragmatic floor because: godot-cpp v10 supports 4.3+ via `api_version`, `GDREGISTER_RUNTIME_CLASS` was added in 4.3, and the GDExtension ABI stabilized significantly from 4.3 onward.
- Set `compatibility_minimum = "4.3"` in the `.gdextension` file (always quote the version string to avoid parsing bugs with patch versions).
- Use godot-cpp v10's `api_version` parameter to target 4.3 as the base API level.
- For users on 4.0-4.2, document that the plugin is not supported. These versions represent a shrinking user base.
- Test on at least 2-3 Godot versions in CI (e.g., 4.3, latest 4.x stable).
- Use `Object::call()` reflection for API features only available in newer versions, accepting the loss of type safety.

**Detection:** The `.gdextension` file has `compatibility_minimum = "4.0"` or the project README claims "works on all Godot 4.x versions" without per-version testing.

**Confidence:** HIGH -- verified via Godot GitHub issues (#75779, #93676, #1643), godot-cpp README, and GodotSteam compatibility reports.

**Phase relevance:** Phase 1 (project setup). The minimum version decision cascades through build configuration, CI, and API usage patterns.

---

### Pitfall 5: Cannot Call Godot APIs in Constructors or During Initialization

**What goes wrong:** GDExtension class constructors run before the object is fully integrated into Godot's object system. Calling Godot methods, accessing singletons (like `RenderingServer`, `EditorInterface`), or interacting with ClassDB from a constructor causes crashes. This was a known issue in GDNative that was "fixed" with `_init()` but regressed in the GDExtension rewrite.

Additionally, `EditorHelp` creates instances of EVERY class in ClassDB when the editor starts (to get property default values). If your constructor does anything non-trivial -- opening a socket, allocating Godot resources, starting threads -- it will execute during editor startup for documentation generation, not just when your plugin is actually used.

**Why it happens:** The GDExtension binding layer calls the C++ constructor before setting the `owner` pointer. Godot singletons like `RenderingServer` are deliberately unavailable at `MODULE_INITIALIZATION_LEVEL_SCENE` initialization.

**Consequences:** Editor crashes on startup with no useful error message. "Hard crash with no error" is the exact symptom reported in Godot issue #71173.

**Prevention:**
- Keep constructors trivially simple: initialize C++ member variables to defaults only. No Godot API calls.
- Move all real initialization to `_ready()` or `_enter_tree()`.
- For the EditorPlugin specifically, use `_enter_tree()` to set up UI and `_exit_tree()` to tear down.
- Do NOT start the MCP server or open network connections in the constructor or `_ready()`. Use an explicit "start server" method triggered by user action.

**Detection:** Editor crashes on startup when the GDExtension is present, even before any scene is opened. Stack traces show crashes in constructor code or singleton access.

**Confidence:** HIGH -- documented in Godot issues #81478, #91401, and godot_voxel #442.

**Phase relevance:** Phase 1 (plugin skeleton). The very first class implementation must follow this pattern.

---

## Moderate Pitfalls

---

### Pitfall 6: Stdout Pollution From Dependencies or Debug Code

**What goes wrong:** Even with the proxy architecture, the standalone MCP server binary must keep its stdout exclusively for JSON-RPC messages. Any stray `printf`, `std::cout`, `fprintf(stdout, ...)`, or third-party library output going to stdout corrupts the MCP protocol stream. This is the #1 reported MCP debugging issue across all languages and implementations.

**Prevention:**
- Redirect all logging to stderr from day one. Create a logging utility that wraps `fprintf(stderr, ...)` and never use `std::cout` or `printf` anywhere.
- Audit all third-party libraries for stdout output. JSON parsing libraries, networking libraries, etc. may print warnings or debug info to stdout.
- In debug builds, assert that stdout only contains valid JSON-RPC messages by running the server with output validation.
- Consider redirecting the process's stdout file descriptor to a pipe and validating output in tests.

**Detection:** MCP client reports "malformed message" or "parse error." Adding a new library or enabling debug logging suddenly breaks the protocol.

**Confidence:** HIGH -- the single most commonly reported MCP server bug per official MCP documentation and community reports.

**Phase relevance:** Phase 1-2 (MCP protocol implementation). Establish the logging discipline immediately.

---

### Pitfall 7: JSON-RPC 2.0 Compliance Subtleties

**What goes wrong:** MCP uses JSON-RPC 2.0 strictly. Common implementation mistakes include:
- Sending a response to a notification (notifications have no `id` field -- the server MUST NOT reply).
- Not copying the request `id` to the response (the `id` must match exactly, including type -- if the client sends `"id": 1` as an integer, responding with `"id": "1"` as a string is wrong).
- Missing the `"jsonrpc": "2.0"` field in responses.
- Including embedded newlines in JSON output (messages are newline-delimited).
- Not implementing the initialization handshake (capabilities negotiation) before processing other requests.
- Incorrect error codes (must follow JSON-RPC 2.0 error code ranges).

**Prevention:**
- Implement a strict JSON-RPC 2.0 message parser/serializer as a dedicated module with tests.
- Write test cases for: notifications (no response), batch requests, error responses, capability negotiation.
- Ensure the JSON serializer does not pretty-print (no embedded newlines in output).
- Use a well-tested JSON library (nlohmann/json for C++) and validate output format.
- Implement the MCP initialization handshake (`initialize` request + `initialized` notification) before accepting any tool calls.

**Detection:** MCP client hangs after connection (failed handshake), returns "method not found" (incorrect method name format), or silently drops responses (mismatched `id`).

**Confidence:** HIGH -- documented in JSON-RPC 2.0 specification and MCP protocol specification.

**Phase relevance:** Phase 1 (MCP protocol core). The JSON-RPC layer must be correct before building tools on top.

---

### Pitfall 8: Windows GUI Application Has No Console / stdout

**What goes wrong:** On Windows, the Godot editor is compiled as a GUI application (`/SUBSYSTEM:WINDOWS`) in release builds. GUI applications do NOT get stdin/stdout/stderr handles by default. This affects both the GDExtension (which runs inside the editor) and has implications for the proxy binary if it's accidentally compiled as a GUI app.

Even for the proxy binary: if compiled with MSVC without `/SUBSYSTEM:CONSOLE`, stdout will be null. For the GDExtension itself: any C++ code that tries to write to `stdout` or read from `stdin` may get null file handles, causing segfaults or silent failures.

**Prevention:**
- The proxy binary must be compiled explicitly as a console application (`/SUBSYSTEM:CONSOLE` on MSVC, default for MinGW).
- Inside the GDExtension, never assume stdout is valid. All GDExtension-side logging should use Godot's `UtilityFunctions::print()` or `push_error()` / `push_warning()`.
- On Windows, `AllocConsole()` can be called to create a console for debugging, but this should never be in release builds.

**Detection:** Plugin works on Linux/macOS but silently fails on Windows. Proxy binary launches but immediately exits. No error messages visible anywhere.

**Confidence:** MEDIUM -- based on Windows application model documentation and Godot build configuration.

**Phase relevance:** Phase 2-3 (cross-platform testing). Must be validated when first testing on Windows.

---

### Pitfall 9: Build System Complexity (SCons + Cross-Platform + CI)

**What goes wrong:** godot-cpp uses SCons as its build system. Cross-compilation between platforms is poorly documented and frequently broken:
- Cross-compiling from macOS to Windows encounters `wcscpy_s` errors with MinGW.
- Cross-compiling from Linux to Windows produces `.so` extensions instead of `.dll`.
- macOS cross-compilation requires osxcross with a custom MoltenVK SDK.
- Debug vs. release build confusion: the editor uses debug builds, exported games use release.
- Float precision mismatch: extension must match the engine's float precision (single vs. double).

Distributing a GDExtension means providing compiled binaries for every platform/architecture combination: Windows x86_64, Linux x86_64, macOS universal (x86_64 + arm64), and potentially Linux arm64.

**Prevention:**
- Use GitHub Actions CI with platform-specific runners (windows-latest, ubuntu-latest, macos-latest) for native compilation rather than cross-compilation.
- Use the official `godot-cpp-template` repository as a starting point -- it includes a working CI workflow.
- Build both debug and release variants for each platform.
- Automate the creation of the addon zip file containing all platform binaries.
- Consider CMake as an alternative to SCons if the team is more familiar -- it works with godot-cpp with minimal configuration.
- Always test debug builds (what the editor uses) before testing release builds.

**Detection:** CI builds green on one platform but broken on others. Users report "No GDExtension library found for current OS and architecture" -- usually a path or naming error in the `.gdextension` file.

**Confidence:** HIGH -- verified via multiple godot-cpp GitHub issues and community reports.

**Phase relevance:** Phase 1 (project setup). The build system must produce correct binaries for at least one platform from the start, with CI added early.

---

### Pitfall 10: EditorPlugin Lifecycle and Hot-Reload Issues

**What goes wrong:** On Windows, the Godot editor locks the GDExtension DLL file while running. You cannot recompile and replace the DLL without closing the editor. This means the edit-compile-test cycle requires a full editor restart on every change. On Linux/macOS, the shared library can technically be replaced, but Godot does not support hot-reloading GDExtension libraries while the editor is running (documented in Godot issue #66231).

Additionally, the plugin's `_exit_tree()` may not be called during editor crashes, leaving the MCP server in an inconsistent state (open sockets, locked resources).

**Prevention:**
- Accept the restart cycle on Windows. Keep compilation fast by using precompiled headers and incremental builds.
- Implement robust cleanup: the MCP communication channel (TCP/WebSocket) should handle ungraceful disconnections. Use SO_REUSEADDR on server sockets to avoid "address already in use" errors after crashes.
- On the proxy side: implement heartbeat/reconnect logic so the proxy can detect when the editor plugin goes away and reconnect when it comes back.
- Consider developing the MCP protocol and tool logic in a separate testable library, only wrapping it in GDExtension bindings at the integration layer. This allows testing without the editor.

**Detection:** "Address already in use" errors after editor crashes. Developer frustration with the compile-restart cycle slowing iteration.

**Confidence:** HIGH -- documented in Godot issue #66231, confirmed by all GDExtension developers.

**Phase relevance:** Phase 1-2 (development workflow). Affects daily development velocity throughout the project.

---

## Minor Pitfalls

---

### Pitfall 11: Forgetting Class Registration

**What goes wrong:** Every GDExtension class must be explicitly registered in `register_types.cpp` using `GDREGISTER_CLASS()`. Missing a class results in silent failures -- the class simply doesn't exist in Godot. No error message, no warning.

**Prevention:** Keep a checklist. Consider using a code generator or macro pattern that makes it impossible to define a class without registering it. Test that all classes appear in the Godot editor's class list after loading.

**Phase relevance:** Phase 1 (any time a new class is added).

---

### Pitfall 12: Using STL Containers Instead of Godot Containers

**What goes wrong:** Using `std::string`, `std::vector`, `std::map` at the GDExtension boundary (in methods exposed to Godot) causes type mismatches. Godot expects `String`, `Array`, `Dictionary`. STL types work fine internally but must be converted at the boundary.

**Prevention:** Use Godot types (`String`, `Array`, `Dictionary`, `PackedStringArray`, etc.) for all public API surfaces. Use STL internally in the C++ implementation if preferred, but convert at the boundary. Be aware that Godot's `String` is UTF-32 internally, not UTF-8 -- conversions via `String::utf8()` and `String(const char*)` are needed.

**Phase relevance:** Phase 1 (from the first exposed method).

---

### Pitfall 13: MCP Spec Evolution and Versioning

**What goes wrong:** The MCP specification is evolving. SSE transport is already deprecated in favor of Streamable HTTP. Protocol versions may introduce breaking changes. Building against a specific spec version without tracking updates leads to client incompatibility.

**Prevention:** Implement the stdio transport only (as planned -- this is the most stable transport). Track the MCP spec version in the capability negotiation response. Pin to a specific MCP spec version (currently 2025-06-18 is latest) and document it. Monitor the official MCP specification repository for breaking changes.

**Phase relevance:** Ongoing, but especially Phase 1 (protocol implementation).

---

### Pitfall 14: UTF-8 Encoding Issues on Windows

**What goes wrong:** MCP JSON-RPC messages must be valid UTF-8. On Windows, the default console encoding is not UTF-8 (it's typically Windows-1252 or the system locale codepage). If the proxy binary reads/writes to stdio without explicitly setting UTF-8 mode, non-ASCII characters (e.g., in file paths, node names, or script content) get corrupted.

**Prevention:** On Windows, call `SetConsoleOutputCP(CP_UTF8)` and `SetConsoleCP(CP_UTF8)` at proxy startup. Use `_setmode(_fileno(stdin), _O_BINARY)` and `_setmode(_fileno(stdout), _O_BINARY)` to prevent Windows from translating newlines. In the GDExtension, Godot's `String` handles encoding correctly -- just ensure all JSON serialization produces valid UTF-8.

**Phase relevance:** Phase 2-3 (cross-platform validation).

---

### Pitfall 15: The `.gdextension` File Quoting Bug

**What goes wrong:** If `compatibility_minimum` contains a patch version like `4.3.1` without quotes, Godot's INI parser misinterprets it and produces a misleading error: "No GDExtension library found for current OS and architecture" instead of the actual version mismatch error.

**Prevention:** Always quote version strings in the `.gdextension` file: `compatibility_minimum = "4.3"`. This is documented in Godot docs issue #7864.

**Phase relevance:** Phase 1 (project setup).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Core architecture / MCP transport | Pitfall 1 (stdio ownership) | Design the proxy + side-channel architecture before writing any code |
| Plugin skeleton / GDExtension setup | Pitfall 2 (implicit @tool), Pitfall 5 (constructor crashes) | Guard all callbacks, trivial constructors, editor-level init only |
| MCP protocol implementation | Pitfall 6 (stdout pollution), Pitfall 7 (JSON-RPC compliance) | Stderr-only logging, strict protocol tests |
| Scene tree / node manipulation tools | Pitfall 3 (thread safety) | Main-thread-only Godot API access via call_deferred or queue |
| Version compatibility / distribution | Pitfall 4 (cross-version), Pitfall 9 (build system) | Target 4.3+ minimum, CI per-platform builds |
| Cross-platform release | Pitfall 8 (Windows GUI), Pitfall 14 (UTF-8) | Console subsystem for proxy, explicit encoding setup |
| Development workflow | Pitfall 10 (hot-reload) | Separate testable library, fast incremental builds |

---

## Sources

### Godot / GDExtension
- [GDExtension C++ example -- Godot 4.4 docs](https://docs.godotengine.org/en/4.4/tutorials/scripting/gdextension/gdextension_cpp_example.html)
- [What is GDExtension -- Godot stable docs](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html)
- [Thread-safe APIs -- Godot stable docs](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html)
- [GDExtension classes crash editor -- Godot #54999](https://github.com/godotengine/godot/issues/54999)
- [GDExtension threading crash -- Godot #77818](https://github.com/godotengine/godot/issues/77818)
- [4.0.2 breaks GDExtension API -- Godot #75779](https://github.com/godotengine/godot/issues/75779)
- [GDExtension library cannot be reloaded -- Godot #66231](https://github.com/godotengine/godot/issues/66231)
- [Resources from GDExtension broken in 4.3 -- Godot #93676](https://github.com/godotengine/godot/issues/93676)
- [godot-cpp v10 version mismatch -- godot-cpp #1643](https://github.com/godotengine/godot-cpp/issues/1643)
- [.gdextension quoting bug -- godot-docs #7864](https://github.com/godotengine/godot-docs/issues/7864)
- [godot-cpp GitHub repository](https://github.com/godotengine/godot-cpp)
- [GDExtension threading in forum](https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547)
- [Bringing C++ to Godot with GDExtensions](https://vilelasagna.ddns.net/coding/bringing-c-to-godot-with-gdextensions/)

### MCP Protocol
- [MCP Tips, Tricks and Pitfalls -- Nearform](https://nearform.com/digital-community/implementing-model-context-protocol-mcp-tips-tricks-and-pitfalls/)
- [MCP Server Development Guide](https://github.com/cyanheads/model-context-protocol-resources/blob/main/guides/mcp-server-development-guide.md)
- [MCP Transports Specification](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports)
- [MCP Troubleshooting -- SFEIR](https://institute.sfeir.com/en/claude-code/claude-code-mcp-model-context-protocol/troubleshooting/)
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
- [Understanding MCP stdio transport](https://medium.com/@laurentkubaski/understanding-mcp-stdio-transport-protocol-ae3d5daf64db)
- [Gopher MCP C++ SDK](https://github.com/GopherSecurity/gopher-mcp)

### Existing Godot MCP Implementations (Architecture Reference)
- [Coding-Solo/godot-mcp](https://github.com/Coding-Solo/godot-mcp) -- TypeScript MCP server + CLI/GDScript
- [slangwald/godot-mcp](https://github.com/slangwald/godot-mcp) -- Python MCP server + TCP (9500/9501) + Editor Plugin
- [Dokujaa/Godot-MCP](https://github.com/Dokujaa/Godot-MCP) -- Python MCP server + Editor Plugin
- [bradypp/godot-mcp](https://github.com/bradypp/godot-mcp) -- Node.js MCP server
- [tomyud1/godot-mcp](https://github.com/tomyud1/godot-mcp) -- Available on AssetLib
- [Godot MCP Pro](https://godot-mcp.abyo.net/) -- Premium, WebSocket-based, 162 tools

### OS / System Level
- [Godot execute_with_pipe](https://dev.to/jeankouss/seamless-inter-process-communication-with-godots-executewithpipe-537i)
- [Windows console vs GUI application -- Godot proposals #1341](https://github.com/godotengine/godot-proposals/issues/1341)
- [Godot stdout/stderr access proposal -- Godot proposals #536](https://github.com/godotengine/godot-proposals/issues/536)
