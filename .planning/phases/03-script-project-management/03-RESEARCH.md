# Phase 3: Script & Project Management - Research

**Researched:** 2026-03-17
**Domain:** GDScript file I/O, project filesystem queries, resource inspection, MCP Resources protocol, threading model
**Confidence:** HIGH

## Summary

Phase 3 adds 9 requirements spanning four domains: script file management (read/write/edit/attach), project structure queries (file listing, project settings, resource inspection), MCP Resources protocol (structured read-only data), and IO thread safety (queue+promise pattern deferred from Phase 1).

The Godot APIs needed are well-understood and verified against actual godot-cpp headers in the project. FileAccess, DirAccess, ResourceLoader, and ProjectSettings are all available as static/singleton APIs in the godot-cpp bindings. The MCP Resources protocol is well-specified (resources/list, resources/read) and straightforward to implement as new method handlers in the existing dispatch chain.

The threading model (MCP-04) is the most architecturally significant change -- it moves TCP I/O to a dedicated thread with a queue+promise bridge to the main thread. However, the current single-threaded polling architecture already works correctly; the threading change is an optimization for concurrent request handling, not a correctness fix. This makes it safe to implement alongside the tool additions.

**Primary recommendation:** Implement tools first (script_tools, project_tools), then MCP Resources protocol handlers, then the IO thread refactor. This ordering ensures tools can be tested immediately via existing polling before the threading change lands.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Provide both full-file replacement AND line-level editing
- `write_script`: create new .gd files with full content (errors if file already exists)
- `read_script`: returns entire file content (no line-range support needed)
- `edit_script`: single tool with `operation` parameter (insert/replace/delete) for line-level editing, 1-based line numbers
- No UndoRedo for script file operations -- git is the undo mechanism
- `attach_script`: attaches existing .gd file to node (does NOT create files inline), uses UndoRedo
- `detach_script`: removes script from node
- If node already has a script, attach directly replaces it (no error, no force flag)
- Tool names are GDScript-specific, internal implementation is generic text file I/O
- All file paths use `res://` prefix format
- File access restricted to res:// scope only
- No write-protection on specific paths
- After file write, call EditorFileSystem scan to refresh editor
- Resource operations (PROJ-03): read-only via ResourceLoader, returns type + property names/values, supports .tres and .res
- No resource creation or modification in Phase 3
- `list_project_files`: flat list of all files under res:// with paths and types
- `get_project_settings`: reads project.godot settings as structured data
- Two MCP Resources: `godot://scene_tree` and `godot://project_files`
- Scene tree Resource coexists with `get_scene_tree` tool
- Requires adding `resources` capability to MCP initialize response

### Claude's Discretion
- IO thread + queue/promise architecture for MCP-04 (threading model details)
- Tool registration pattern (extend if-else chain or refactor)
- MCP Resources protocol implementation details (resources/list, resources/read handlers)
- Whether to add `resources/templates` capability alongside resources
- Exact JSON structure of resource query results
- Error message wording for new tools
- Whether edit_script supports multiple operations in single call

### Deferred Ideas (OUT OF SCOPE)
- Shader file editing (.gdshader)
- Resource modification (write .tres/.res via ResourceSaver)
- Line-range reading for very large scripts
- Resource write protection / path blacklist
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SCRP-01 | AI can read GDScript file content | FileAccess::open(READ) + get_as_text() -- verified in godot-cpp headers |
| SCRP-02 | AI can create new GDScript files | FileAccess::open(WRITE) + store_string() -- verified; EditorFileSystem::update_file() for refresh |
| SCRP-03 | AI can edit existing GDScript files | FileAccess read+parse lines+write pattern; line-level insert/replace/delete |
| SCRP-04 | AI can attach/detach scripts to/from nodes | Node::set_script() + ResourceLoader::load() for attach; set_script(Variant()) for detach; UndoRedo integration |
| PROJ-01 | AI can query project file structure | DirAccess recursive traversal of res:// -- verified API signatures |
| PROJ-02 | AI can read project.godot settings | ProjectSettings::get_singleton() + iteration via get_property_list() |
| PROJ-03 | AI can query .tres/.res resource files | ResourceLoader::get_singleton()->load() + Object::get_property_list() + Object::get() |
| PROJ-04 | Scene tree and project files as MCP Resources | MCP resources/list + resources/read protocol handlers; godot:// custom URI scheme |
| MCP-04 | IO thread + queue/promise pattern | std::thread for TCP I/O, std::mutex + std::condition_variable queue, main thread polls results in _process() |
</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ | C++ bindings for Godot 4.3+ | Only official C++ binding for GDExtension |
| nlohmann/json | 3.12.0 | JSON serialization/deserialization | Already used for MCP protocol, tool results |
| GoogleTest | 1.17.0 | Unit testing for pure C++ protocol code | Already configured in tests/CMakeLists.txt |

### Godot APIs Required (verified from godot-cpp headers)
| API Class | Header | Purpose | Key Methods |
|-----------|--------|---------|-------------|
| FileAccess | `<godot_cpp/classes/file_access.hpp>` | Read/write script files | `open()`, `get_as_text()`, `store_string()`, `file_exists()`, `close()` |
| DirAccess | `<godot_cpp/classes/dir_access.hpp>` | List project file structure | `open()`, `list_dir_begin()`, `get_next()`, `current_is_dir()`, `get_files()` |
| ResourceLoader | `<godot_cpp/classes/resource_loader.hpp>` | Load .tres/.res resources, load scripts for attach | `get_singleton()`, `load()`, `exists()` |
| ProjectSettings | `<godot_cpp/classes/project_settings.hpp>` | Read project.godot settings | `get_singleton()`, `get_setting()`, `has_setting()` |
| EditorFileSystem | `<godot_cpp/classes/editor_file_system.hpp>` | Refresh editor after file changes | `update_file()`, `scan()` |
| EditorInterface | `<godot_cpp/classes/editor_interface.hpp>` | Access EditorFileSystem singleton | `get_resource_filesystem()` |
| Resource | `<godot_cpp/classes/resource.hpp>` | Base class for loaded resources | `get_path()`, `get_name()`, `get_class()` |
| Object | `<godot_cpp/classes/object.hpp>` | Property inspection on resources | `get_property_list()`, `get()`, `set_script()`, `get_script()` |

### Supporting (C++ standard library for threading)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<thread>` | C++17 | IO thread for TCP polling | MCP-04 threading model |
| `<mutex>` | C++17 | Thread-safe request/response queue | MCP-04 queue protection |
| `<condition_variable>` | C++17 | IO thread wake-up signaling | MCP-04 response notification |
| `<queue>` | C++17 | Request/response FIFO queues | MCP-04 message passing |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| std::thread | Godot Thread class | std::thread is simpler, no Godot dependency for IO thread; Godot Thread adds ClassDB overhead |
| std::mutex | Godot Mutex | std::mutex is C++ standard, no Godot binding overhead |
| DirAccess recursive | EditorFileSystem::get_filesystem() | EditorFileSystem provides cached file type info but requires editor to have completed scanning; DirAccess is more reliable for immediate queries |

## Architecture Patterns

### Recommended Project Structure
```
src/
├── mcp_server.h/cpp      # MCPServer class (refactored for IO thread)
├── mcp_protocol.h/cpp    # JSON-RPC protocol + MCP message builders (add resources)
├── scene_tools.h/cpp     # Scene tree query (existing)
├── scene_mutation.h/cpp  # Scene CRUD tools (existing)
├── script_tools.h/cpp    # NEW: read_script, write_script, edit_script, attach/detach
├── project_tools.h/cpp   # NEW: list_project_files, get_project_settings, get_resource_info
├── variant_parser.h/cpp  # Variant type parsing (existing, reusable for display)
├── mcp_plugin.h/cpp      # Editor plugin entry point (existing)
└── register_types.cpp    # Class registration (existing)
```

### Pattern 1: Tool Module Pattern (established)
**What:** Each tool domain gets its own header/cpp pair with free functions returning `nlohmann::json`
**When to use:** All new tools (script_tools, project_tools)
**Example:**
```cpp
// Source: Established pattern from scene_tools.h, scene_mutation.h
// script_tools.h
#include <nlohmann/json.hpp>
#include <string>

namespace godot { class EditorUndoRedoManager; }

nlohmann::json read_script(const std::string& path);
nlohmann::json write_script(const std::string& path, const std::string& content);
nlohmann::json edit_script(const std::string& path, const std::string& operation,
                           int line, const std::string& content, int end_line = -1);
nlohmann::json attach_script(const std::string& node_path, const std::string& script_path,
                              godot::EditorUndoRedoManager* undo_redo);
nlohmann::json detach_script(const std::string& node_path,
                              godot::EditorUndoRedoManager* undo_redo);
```

### Pattern 2: MCP Resources Handler in Protocol Layer
**What:** Add resources/list and resources/read method handlers to mcp_server.cpp dispatch, with protocol helpers in mcp_protocol.cpp
**When to use:** MCP Resources implementation (PROJ-04)
**Example:**
```cpp
// In mcp_protocol.h -- add new builders
nlohmann::json create_resources_list_response(const nlohmann::json& id,
                                               const nlohmann::json& resources);
nlohmann::json create_resource_read_response(const nlohmann::json& id,
                                              const nlohmann::json& contents);

// In mcp_protocol.cpp -- update initialize response
nlohmann::json create_initialize_response(const nlohmann::json& id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"protocolVersion", "2025-03-26"},
            {"capabilities", {
                {"tools", {{"listChanged", false}}},
                {"resources", {}}  // NEW: empty object = basic resources support
            }},
            {"serverInfo", {
                {"name", "godot-mcp-meow"},
                {"version", "0.2.0"}
            }}
        }}
    };
}
```

### Pattern 3: IO Thread + Queue Pattern (MCP-04)
**What:** Dedicated IO thread handles TCP accept/read/write; main thread processes requests via queue
**When to use:** MCP-04 threading requirement
**Architecture:**
```
IO Thread                          Main Thread (_process)
---------                          ---------------------
TCP accept/read                    Check response queue
Parse JSON-RPC                     -> If response ready, dequeue and TCP send
Put request in request_queue  -->  Check request queue
                                   -> If request ready, dequeue
                                   -> Execute tool function (Godot API calls)
Wait for response             <--  Put result in response_queue
TCP write response
```
**Key detail:** All Godot API calls (scene tree, file access, etc.) MUST happen on the main thread. The IO thread only handles TCP byte shuffling and JSON parsing.

### Pattern 4: res:// Path Validation
**What:** Validate all file paths start with `res://` before any file operation
**When to use:** Every script and project tool function
**Example:**
```cpp
// Common validation at top of every file tool
static bool validate_res_path(const std::string& path, nlohmann::json& error_out) {
    if (path.substr(0, 6) != "res://") {
        error_out = {{"error", "Path must start with res://: " + path}};
        return false;
    }
    return true;
}
```

### Anti-Patterns to Avoid
- **Direct file writes without EditorFileSystem notification:** Files written by FileAccess won't appear in the editor file browser until EditorFileSystem is notified. Always call `update_file()` after writes.
- **Accessing scene tree from IO thread:** Scene tree is NOT thread-safe. All Node/scene operations must happen on main thread. Even read-only queries must be on main thread.
- **Using DirAccess in exported builds for res://:** DirAccess behavior differs between editor and exports. Since this is an editor plugin, this is not a concern, but worth noting.
- **Blocking main thread with IO waits:** Never block `_process()` waiting for TCP data. The queue pattern ensures non-blocking operation.
- **Using scan() for single file updates:** `scan()` rescans the entire project filesystem. Use `update_file(path)` for single file changes -- much faster.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| File read/write | Custom fopen/fwrite | Godot FileAccess | Handles res:// path resolution, cross-platform, proper encoding |
| Directory listing | OS-specific opendir/readdir | Godot DirAccess | Handles res:// virtual filesystem, cross-platform |
| Resource loading | Manual .tres/.res parsing | Godot ResourceLoader | Handles all Godot resource formats, import pipeline |
| Project settings | Manual project.godot INI parsing | Godot ProjectSettings singleton | Already parsed and cached by engine |
| Script loading for attach | Manual GDScript parsing | ResourceLoader::load() | Handles script dependencies, class registration |
| Thread-safe queue | Lock-free ring buffer | std::mutex + std::queue | Correctness over performance; request rate is low (AI tool calls) |

**Key insight:** Godot provides singleton APIs for all file/project operations that handle the res:// virtual filesystem correctly. Building custom parsers would duplicate engine logic and break on edge cases (imported resources, .godot cache, etc.).

## Common Pitfalls

### Pitfall 1: FileAccess::open Returns Null on Error
**What goes wrong:** Not checking the returned Ref for validity leads to null pointer dereference
**Why it happens:** open() returns a null Ref if the file doesn't exist or permissions are wrong, rather than throwing
**How to avoid:** Always check `file.is_valid()` before using. Use `FileAccess::get_open_error()` to get the specific error code.
**Warning signs:** Crashes in file operations with no error message

### Pitfall 2: EditorFileSystem scan() Causes Reentrant Crashes
**What goes wrong:** Calling `scan()` from within a tool handler can trigger reimport which calls Main::iteration reentrantly
**Why it happens:** scan() is asynchronous and triggers the full import pipeline
**How to avoid:** Use `update_file(path)` for single files instead of `scan()`. Only use `scan()` if absolutely necessary and only for bulk operations.
**Warning signs:** Editor crashes or freezes during file write operations

### Pitfall 3: Line Number Off-By-One in edit_script
**What goes wrong:** User expects 1-based line numbers but implementation uses 0-based indexing
**Why it happens:** C++ std::vector is 0-indexed, context specifies 1-based
**How to avoid:** Convert at the tool boundary: `internal_idx = user_line - 1`. Validate `line >= 1` before conversion. Return errors for line 0 or negative.
**Warning signs:** Wrong lines being edited, especially at file boundaries

### Pitfall 4: File Write Truncates Without Warning
**What goes wrong:** `FileAccess::WRITE` mode truncates existing files, losing content
**Why it happens:** WRITE mode creates or truncates (standard file I/O behavior)
**How to avoid:** For `write_script`, check `FileAccess::file_exists()` first and error if file exists (per context decisions). For `edit_script`, read file first, modify in memory, then write back.
**Warning signs:** Files being emptied unexpectedly

### Pitfall 5: Object::get_property_list Returns ALL Properties Including Internal Ones
**What goes wrong:** Resource property inspection returns hundreds of internal engine properties
**Why it happens:** get_property_list() includes all inherited properties from Object, RefCounted, Resource, etc.
**How to avoid:** Filter properties by usage flags. Properties with PROPERTY_USAGE_EDITOR or PROPERTY_USAGE_STORAGE are the user-visible ones. Skip properties where usage has PROPERTY_USAGE_INTERNAL flag.
**Warning signs:** Huge JSON output with meaningless internal properties

### Pitfall 6: set_script Requires Variant, Not Ref<Script>
**What goes wrong:** Compilation errors or incorrect script attachment
**Why it happens:** Object::set_script() takes a Variant parameter, not Ref<Script>
**How to avoid:** Load with ResourceLoader::load() which returns Ref<Resource>, then pass directly -- Ref<Resource> auto-converts to Variant. For detach, pass `Variant()` (nil).
**Warning signs:** Script appears attached but doesn't actually work

### Pitfall 7: Threading -- Godot API Calls from IO Thread Crash
**What goes wrong:** Accessing EditorInterface, FileAccess, or any Godot singleton from the IO thread causes crashes or assertion failures
**Why it happens:** Godot 4.x enforces thread safety checks: "Function can only be accessed from the main thread"
**How to avoid:** IO thread ONLY handles TCP socket operations and JSON parsing. All Godot API calls go through the request queue and execute on the main thread in _process().
**Warning signs:** `Condition "!is_readable_from_caller_thread()" is true` errors

## Code Examples

Verified patterns from godot-cpp headers:

### Reading a Script File (SCRP-01)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/file_access.hpp -- verified static methods
#include <godot_cpp/classes/file_access.hpp>

nlohmann::json read_script(const std::string& path) {
    godot::String godot_path(path.c_str());

    if (!godot::FileAccess::file_exists(godot_path)) {
        return {{"error", "File not found: " + path}};
    }

    godot::Ref<godot::FileAccess> file = godot::FileAccess::open(godot_path, godot::FileAccess::READ);
    if (!file.is_valid()) {
        return {{"error", "Failed to open file: " + path}};
    }

    godot::String content = file->get_as_text();
    file->close();

    std::string content_str(content.utf8().get_data());
    return {
        {"success", true},
        {"path", path},
        {"content", content_str},
        {"line_count", std::count(content_str.begin(), content_str.end(), '\n') + 1}
    };
}
```

### Writing a New Script File (SCRP-02)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/file_access.hpp + editor_file_system.hpp
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>

nlohmann::json write_script(const std::string& path, const std::string& content) {
    godot::String godot_path(path.c_str());

    // Error if file already exists (per context decisions)
    if (godot::FileAccess::file_exists(godot_path)) {
        return {{"error", "File already exists: " + path + ". Use edit_script to modify existing files."}};
    }

    godot::Ref<godot::FileAccess> file = godot::FileAccess::open(godot_path, godot::FileAccess::WRITE);
    if (!file.is_valid()) {
        return {{"error", "Failed to create file: " + path}};
    }

    file->store_string(godot::String(content.c_str()));
    file->close();

    // Notify editor filesystem
    godot::EditorFileSystem* efs = godot::EditorInterface::get_singleton()->get_resource_filesystem();
    efs->update_file(godot_path);

    return {{"success", true}, {"path", path}};
}
```

### Attaching Script to Node (SCRP-04)
```cpp
// Source: godot-cpp headers -- Object::set_script(Variant), ResourceLoader::load()
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>

nlohmann::json attach_script(const std::string& node_path, const std::string& script_path,
                              godot::EditorUndoRedoManager* undo_redo) {
    godot::Node* scene_root = godot::EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) return {{"error", "No scene open"}};

    godot::Node* node = scene_root->get_node_or_null(godot::NodePath(node_path.c_str()));
    if (!node) return {{"error", "Node not found: " + node_path}};

    // Load script as resource
    godot::Ref<godot::Resource> script = godot::ResourceLoader::get_singleton()->load(
        godot::String(script_path.c_str()), "Script");
    if (!script.is_valid()) return {{"error", "Script not found: " + script_path}};

    // UndoRedo for attach (scene tree operation)
    godot::Variant old_script = node->get_script();
    undo_redo->create_action(godot::String("MCP: Attach script"));
    undo_redo->add_do_method(node, "set_script", script);
    undo_redo->add_undo_method(node, "set_script", old_script);
    undo_redo->commit_action();

    return {{"success", true}, {"node_path", node_path}, {"script_path", script_path}};
}
```

### Listing Project Files (PROJ-01)
```cpp
// Source: godot-cpp/gen/include/godot_cpp/classes/dir_access.hpp -- verified API
#include <godot_cpp/classes/dir_access.hpp>

void list_files_recursive(const godot::String& dir_path, nlohmann::json& files) {
    godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(dir_path);
    if (!dir.is_valid()) return;

    // List files in this directory
    dir->list_dir_begin();
    godot::String name = dir->get_next();
    while (name != godot::String()) {
        godot::String full_path = dir_path + "/" + name;
        // Normalize: ensure path starts with res://
        if (dir->current_is_dir()) {
            // Skip hidden directories (.godot, .git, etc.)
            if (!name.begins_with(".")) {
                list_files_recursive(full_path, files);
            }
        } else {
            std::string path_str(full_path.utf8().get_data());
            std::string name_str(name.utf8().get_data());
            // Determine file type from extension
            std::string ext = name_str.substr(name_str.find_last_of('.') + 1);
            files.push_back({{"path", path_str}, {"type", ext}});
        }
        name = dir->get_next();
    }
    dir->list_dir_end();
}
```

### MCP Resources Protocol (PROJ-04)
```cpp
// Source: MCP spec 2025-03-26 / 2025-06-18 -- resources/list response format
// In mcp_server.cpp handle_request():
if (method == "resources/list") {
    nlohmann::json resources = nlohmann::json::array();
    resources.push_back({
        {"uri", "godot://scene_tree"},
        {"name", "Scene Tree"},
        {"description", "Current scene tree structure with node names, types, and properties"},
        {"mimeType", "application/json"}
    });
    resources.push_back({
        {"uri", "godot://project_files"},
        {"name", "Project Files"},
        {"description", "Flat listing of all files in the project (res://)"},
        {"mimeType", "application/json"}
    });
    return mcp::create_resources_list_response(id, resources);
}

if (method == "resources/read") {
    std::string uri;
    if (params.contains("uri") && params["uri"].is_string()) {
        uri = params["uri"].get<std::string>();
    }
    if (uri == "godot://scene_tree") {
        nlohmann::json tree = get_scene_tree(); // reuse existing function
        return mcp::create_resource_read_response(id, {
            {{"uri", uri}, {"mimeType", "application/json"}, {"text", tree.dump()}}
        });
    }
    // ... similar for godot://project_files
}
```

### IO Thread Queue Pattern (MCP-04)
```cpp
// Source: C++17 standard library + Godot threading guidelines
// In mcp_server.h:
struct PendingRequest {
    std::string method;
    nlohmann::json id;
    nlohmann::json params;
};

struct PendingResponse {
    nlohmann::json response;
};

// Thread-safe queues
std::mutex queue_mutex;
std::queue<PendingRequest> request_queue;
std::queue<PendingResponse> response_queue;
std::condition_variable response_cv;
std::thread io_thread;
std::atomic<bool> running{false};

// IO thread function:
void io_thread_func() {
    while (running.load()) {
        // TCP accept/read (blocking with timeout)
        // Parse JSON-RPC
        // Push to request_queue
        // Wait on response_cv for response
        // TCP write response
    }
}

// Main thread poll() -- called from _process():
void poll() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!request_queue.empty()) {
        auto req = request_queue.front();
        request_queue.pop();
        // Execute on main thread (all Godot API calls safe here)
        auto response = handle_request(req.method, req.id, req.params);
        response_queue.push({response});
        response_cv.notify_one();
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| MCP tools only | MCP tools + resources | Spec 2025-03-26 | Resources provide passive context, tools provide active operations |
| Single-thread TCP polling | IO thread + main thread queue | Phase 3 | Prevents TCP accept blocking, enables future concurrent requests |
| scene_tools only | scene_tools + script_tools + project_tools | Phase 3 | Modular tool organization per domain |

**Note on MCP spec version:** The project targets MCP spec 2025-03-26. The newer 2025-06-18 spec adds annotations, size fields, and title to resources -- these are backward-compatible additions that can be adopted later without breaking changes.

## Open Questions

1. **edit_script: multiple operations per call?**
   - What we know: Context says Claude's discretion on this
   - What's unclear: Whether supporting batch operations (array of operations) adds value vs complexity
   - Recommendation: Start with single operation per call. Multiple operations can be done with multiple tool calls. If needed, add batch support later.

2. **EditorFileSystem update_file vs scan timing**
   - What we know: update_file() is for single files, scan() is for full rescan. update_file() is preferred.
   - What's unclear: Whether update_file() is synchronous or has a slight delay before the file browser updates
   - Recommendation: Use update_file() and trust it works. If testing reveals timing issues, add a brief comment but don't add artificial delays.

3. **resources/templates capability**
   - What we know: MCP spec supports resource templates with URI patterns. Context says Claude's discretion.
   - What's unclear: Whether AI clients (Claude Desktop, Cursor) use resource templates today
   - Recommendation: Skip resource templates for now. Two static resources (scene_tree, project_files) don't benefit from templates. Add if clients request it.

4. **ProjectSettings property enumeration**
   - What we know: ProjectSettings has get_setting() and has_setting(), but no direct "list all settings" method
   - What's unclear: How to enumerate all settings without a list method. get_property_list() may work on the singleton but needs verification.
   - Recommendation: Use Object::get_property_list() on ProjectSettings singleton. If that doesn't return project settings, fall back to reading project.godot via FileAccess as text and parsing the INI-like format.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd tests/build && ctest --output-on-failure` |
| Full suite command | `cd tests/build && cmake --build . && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SCRP-01 | read_script returns file content | unit (mock) | `ctest -R test_protocol --output-on-failure` | Wave 0 |
| SCRP-02 | write_script creates new file, errors on existing | unit (mock) | `ctest -R test_protocol --output-on-failure` | Wave 0 |
| SCRP-03 | edit_script insert/replace/delete operations | unit | `ctest -R test_script_tools --output-on-failure` | Wave 0 |
| SCRP-04 | attach/detach script to/from node | manual-only | Manual UAT -- requires running Godot editor | N/A |
| PROJ-01 | list_project_files returns flat file list | manual-only | Manual UAT -- requires res:// filesystem | N/A |
| PROJ-02 | get_project_settings returns settings | manual-only | Manual UAT -- requires Godot ProjectSettings | N/A |
| PROJ-03 | get_resource_info loads and inspects resource | manual-only | Manual UAT -- requires Godot ResourceLoader | N/A |
| PROJ-04 | MCP resources/list and resources/read handlers | unit | `ctest -R test_protocol --output-on-failure` | Wave 0 |
| MCP-04 | IO thread + queue pattern | unit | `ctest -R test_protocol --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd tests/build && ctest --output-on-failure`
- **Per wave merge:** Full suite: `cd tests/build && cmake --build . && ctest --output-on-failure`
- **Phase gate:** Full suite green + UAT script before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_script_tools.cpp` -- covers SCRP-03 line editing logic (pure C++ string manipulation, no Godot dependency)
- [ ] `tests/test_protocol.cpp` -- extend with resources/list and resources/read response format tests (PROJ-04)
- [ ] `tests/test_protocol.cpp` -- extend with new tool schema validation for Phase 3 tools
- [ ] `tests/CMakeLists.txt` -- add test_script_tools executable if new test file created

## Sources

### Primary (HIGH confidence)
- godot-cpp generated headers in `godot-cpp/gen/include/godot_cpp/classes/` -- FileAccess, DirAccess, ResourceLoader, ProjectSettings, EditorFileSystem, EditorInterface, Resource, Object method signatures verified directly
- [MCP Resources Specification (2025-06-18)](https://modelcontextprotocol.io/specification/2025-06-18/server/resources) -- resources/list, resources/read protocol, capability declaration, URI schemes
- Existing codebase: mcp_server.cpp, mcp_protocol.cpp, scene_tools.cpp, scene_mutation.cpp -- established patterns for tool modules, dispatch, protocol handling

### Secondary (MEDIUM confidence)
- [FileAccess documentation (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_fileaccess.html) -- behavior details (mode flags, auto-close)
- [EditorFileSystem documentation](https://docs.godotengine.org/en/stable/classes/class_editorfilesystem.html) -- update_file vs scan semantics
- [ProjectSettings documentation (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_projectsettings.html) -- setting paths, singleton access
- [DirAccess documentation (Godot 4.3)](https://docs.godotengine.org/en/4.3/classes/class_diraccess.html) -- directory traversal pattern
- [ResourceLoader documentation](https://docs.godotengine.org/en/stable/classes/class_resourceloader.html) -- load() for script attachment
- [MCP Lifecycle (2025-03-26)](https://modelcontextprotocol.io/specification/2025-03-26/basic/lifecycle) -- capability negotiation during initialize

### Tertiary (LOW confidence)
- [Godot Forum: Using Thread in GDExtension](https://forum.godotengine.org/t/using-thread-in-a-gdextension/73547) -- community patterns for GDExtension threading
- [GitHub Issue #46893: EditorFileSystem scan crash](https://github.com/godotengine/godot/issues/46893) -- reentrant scan danger
- [GitHub Issue #83900: Thread safety restrictions](https://github.com/godotengine/godot/issues/83900) -- "is_readable_from_caller_thread" enforcement

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All APIs verified from actual godot-cpp headers in the project
- Architecture: HIGH - Tool module pattern is established; MCP Resources spec is well-documented
- Pitfalls: HIGH - Verified against godot-cpp headers and official docs; threading pitfalls confirmed by community reports
- Threading (MCP-04): MEDIUM - C++ standard threading is well-understood, but Godot-specific thread safety constraints need runtime validation

**Research date:** 2026-03-17
**Valid until:** 2026-04-17 (stable APIs, no major changes expected)
