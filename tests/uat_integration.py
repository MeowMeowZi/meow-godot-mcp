#!/usr/bin/env python3
"""
Integration UAT - Comprehensive end-to-end tests for MCP tool collaboration.

Tests a full workflow: scene creation -> node hierarchy -> properties ->
scripts -> signals -> UI styling -> scene tree inspection -> save -> cleanup.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_integration.py

Covers tools:
  - create_scene, get_scene_tree
  - create_node, delete_node, set_node_property
  - write_script, attach_script, detach_script, read_script
  - connect_signal, get_node_signals
  - set_layout_preset, set_theme_override, get_ui_properties
  - save_scene, list_open_scenes
  - list_project_files, get_project_settings
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

    def _next_id(self):
        self._id += 1
        return self._id

    def send(self, obj):
        data = json.dumps(obj) + "\n"
        self.sock.sendall(data.encode("utf-8"))

    def recv(self, timeout=None):
        old_timeout = self.sock.gettimeout()
        if timeout is not None:
            self.sock.settimeout(timeout)
        try:
            while "\n" not in self.buffer:
                chunk = self.sock.recv(65536).decode("utf-8")
                if not chunk:
                    raise ConnectionError("Server closed connection")
                self.buffer += chunk
            line, self.buffer = self.buffer.split("\n", 1)
            return json.loads(line)
        finally:
            self.sock.settimeout(old_timeout)

    def request(self, method, params=None, timeout=None):
        msg = {"jsonrpc": "2.0", "id": self._next_id(), "method": method}
        if params is not None:
            msg["params"] = params
        self.send(msg)
        return self.recv(timeout=timeout)

    def notify(self, method, params=None):
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        self.send(msg)


# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"
SKIP = "\033[93mSKIP\033[0m"

results = []


def report(num, name, passed, detail=""):
    tag = PASS if passed else FAIL
    results.append((num, name, passed))
    print(f"  [{tag}] Test {num}: {name}")
    if detail:
        for line in detail.strip().split("\n"):
            print(f"         {line}")


def call_tool_text(client, tool_name, arguments=None, timeout=None):
    """Call a tool and parse text content as JSON. Returns (parsed_json, resp)."""
    if arguments is None:
        arguments = {}
    resp = client.request("tools/call", {"name": tool_name, "arguments": arguments},
                          timeout=timeout)
    result = resp.get("result", {})
    content_list = result.get("content", [])
    if content_list and "text" in content_list[0]:
        return json.loads(content_list[0]["text"]), resp
    return result, resp


def get_tool_text_raw(resp):
    """Extract raw text from a tools/call response."""
    result = resp.get("result", {})
    content_list = result.get("content", [])
    if content_list and "text" in content_list[0]:
        return content_list[0]["text"]
    return str(result)


def is_success(data):
    return isinstance(data, dict) and data.get("success") == True


def is_error_resp(resp):
    return resp.get("result", {}).get("isError", False)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def run_tests():
    client = MCPClient()

    print("\n" + "=" * 70)
    print("  MCP Meow - Comprehensive Integration Test")
    print("=" * 70)

    print("\n  Connecting to Godot MCP server (127.0.0.1:6800)...")
    try:
        client.connect()
    except Exception as e:
        print(f"\n  Connection failed: {e}")
        print("  Please ensure the Godot editor is open with the MCP Meow plugin enabled.")
        sys.exit(1)

    # --- Handshake ---
    print("  Performing MCP handshake...")
    init_resp = client.request("initialize")
    server_info = init_resp["result"]["serverInfo"]
    assert server_info["name"] == "meow-godot-mcp"
    client.notify("notifications/initialized")
    time.sleep(0.3)
    version = server_info["version"]
    print(f"  Handshake OK: meow-godot-mcp v{version}")

    # --- Protocol: tools/list ---
    tools_resp = client.request("tools/list")
    tool_count = len(tools_resp.get("result", {}).get("tools", []))
    print(f"  Tools available: {tool_count}")

    # --- Protocol: prompts/list ---
    prompts_resp = client.request("prompts/list")
    prompt_count = len(prompts_resp.get("result", {}).get("prompts", []))
    print(f"  Prompts available: {prompt_count}")

    # --- Protocol: resources/list ---
    resources_resp = client.request("resources/list")
    resource_count = len(resources_resp.get("result", {}).get("resources", []))
    print(f"  Resources available: {resource_count}")
    print()

    # ==================================================================
    # Section 1: Scene Creation & Node Hierarchy
    # ==================================================================
    print("  " + "-" * 50)
    print("  Section 1: Scene Creation & Node Hierarchy")
    print("  " + "-" * 50 + "\n")

    # Test 1: create_scene with Node2D root (always creates fresh scene)
    try:
        data, resp = call_tool_text(client, "create_scene", {
            "root_type": "Node2D",
            "path": "res://test_integration.tscn",
            "root_name": "IntegrationRoot"
        })
        ok = is_success(data) and not is_error_resp(resp)
        report(1, "create_scene with Node2D root", ok,
               f"Response: {json.dumps(data)[:200]}")
    except Exception as e:
        report(1, "create_scene with Node2D root", False, f"Error: {e}")

    time.sleep(0.5)

    # Cleanup: delete leftover nodes from previous runs (idempotency)
    for leftover in ["Player", "Player2", "ScoreLabel", "ScoreLabel2", "UIPanel"]:
        try:
            call_tool_text(client, "delete_node", {"node_path": leftover})
        except:
            pass

    # Test 2: create_node - Sprite2D child
    try:
        data, resp = call_tool_text(client, "create_node", {
            "type": "Sprite2D",
            "parent_path": "",
            "name": "Player"
        })
        ok = is_success(data) and data.get("path") == "Player"
        report(2, "create_node Sprite2D as child of root", ok,
               f"path: {data.get('path')}")
    except Exception as e:
        report(2, "create_node Sprite2D as child of root", False, f"Error: {e}")

    # Test 3: create_node - nested hierarchy (Area2D under Player)
    try:
        data, resp = call_tool_text(client, "create_node", {
            "type": "Area2D",
            "parent_path": "Player",
            "name": "HitBox"
        })
        ok = is_success(data) and data.get("path") == "Player/HitBox"
        report(3, "create_node Area2D nested under Player", ok,
               f"path: {data.get('path')}")
    except Exception as e:
        report(3, "create_node Area2D nested under Player", False, f"Error: {e}")

    # Test 4: create_node with initial properties
    try:
        data, resp = call_tool_text(client, "create_node", {
            "type": "Label",
            "parent_path": "",
            "name": "ScoreLabel",
            "properties": {
                "text": "Score: 0",
                "visible": "true"
            }
        })
        ok = is_success(data) and data.get("path") == "ScoreLabel"
        report(4, "create_node Label with initial properties", ok,
               f"path: {data.get('path')}")
    except Exception as e:
        report(4, "create_node Label with initial properties", False, f"Error: {e}")

    # Test 5: create_node with invalid type returns error
    try:
        data, resp = call_tool_text(client, "create_node", {
            "type": "NonExistentType",
            "parent_path": "",
            "name": "BadNode"
        })
        has_error = "error" in data if isinstance(data, dict) else False
        report(5, "create_node with invalid type returns error", has_error,
               f"error: {data.get('error', 'none')}")
    except Exception as e:
        report(5, "create_node with invalid type returns error", False, f"Error: {e}")

    # ==================================================================
    # Section 2: Property Operations
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 2: Property Operations")
    print("  " + "-" * 50 + "\n")

    # Test 6: set_node_property - Vector2 position
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": "Player",
            "property": "position",
            "value": "Vector2(100, 200)"
        })
        ok = is_success(data)
        report(6, "set_node_property Vector2 position", ok,
               f"Response: {json.dumps(data)[:200]}")
    except Exception as e:
        report(6, "set_node_property Vector2 position", False, f"Error: {e}")

    # Test 7: set_node_property - float scale
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": "Player",
            "property": "scale",
            "value": "Vector2(2.0, 2.0)"
        })
        ok = is_success(data)
        report(7, "set_node_property Vector2 scale", ok)
    except Exception as e:
        report(7, "set_node_property Vector2 scale", False, f"Error: {e}")

    # Test 8: set_node_property - Color modulate
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": "Player",
            "property": "modulate",
            "value": "#ff0000"
        })
        ok = is_success(data)
        report(8, "set_node_property hex color modulate", ok)
    except Exception as e:
        report(8, "set_node_property hex color modulate", False, f"Error: {e}")

    # Test 9: set_node_property - boolean
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": "Player",
            "property": "visible",
            "value": "false"
        })
        ok = is_success(data)
        report(9, "set_node_property boolean visible=false", ok)
    except Exception as e:
        report(9, "set_node_property boolean visible=false", False, f"Error: {e}")

    # Test 10: set_node_property on nonexistent node returns error
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": "NonExistentNode",
            "property": "visible",
            "value": "true"
        })
        has_error = "error" in data if isinstance(data, dict) else False
        report(10, "set_node_property on nonexistent node returns error", has_error,
               f"error: {data.get('error', 'none')}")
    except Exception as e:
        report(10, "set_node_property on nonexistent node returns error", False, f"Error: {e}")

    # ==================================================================
    # Section 3: Scene Tree Inspection
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 3: Scene Tree Inspection")
    print("  " + "-" * 50 + "\n")

    # Test 11: get_scene_tree returns full hierarchy
    try:
        data, resp = call_tool_text(client, "get_scene_tree", {})
        ok = isinstance(data, dict) and "name" in data
        child_count = len(data.get("children", [])) if isinstance(data, dict) else 0
        report(11, "get_scene_tree returns hierarchy with children", ok and child_count >= 2,
               f"root: {data.get('name')}, children: {child_count}")
    except Exception as e:
        report(11, "get_scene_tree returns hierarchy with children", False, f"Error: {e}")

    # Test 12: get_scene_tree with max_depth=1
    try:
        data, resp = call_tool_text(client, "get_scene_tree", {"max_depth": 1})
        ok = isinstance(data, dict) and "name" in data
        # At depth 1, Player's children (HitBox) should not appear
        children = data.get("children", []) if isinstance(data, dict) else []
        player_node = next((c for c in children if c.get("name") == "Player"), None)
        # Player should exist but HitBox should not (depth limited)
        has_player = player_node is not None
        no_hitbox_nested = player_node.get("children", []) == [] if player_node else True
        report(12, "get_scene_tree with max_depth=1 limits depth", ok and has_player and no_hitbox_nested,
               f"has_player: {has_player}, hitbox_hidden: {no_hitbox_nested}")
    except Exception as e:
        report(12, "get_scene_tree with max_depth=1 limits depth", False, f"Error: {e}")

    # ==================================================================
    # Section 4: Script Operations
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 4: Script Operations")
    print("  " + "-" * 50 + "\n")

    test_script = '''extends Sprite2D

var speed := 200.0
var health := 100

func _ready():
\tprint("Player ready!")

func _process(delta):
\tposition.x += speed * delta

func take_damage(amount: int) -> void:
\thealth -= amount
\tif health <= 0:
\t\tqueue_free()
'''

    # Test 13: write_script (use unique path to avoid "already exists" on re-run)
    test_script_path = "res://test_player_integration.gd"
    try:
        data, resp = call_tool_text(client, "write_script", {
            "path": test_script_path,
            "content": test_script
        })
        # Accept both fresh create and overwrite
        ok = is_success(data) or ("already exists" not in str(data.get("error", "")))
        if not ok:
            # File exists from previous run - use edit_script to overwrite
            data, resp = call_tool_text(client, "edit_script", {
                "path": test_script_path,
                "operation": "replace",
                "line": 1,
                "end_line": 999,
                "content": test_script
            })
            ok = is_success(data)
        report(13, "write_script creates GDScript file", ok,
               f"Response: {json.dumps(data)[:200]}")
    except Exception as e:
        report(13, "write_script creates GDScript file", False, f"Error: {e}")

    time.sleep(0.3)

    # Test 14: read_script
    try:
        data, resp = call_tool_text(client, "read_script", {
            "path": test_script_path
        })
        ok = is_success(data) and "content" in data
        has_extends = "extends Sprite2D" in data.get("content", "") if isinstance(data, dict) else False
        report(14, "read_script returns written content", ok and has_extends,
               f"has_extends: {has_extends}, lines: {data.get('line_count', 0)}")
    except Exception as e:
        report(14, "read_script returns written content", False, f"Error: {e}")

    # Test 15: attach_script
    try:
        data, resp = call_tool_text(client, "attach_script", {
            "node_path": "Player",
            "script_path": test_script_path
        })
        ok = is_success(data)
        report(15, "attach_script to Player node", ok,
               f"Response: {json.dumps(data)[:200]}")
    except Exception as e:
        report(15, "attach_script to Player node", False, f"Error: {e}")

    # Test 16: detach_script
    try:
        data, resp = call_tool_text(client, "detach_script", {
            "node_path": "Player"
        })
        ok = is_success(data)
        report(16, "detach_script from Player node", ok)
    except Exception as e:
        report(16, "detach_script from Player node", False, f"Error: {e}")

    # Test 17: re-attach for later tests
    try:
        data, resp = call_tool_text(client, "attach_script", {
            "node_path": "Player",
            "script_path": test_script_path
        })
        ok = is_success(data)
        report(17, "re-attach_script for subsequent tests", ok)
    except Exception as e:
        report(17, "re-attach_script for subsequent tests", False, f"Error: {e}")

    # ==================================================================
    # Section 5: Signal Operations
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 5: Signal Operations")
    print("  " + "-" * 50 + "\n")

    # Test 18: get_node_signals
    try:
        data, resp = call_tool_text(client, "get_node_signals", {
            "node_path": "Player"
        })
        ok = isinstance(data, dict) and ("signals" in data or "success" in data)
        signal_count = len(data.get("signals", [])) if isinstance(data, dict) else 0
        report(18, "get_node_signals lists signals on Player", ok and signal_count > 0,
               f"signal_count: {signal_count}")
    except Exception as e:
        report(18, "get_node_signals lists signals on Player", False, f"Error: {e}")

    # Test 19: connect_signal (visibility_changed -> ScoreLabel.show)
    try:
        data, resp = call_tool_text(client, "connect_signal", {
            "source_path": "Player",
            "signal_name": "visibility_changed",
            "target_path": "ScoreLabel",
            "method_name": "show"
        })
        ok = is_success(data)
        report(19, "connect_signal Player.visibility_changed -> ScoreLabel.show", ok,
               f"Response: {json.dumps(data)[:200]}")
    except Exception as e:
        report(19, "connect_signal Player.visibility_changed -> ScoreLabel.show", False, f"Error: {e}")

    # Test 20: disconnect_signal
    try:
        data, resp = call_tool_text(client, "disconnect_signal", {
            "source_path": "Player",
            "signal_name": "visibility_changed",
            "target_path": "ScoreLabel",
            "method_name": "show"
        })
        ok = is_success(data)
        report(20, "disconnect_signal successfully", ok)
    except Exception as e:
        report(20, "disconnect_signal successfully", False, f"Error: {e}")

    # ==================================================================
    # Section 6: UI Tools
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 6: UI Tools (Control nodes)")
    print("  " + "-" * 50 + "\n")

    # Create a Control node for UI tests
    try:
        call_tool_text(client, "create_node", {
            "type": "PanelContainer",
            "parent_path": "",
            "name": "UIPanel"
        })
        call_tool_text(client, "create_node", {
            "type": "Label",
            "parent_path": "UIPanel",
            "name": "Title",
            "properties": {"text": "Integration Test"}
        })
        time.sleep(0.2)
    except:
        pass

    # Test 21: set_layout_preset
    try:
        data, resp = call_tool_text(client, "set_layout_preset", {
            "node_path": "UIPanel",
            "preset": "center"
        })
        ok = is_success(data)
        report(21, "set_layout_preset center on UIPanel", ok)
    except Exception as e:
        report(21, "set_layout_preset center on UIPanel", False, f"Error: {e}")

    # Test 22: set_theme_override - font_size
    try:
        data, resp = call_tool_text(client, "set_theme_override", {
            "node_path": "UIPanel/Title",
            "overrides": {
                "font_size": "24"
            }
        })
        ok = is_success(data)
        report(22, "set_theme_override font_size on Title", ok)
    except Exception as e:
        report(22, "set_theme_override font_size on Title", False, f"Error: {e}")

    # Test 23: get_ui_properties
    try:
        data, resp = call_tool_text(client, "get_ui_properties", {
            "node_path": "UIPanel"
        })
        ok = isinstance(data, dict) and not is_error_resp(resp)
        has_props = len(data) > 0 if isinstance(data, dict) else False
        report(23, "get_ui_properties returns UI property data", ok and has_props,
               f"keys: {list(data.keys())[:6] if isinstance(data, dict) else 'N/A'}")
    except Exception as e:
        report(23, "get_ui_properties returns anchor data", False, f"Error: {e}")

    # ==================================================================
    # Section 7: Scene Save & Project Info
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 7: Scene Save & Project Info")
    print("  " + "-" * 50 + "\n")

    # Test 24: save_scene
    try:
        data, resp = call_tool_text(client, "save_scene", {
            "path": "res://test_integration.tscn"
        })
        ok = is_success(data)
        report(24, "save_scene writes .tscn to disk", ok,
               f"Response: {json.dumps(data)[:200]}")
    except Exception as e:
        report(24, "save_scene writes .tscn to disk", False, f"Error: {e}")

    # Test 25: list_open_scenes
    try:
        data, resp = call_tool_text(client, "list_open_scenes", {})
        ok = isinstance(data, dict) and "scenes" in data
        scene_count = len(data.get("scenes", [])) if isinstance(data, dict) else 0
        report(25, "list_open_scenes shows current scenes", ok and scene_count > 0,
               f"open_scenes: {scene_count}")
    except Exception as e:
        report(25, "list_open_scenes shows current scenes", False, f"Error: {e}")

    # Test 26: list_project_files
    try:
        data, resp = call_tool_text(client, "list_project_files", {})
        ok = is_success(data) and "files" in data
        file_count = data.get("count", 0)
        report(26, "list_project_files returns file listing", ok and file_count > 0,
               f"file_count: {file_count}")
    except Exception as e:
        report(26, "list_project_files returns file listing", False, f"Error: {e}")

    # Test 27: get_project_settings
    try:
        data, resp = call_tool_text(client, "get_project_settings", {})
        ok = is_success(data) and "settings" in data
        settings_count = len(data.get("settings", {}))
        report(27, "get_project_settings returns settings", ok and settings_count > 0,
               f"settings_count: {settings_count}")
    except Exception as e:
        report(27, "get_project_settings returns settings", False, f"Error: {e}")

    # ==================================================================
    # Section 8: Delete & Cleanup
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 8: Delete & Cleanup")
    print("  " + "-" * 50 + "\n")

    # Test 28: delete_node
    try:
        data, resp = call_tool_text(client, "delete_node", {
            "node_path": "Player/HitBox"
        })
        ok = is_success(data)
        report(28, "delete_node removes HitBox from Player", ok)
    except Exception as e:
        report(28, "delete_node removes HitBox from Player", False, f"Error: {e}")

    # Test 29: verify deletion via scene tree
    try:
        data, resp = call_tool_text(client, "get_scene_tree", {})
        children = data.get("children", []) if isinstance(data, dict) else []
        player = next((c for c in children if c.get("name") == "Player"), None)
        player_children = player.get("children", []) if player else []
        hitbox_gone = not any(c.get("name") == "HitBox" for c in player_children)
        report(29, "get_scene_tree confirms HitBox deleted", hitbox_gone,
               f"player_children: {[c.get('name') for c in player_children]}")
    except Exception as e:
        report(29, "get_scene_tree confirms HitBox deleted", False, f"Error: {e}")

    # Test 30: delete_node on root returns error
    try:
        data, resp = call_tool_text(client, "delete_node", {
            "node_path": ""
        })
        has_error = "error" in data if isinstance(data, dict) else False
        report(30, "delete_node on root returns error", has_error,
               f"error: {data.get('error', 'none')}")
    except Exception as e:
        report(30, "delete_node on root returns error", False, f"Error: {e}")

    # ==================================================================
    # Section 9: Edit Script
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 9: Edit Script")
    print("  " + "-" * 50 + "\n")

    # Test 31: edit_script - insert line
    try:
        data, resp = call_tool_text(client, "edit_script", {
            "path": test_script_path,
            "operation": "insert",
            "line": 4,
            "content": "var score := 0"
        })
        ok = is_success(data)
        report(31, "edit_script insert new var line", ok)
    except Exception as e:
        report(31, "edit_script insert new var line", False, f"Error: {e}")

    # Test 32: read_script to verify edit
    try:
        data, resp = call_tool_text(client, "read_script", {
            "path": test_script_path
        })
        content = data.get("content", "") if isinstance(data, dict) else ""
        has_score = "var score := 0" in content
        report(32, "read_script confirms inserted line", has_score,
               f"has 'var score': {has_score}")
    except Exception as e:
        report(32, "read_script confirms inserted line", False, f"Error: {e}")

    # ==================================================================
    # Section 10: MCP Protocol Verification
    # ==================================================================
    print("\n  " + "-" * 50)
    print("  Section 10: MCP Protocol Verification")
    print("  " + "-" * 50 + "\n")

    # Test 33: resources/read scene_tree
    try:
        resp = client.request("resources/read", {"uri": "godot://scene_tree"})
        result = resp.get("result", {})
        contents = result.get("contents", [])
        ok = len(contents) > 0 and "text" in contents[0]
        tree_data = json.loads(contents[0]["text"]) if ok else {}
        report(33, "resources/read godot://scene_tree returns valid JSON", ok and "name" in tree_data,
               f"root: {tree_data.get('name', 'N/A')}")
    except Exception as e:
        report(33, "resources/read godot://scene_tree returns valid JSON", False, f"Error: {e}")

    # Test 34: resources/read project_files
    try:
        resp = client.request("resources/read", {"uri": "godot://project_files"})
        result = resp.get("result", {})
        contents = result.get("contents", [])
        ok = len(contents) > 0 and "text" in contents[0]
        files_data = json.loads(contents[0]["text"]) if ok else {}
        report(34, "resources/read godot://project_files returns file list", ok and "files" in files_data,
               f"file_count: {files_data.get('count', 0)}")
    except Exception as e:
        report(34, "resources/read godot://project_files returns file list", False, f"Error: {e}")

    # Test 35: prompts/get
    try:
        resp = client.request("prompts/get", {"name": "build_ui_layout"})
        result = resp.get("result", {})
        has_messages = "messages" in result
        has_description = "description" in result
        ok = has_messages and has_description
        report(35, "prompts/get build_ui_layout returns prompt", ok,
               f"has_messages: {has_messages}, has_description: {has_description}")
    except Exception as e:
        report(35, "prompts/get build_ui_layout returns prompt", False, f"Error: {e}")

    # ==================================================================
    # Cleanup
    # ==================================================================
    client.close()

    # ==================================================================
    # Summary
    # ==================================================================
    _print_summary()

    passed = sum(1 for _, _, ok in results if ok)
    total = len(results)
    sys.exit(0 if passed == total else 1)


def _print_summary():
    print("\n" + "=" * 70)
    print("  MCP Meow - Integration Test Results")
    print("=" * 70)

    sections = {
        "Scene Creation & Hierarchy": range(1, 6),
        "Property Operations":       range(6, 11),
        "Scene Tree Inspection":     range(11, 13),
        "Script Operations":         range(13, 18),
        "Signal Operations":         range(18, 21),
        "UI Tools":                  range(21, 24),
        "Scene Save & Project Info": range(24, 28),
        "Delete & Cleanup":          range(28, 31),
        "Edit Script":               range(31, 33),
        "MCP Protocol":              range(33, 36),
    }

    for section_name, test_range in sections.items():
        section_results = [(n, name, ok) for n, name, ok in results if n in test_range]
        section_passed = sum(1 for _, _, ok in section_results if ok)
        section_total = len(section_results)
        status = "PASS" if section_passed == section_total else "FAIL"
        print(f"\n  [{status}] {section_name}: {section_passed}/{section_total}")
        for num, name, ok in section_results:
            tag = "[PASS]" if ok else "[FAIL]"
            print(f"    {tag} {num:2d}. {name}")

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    print()
    print(f"  {'=' * 40}")
    print(f"  INTEGRATION TEST SUMMARY")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print(f"  {'=' * 40}")


if __name__ == "__main__":
    run_tests()
