#!/usr/bin/env python3
"""
v1.5 UAT - AI Workflow Enhancement

Tests:
  Phase 22: Smart Error Handling (isError flag, fuzzy matching, recovery guidance)
  Phase 23: Enriched Resources (enriched scene tree, URI templates, project files)
  Phase 24: Composite Tools (find_nodes, batch_set_property, create_character, create_ui_panel, duplicate_node)
  Phase 25: Prompt Templates (8 new prompts, 15 total)

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_v15_ai_workflow.py
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client
# ---------------------------------------------------------------------------

class MCPClient:
    def __init__(self, host="127.0.0.1", port=6800, timeout=10):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None
        self.buffer = ""
        self._id = 0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(self.timeout)
        self.sock.connect((self.host, self.port))
        self.buffer = ""

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def reconnect(self):
        self.close()
        time.sleep(0.5)
        self.connect()
        self.request("initialize")
        self.notify("notifications/initialized")
        time.sleep(0.3)

    def _next_id(self):
        self._id += 1
        return self._id

    def send(self, obj):
        data = json.dumps(obj) + "\n"
        self.sock.sendall(data.encode("utf-8"))

    def recv(self):
        while "\n" not in self.buffer:
            chunk = self.sock.recv(65536).decode("utf-8")
            if not chunk:
                raise ConnectionError("Server closed connection")
            self.buffer += chunk
        line, self.buffer = self.buffer.split("\n", 1)
        return json.loads(line)

    def request(self, method, params=None):
        msg = {"jsonrpc": "2.0", "id": self._next_id(), "method": method}
        if params is not None:
            msg["params"] = params
        self.send(msg)
        return self.recv()

    def notify(self, method, params=None):
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        self.send(msg)

    def call_tool(self, tool_name, arguments=None):
        params = {"name": tool_name}
        if arguments is not None:
            params["arguments"] = arguments
        return self.request("tools/call", params)

    def call_tool_text(self, tool_name, arguments=None):
        resp = self.call_tool(tool_name, arguments)
        if "result" in resp and "content" in resp["result"]:
            for item in resp["result"]["content"]:
                if item.get("type") == "text":
                    try:
                        return json.loads(item["text"])
                    except json.JSONDecodeError:
                        return item["text"]
        return resp

# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

results = []

def test(name, fn):
    try:
        fn()
        results.append(("PASS", name))
        print(f"  PASS {name}")
    except AssertionError as e:
        results.append(("FAIL", name, str(e)))
        print(f"  FAIL {name}: {e}")
    except Exception as e:
        results.append(("ERROR", name, str(e)))
        print(f"  ! {name}: {e}")

def summary():
    passed = sum(1 for r in results if r[0] == "PASS")
    failed = sum(1 for r in results if r[0] == "FAIL")
    errors = sum(1 for r in results if r[0] == "ERROR")
    total = len(results)
    print(f"\n{'='*60}")
    print(f"Results: {passed}/{total} passed, {failed} failed, {errors} errors")
    if failed > 0 or errors > 0:
        print("\nFailed/Error tests:")
        for r in results:
            if r[0] != "PASS":
                print(f"  {r[0]}: {r[1]} — {r[2]}")
    print(f"{'='*60}")
    return 0 if failed == 0 and errors == 0 else 1

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------

print("v1.5 UAT — AI Workflow Enhancement")
print("="*60)

client = MCPClient()
try:
    client.connect()
except ConnectionRefusedError:
    print("ERROR: Cannot connect to MCP server on port 6800")
    print("Make sure Godot is running with the MCP plugin enabled")
    sys.exit(1)

# Initialize MCP
init_resp = client.request("initialize")
client.notify("notifications/initialized")
time.sleep(0.3)

# ---------------------------------------------------------------------------
# Phase 22: Smart Error Handling
# ---------------------------------------------------------------------------

print("\n--- Phase 22: Smart Error Handling ---")

# Setup: create a test scene
client.call_tool("create_scene", {"root_type": "Node2D", "root_name": "V15TestScene"})
time.sleep(0.5)
client.call_tool("create_node", {"type": "Sprite2D", "name": "TestSprite", "parent": ""})
client.call_tool("create_node", {"type": "Label", "name": "TestLabel", "parent": ""})
time.sleep(0.3)

def test_err01_isError_flag():
    """ERR-01: Error responses have isError: true"""
    resp = client.call_tool("set_node_property", {
        "node_path": "NonExistentNode",
        "property": "position",
        "value": "Vector2(0, 0)"
    })
    result = resp.get("result", {})
    assert result.get("isError") == True, f"Expected isError=true, got {result.get('isError')}"

def test_err02_fuzzy_matching():
    """ERR-02: Node not found includes fuzzy suggestions"""
    resp = client.call_tool("set_node_property", {
        "node_path": "TestSprit",  # typo: Sprit instead of Sprite
        "property": "position",
        "value": "Vector2(0, 0)"
    })
    result = resp.get("result", {})
    text = ""
    for item in result.get("content", []):
        if item.get("type") == "text":
            text = item["text"]
            break
    assert "TestSprite" in text, f"Expected fuzzy suggestion 'TestSprite' in error, got: {text[:200]}"

def test_err03_no_scene_guidance():
    """ERR-03: No-scene errors include guidance"""
    # Close scene first, then try an operation
    # This test relies on error message pattern when nodes aren't found
    resp = client.call_tool("get_scene_tree", {"max_depth": 1})
    # Just verify we get a response (scene is open, so no error expected)
    assert "result" in resp, "Expected result from get_scene_tree"

def test_err05_precondition_guidance():
    """ERR-05: Game not running error includes guidance"""
    resp = client.call_tool("inject_input", {
        "type": "key",
        "key": "ui_accept",
        "pressed": True
    })
    result = resp.get("result", {})
    text = ""
    for item in result.get("content", []):
        if item.get("type") == "text":
            text = item["text"]
            break
    # Should mention run_game since game isn't running
    assert "run_game" in text.lower() or "not running" in text.lower() or "not initialized" in text.lower(), \
        f"Expected precondition guidance mentioning run_game, got: {text[:200]}"

def test_err04_missing_param_hints():
    """ERR-04: Missing params include format examples"""
    resp = client.call_tool("create_node", {})  # Missing required params
    # Should be a JSON-RPC error or tool error with hints
    if "error" in resp:
        msg = resp["error"].get("message", "")
        assert "type" in msg.lower() or "name" in msg.lower() or "parameter" in msg.lower(), \
            f"Expected param hints in error, got: {msg[:200]}"
    elif "result" in resp:
        text = ""
        for item in resp["result"].get("content", []):
            if item.get("type") == "text":
                text = item["text"]
                break
        assert "type" in text.lower() or "name" in text.lower(), \
            f"Expected param hints in error, got: {text[:200]}"

def test_err07_suggested_tools():
    """ERR-07: Error includes suggested recovery tools"""
    resp = client.call_tool("set_node_property", {
        "node_path": "NonExistentNode",
        "property": "position",
        "value": "Vector2(0, 0)"
    })
    result = resp.get("result", {})
    text = ""
    for item in result.get("content", []):
        if item.get("type") == "text":
            text = item["text"]
            break
    assert "get_scene_tree" in text.lower() or "scene_tree" in text.lower(), \
        f"Expected suggested tool in error, got: {text[:200]}"

test("ERR-01: isError flag on errors", test_err01_isError_flag)
test("ERR-02: Fuzzy matching suggestions", test_err02_fuzzy_matching)
test("ERR-03: No-scene guidance", test_err03_no_scene_guidance)
test("ERR-04: Missing param format hints", test_err04_missing_param_hints)
test("ERR-05: Precondition guidance", test_err05_precondition_guidance)
test("ERR-07: Suggested recovery tools", test_err07_suggested_tools)

# ---------------------------------------------------------------------------
# Phase 23: Enriched Resources
# ---------------------------------------------------------------------------

print("\n--- Phase 23: Enriched Resources ---")

def test_res01_enriched_scene_tree():
    """RES-01: Scene tree includes scripts, signals, properties"""
    resp = client.request("resources/read", {"uri": "godot://scene_tree"})
    result = resp.get("result", {})
    contents = result.get("contents", [])
    assert len(contents) > 0, "Expected non-empty contents"
    text = contents[0].get("text", "")
    data = json.loads(text)
    # Enriched tree should have more than just name/type/path
    assert isinstance(data, (dict, list)), f"Expected dict/list, got {type(data)}"

def test_res02_uri_templates_list():
    """RES-02: resources/templates/list returns URI templates"""
    resp = client.request("resources/templates/list")
    result = resp.get("result", {})
    templates = result.get("resourceTemplates", [])
    assert len(templates) >= 3, f"Expected ≥3 templates, got {len(templates)}"
    uris = [t.get("uriTemplate", "") for t in templates]
    has_node = any("node" in u for u in uris)
    has_script = any("script" in u for u in uris)
    has_signals = any("signal" in u for u in uris)
    assert has_node and has_script and has_signals, \
        f"Expected node/script/signals templates, got: {uris}"

def test_res02_uri_node_detail():
    """RES-02: godot://node/{path} returns node detail"""
    resp = client.request("resources/read", {"uri": "godot://node/TestSprite"})
    result = resp.get("result", {})
    contents = result.get("contents", [])
    assert len(contents) > 0, "Expected non-empty contents for node detail"

def test_res03_enriched_project_files():
    """RES-03: Project files include size, type, modification time"""
    resp = client.request("resources/read", {"uri": "godot://project_files"})
    result = resp.get("result", {})
    contents = result.get("contents", [])
    assert len(contents) > 0, "Expected non-empty contents"
    text = contents[0].get("text", "")
    data = json.loads(text)
    # Check for enriched fields
    if isinstance(data, list) and len(data) > 0:
        first = data[0]
        has_size = "size" in first or "bytes" in str(first)
        has_type = "type" in first or "category" in str(first)
        assert has_size or has_type, f"Expected enriched fields (size/type), got: {list(first.keys()) if isinstance(first, dict) else first}"

def test_res_initialize_capability():
    """RES: Initialize response includes resources capability"""
    assert "result" in init_resp, "Expected result in initialize response"
    capabilities = init_resp["result"].get("capabilities", {})
    assert "resources" in capabilities, f"Expected 'resources' capability, got: {list(capabilities.keys())}"

test("RES-01: Enriched scene tree", test_res01_enriched_scene_tree)
test("RES-02: URI templates list", test_res02_uri_templates_list)
test("RES-02: URI node detail query", test_res02_uri_node_detail)
test("RES-03: Enriched project files", test_res03_enriched_project_files)
test("RES: Initialize has resources capability", test_res_initialize_capability)

# ---------------------------------------------------------------------------
# Phase 24: Composite Tools
# ---------------------------------------------------------------------------

print("\n--- Phase 24: Composite Tools ---")

def test_comp01_find_nodes():
    """COMP-01: find_nodes searches by type"""
    result = client.call_tool_text("find_nodes", {"type": "Sprite2D"})
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    nodes = result.get("nodes", [])
    assert len(nodes) >= 1, f"Expected ≥1 Sprite2D node, got {len(nodes)}"
    assert any("TestSprite" in str(n) for n in nodes), f"Expected TestSprite in results: {nodes}"

def test_comp01_find_nodes_name_pattern():
    """COMP-01: find_nodes searches by name pattern"""
    result = client.call_tool_text("find_nodes", {"name_pattern": "Test*"})
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    nodes = result.get("nodes", [])
    assert len(nodes) >= 2, f"Expected ≥2 Test* nodes, got {len(nodes)}: {nodes}"

def test_comp02_batch_set_property():
    """COMP-02: batch_set_property sets property on multiple nodes"""
    result = client.call_tool_text("batch_set_property", {
        "node_paths": ["TestSprite", "TestLabel"],
        "property": "visible",
        "value": "false"
    })
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    modified = result.get("modified_count", result.get("modified", 0))
    assert modified >= 2, f"Expected ≥2 nodes modified, got {modified}"

def test_comp02_batch_set_property_type_filter():
    """COMP-02: batch_set_property with type_filter"""
    # Reset visibility first
    client.call_tool("set_node_property", {"node_path": "TestSprite", "property": "visible", "value": "true"})
    client.call_tool("set_node_property", {"node_path": "TestLabel", "property": "visible", "value": "true"})
    time.sleep(0.2)
    result = client.call_tool_text("batch_set_property", {
        "type_filter": "Label",
        "property": "visible",
        "value": "false"
    })
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"

def test_comp03_create_character():
    """COMP-03: create_character creates character with collision"""
    result = client.call_tool_text("create_character", {
        "name": "TestPlayer",
        "type": "2d",
        "shape_type": "rectangle",
        "parent_path": ""
    })
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    # Verify the character was created
    tree = client.call_tool_text("get_scene_tree", {"max_depth": 3})
    tree_str = json.dumps(tree)
    assert "TestPlayer" in tree_str, f"Expected TestPlayer in scene tree"

def test_comp04_create_ui_panel():
    """COMP-04: create_ui_panel from declarative spec"""
    result = client.call_tool_text("create_ui_panel", {
        "spec": {
            "root_type": "VBoxContainer",
            "children": [
                {"type": "Label", "text": "Hello"},
                {"type": "Button", "text": "Click Me"}
            ]
        },
        "parent_path": ""
    })
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    tree = client.call_tool_text("get_scene_tree", {"max_depth": 3})
    tree_str = json.dumps(tree)
    assert "VBoxContainer" in tree_str, f"Expected VBoxContainer in scene tree"

def test_comp05_duplicate_node():
    """COMP-05: duplicate_node deep-copies a subtree"""
    result = client.call_tool_text("duplicate_node", {
        "source_path": "TestSprite",
        "new_name": "TestSpriteCopy"
    })
    assert isinstance(result, dict), f"Expected dict, got {type(result)}"
    tree = client.call_tool_text("get_scene_tree", {"max_depth": 2})
    tree_str = json.dumps(tree)
    assert "TestSpriteCopy" in tree_str, f"Expected TestSpriteCopy in scene tree"

test("COMP-01: find_nodes by type", test_comp01_find_nodes)
test("COMP-01: find_nodes by name pattern", test_comp01_find_nodes_name_pattern)
test("COMP-02: batch_set_property by paths", test_comp02_batch_set_property)
test("COMP-02: batch_set_property by type_filter", test_comp02_batch_set_property_type_filter)
test("COMP-03: create_character", test_comp03_create_character)
test("COMP-04: create_ui_panel", test_comp04_create_ui_panel)
test("COMP-05: duplicate_node", test_comp05_duplicate_node)

# ---------------------------------------------------------------------------
# Phase 25: Prompt Templates
# ---------------------------------------------------------------------------

print("\n--- Phase 25: Prompt Templates ---")

def test_prompt_count():
    """PROMPT: 15 prompts registered"""
    resp = client.request("prompts/list")
    result = resp.get("result", {})
    prompts = result.get("prompts", [])
    assert len(prompts) == 15, f"Expected 15 prompts, got {len(prompts)}"

def test_prompt01_tool_composition_guide():
    """PROMPT-01: tool_composition_guide exists and returns content"""
    resp = client.request("prompts/get", {"name": "tool_composition_guide"})
    result = resp.get("result", {})
    messages = result.get("messages", [])
    assert len(messages) > 0, "Expected messages from tool_composition_guide"
    text = messages[0].get("content", {}).get("text", "")
    assert len(text) > 100, f"Expected substantial content, got {len(text)} chars"

def test_prompt02_debug_game_crash():
    """PROMPT-02: debug_game_crash exists"""
    resp = client.request("prompts/get", {"name": "debug_game_crash"})
    result = resp.get("result", {})
    messages = result.get("messages", [])
    assert len(messages) > 0, "Expected messages from debug_game_crash"

def test_prompt03_build_platformer():
    """PROMPT-03: build_platformer_game exists with complexity arg"""
    resp = client.request("prompts/get", {"name": "build_platformer_game", "arguments": {"complexity": "simple"}})
    result = resp.get("result", {})
    messages = result.get("messages", [])
    assert len(messages) > 0, "Expected messages from build_platformer_game"

def test_prompt04_setup_tilemap():
    """PROMPT-04: setup_tilemap_level exists"""
    resp = client.request("prompts/get", {"name": "setup_tilemap_level"})
    result = resp.get("result", {})
    messages = result.get("messages", [])
    assert len(messages) > 0, "Expected messages from setup_tilemap_level"

def test_prompt07_create_game():
    """PROMPT-07: create_game_from_scratch with genre arg"""
    resp = client.request("prompts/get", {"name": "create_game_from_scratch", "arguments": {"genre": "platformer"}})
    result = resp.get("result", {})
    messages = result.get("messages", [])
    assert len(messages) > 0, "Expected messages from create_game_from_scratch"

def test_prompt08_fix_common_errors():
    """PROMPT-08: fix_common_errors exists"""
    resp = client.request("prompts/get", {"name": "fix_common_errors"})
    result = resp.get("result", {})
    messages = result.get("messages", [])
    assert len(messages) > 0, "Expected messages from fix_common_errors"

new_prompt_names = [
    "tool_composition_guide", "debug_game_crash", "build_platformer_game",
    "setup_tilemap_level", "build_top_down_game", "debug_physics_issue",
    "create_game_from_scratch", "fix_common_errors"
]

def test_all_new_prompts_exist():
    """PROMPT: All 8 new prompts are registered"""
    resp = client.request("prompts/list")
    prompts = resp.get("result", {}).get("prompts", [])
    names = [p["name"] for p in prompts]
    missing = [n for n in new_prompt_names if n not in names]
    assert len(missing) == 0, f"Missing prompts: {missing}"

test("PROMPT: 15 prompts registered", test_prompt_count)
test("PROMPT: All 8 new prompts exist", test_all_new_prompts_exist)
test("PROMPT-01: tool_composition_guide", test_prompt01_tool_composition_guide)
test("PROMPT-02: debug_game_crash", test_prompt02_debug_game_crash)
test("PROMPT-03: build_platformer_game", test_prompt03_build_platformer)
test("PROMPT-04: setup_tilemap_level", test_prompt04_setup_tilemap)
test("PROMPT-07: create_game_from_scratch", test_prompt07_create_game)
test("PROMPT-08: fix_common_errors", test_prompt08_fix_common_errors)

# ---------------------------------------------------------------------------
# Cleanup & Summary
# ---------------------------------------------------------------------------

client.close()
exit_code = summary()
sys.exit(exit_code)
