#!/usr/bin/env python3
"""
Phase 6 UAT - Automated end-to-end tests for scene file management tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase6.py

Tests cover:
  - tools/list shows 23 tools including all Phase 6 tools
  - create_scene creates a new scene with specified root type
  - list_open_scenes returns open scene list with active detection
  - save_scene overwrites current file (SCNF-01)
  - save_scene with path saves to new location (SCNF-05)
  - open_scene opens existing scene file (SCNF-02)
  - open_scene with invalid path returns error
  - create_scene with invalid/non-Node class returns error
  - instantiate_scene adds sub-scene with undo/redo (SCNF-06)
  - instantiate_scene with non-existent scene returns error
  - Cross-validation: scene tree contains instantiated child
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase5.py)
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


# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"

results = []

def report(num, name, passed, detail=""):
    tag = PASS if passed else FAIL
    results.append((num, name, passed))
    print(f"  [{tag}] Test {num}: {name}")
    if detail:
        for line in detail.strip().split("\n"):
            print(f"         {line}")


def call_tool(client, tool_name, arguments=None):
    """Call a tool via tools/call and return parsed content JSON."""
    if arguments is None:
        arguments = {}
    resp = client.request("tools/call", {"name": tool_name, "arguments": arguments})
    result = resp.get("result", {})
    content_list = result.get("content", [])
    if content_list and "text" in content_list[0]:
        return json.loads(content_list[0]["text"]), resp
    return result, resp


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def run_tests():
    client = MCPClient()

    print("\nConnecting to Godot MCP server (127.0.0.1:6800)...")
    try:
        client.connect()
    except Exception as e:
        print(f"\n  Connection failed: {e}")
        print("  Please ensure the Godot editor is open with the MCP Meow plugin enabled.")
        sys.exit(1)

    # --- Handshake ---
    print("  Performing MCP handshake...")
    init_resp = client.request("initialize")
    assert init_resp["result"]["serverInfo"]["name"] == "meow-godot-mcp"
    client.notify("notifications/initialized")
    time.sleep(0.3)
    print("  Handshake OK: meow-godot-mcp v" + init_resp["result"]["serverInfo"]["version"])
    print()

    print("=" * 60)
    print("  Phase 6 UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 23 tools including all Phase 6 tools
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        expected_phase6 = ["save_scene", "open_scene", "list_open_scenes",
                           "create_scene", "instantiate_scene"]
        has_all = all(n in tool_names for n in expected_phase6)
        ok = len(tools) >= 23 and has_all
        report(1, "tools/list includes Phase 6 tools", ok,
               f"Tool count: {len(tools)}\n"
               f"         Phase 6 tools present: {has_all}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 23 tools with Phase 6 tools", False, f"Error: {e}")

    # ===================================================================
    # Test 2: create_scene creates a new scene with Node2D root (SCNF-04)
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_scene", {
            "root_type": "Node2D",
            "path": "res://test_uat_scene.tscn",
            "root_name": "TestRoot"
        })
        ok = (data.get("success") == True and
              data.get("path") == "res://test_uat_scene.tscn" and
              data.get("root_type") == "Node2D")
        report(2, "create_scene creates Node2D scene (SCNF-04)", ok,
               f"Response: {json.dumps(data)}")
        time.sleep(0.5)
    except Exception as e:
        report(2, "create_scene creates Node2D scene (SCNF-04)", False, f"Error: {e}")

    # ===================================================================
    # Test 3: list_open_scenes shows the created scene (SCNF-03)
    # ===================================================================
    try:
        data, _ = call_tool(client, "list_open_scenes")
        ok = False
        detail = ""
        if data.get("success") == True and data.get("count", 0) >= 1:
            scenes = data.get("scenes", [])
            has_scene = any("test_uat_scene.tscn" in s.get("path", "") for s in scenes)
            has_keys = all(
                all(k in s for k in ("path", "title", "is_active"))
                for s in scenes
            )
            ok = has_scene and has_keys
            detail = (f"Count: {data['count']}\n"
                      f"         Has test scene: {has_scene}\n"
                      f"         All keys present: {has_keys}\n"
                      f"         Scenes: {json.dumps(scenes)}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(3, "list_open_scenes shows created scene (SCNF-03)", ok, detail)
    except Exception as e:
        report(3, "list_open_scenes shows created scene (SCNF-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 4: save_scene saves current scene without path (SCNF-01)
    # ===================================================================
    try:
        data, _ = call_tool(client, "save_scene")
        ok = (data.get("success") == True and
              "test_uat_scene.tscn" in data.get("path", ""))
        report(4, "save_scene overwrites current scene (SCNF-01)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(4, "save_scene overwrites current scene (SCNF-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 5: save_scene with path saves to new location (SCNF-05)
    # ===================================================================
    try:
        data, _ = call_tool(client, "save_scene", {"path": "res://test_uat_scene_copy.tscn"})
        ok = (data.get("success") == True and
              data.get("path") == "res://test_uat_scene_copy.tscn")
        report(5, "save_scene with path saves to new location (SCNF-05)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(5, "save_scene with path saves to new location (SCNF-05)", False, f"Error: {e}")

    # ===================================================================
    # Test 6: open_scene opens an existing scene file (SCNF-02)
    # ===================================================================
    try:
        data, _ = call_tool(client, "open_scene", {"path": "res://test_uat_scene_copy.tscn"})
        ok = (data.get("success") == True and
              data.get("path") == "res://test_uat_scene_copy.tscn")
        report(6, "open_scene opens existing scene file (SCNF-02)", ok,
               f"Response: {json.dumps(data)}")
        time.sleep(0.5)
    except Exception as e:
        report(6, "open_scene opens existing scene file (SCNF-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 7: list_open_scenes shows both scenes after open_scene
    # ===================================================================
    try:
        data, _ = call_tool(client, "list_open_scenes")
        ok = False
        detail = ""
        if data.get("success") == True and data.get("count", 0) >= 2:
            scenes = data.get("scenes", [])
            paths = [s.get("path", "") for s in scenes]
            has_original = any("test_uat_scene.tscn" in p for p in paths)
            has_copy = any("test_uat_scene_copy.tscn" in p for p in paths)
            ok = has_original and has_copy
            detail = (f"Count: {data['count']}\n"
                      f"         Has original: {has_original}\n"
                      f"         Has copy: {has_copy}\n"
                      f"         Paths: {paths}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(7, "list_open_scenes shows both scenes", ok, detail)
    except Exception as e:
        report(7, "list_open_scenes shows both scenes", False, f"Error: {e}")

    # ===================================================================
    # Test 8: open_scene with non-existent file returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "open_scene", {"path": "res://nonexistent_scene.tscn"})
        has_error = "error" in data
        error_msg = data.get("error", "")
        ok = has_error and "not found" in error_msg.lower()
        report(8, "open_scene with non-existent file returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(8, "open_scene with non-existent file returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 9: create_scene with invalid class returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_scene", {
            "root_type": "FakeNodeClass",
            "path": "res://test_fake.tscn"
        })
        has_error = "error" in data
        error_msg = data.get("error", "")
        ok = has_error and ("unknown class" in error_msg.lower() or "not a node" in error_msg.lower())
        report(9, "create_scene with invalid class returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(9, "create_scene with invalid class returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 10: create_scene with non-Node class returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_scene", {
            "root_type": "Resource",
            "path": "res://test_resource.tscn"
        })
        has_error = "error" in data
        error_msg = data.get("error", "")
        ok = has_error and "not a node" in error_msg.lower()
        report(10, "create_scene with non-Node class returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(10, "create_scene with non-Node class returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 11: instantiate_scene adds a sub-scene (SCNF-06)
    # ===================================================================
    try:
        # Switch back to test_uat_scene.tscn
        call_tool(client, "open_scene", {"path": "res://test_uat_scene.tscn"})
        time.sleep(0.5)

        data, _ = call_tool(client, "instantiate_scene", {
            "scene_path": "res://test_uat_scene_copy.tscn",
            "name": "InstancedChild"
        })
        ok = (data.get("success") == True and
              "InstancedChild" in data.get("path", "") and
              data.get("scene_path") == "res://test_uat_scene_copy.tscn")
        report(11, "instantiate_scene adds sub-scene (SCNF-06)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(11, "instantiate_scene adds sub-scene (SCNF-06)", False, f"Error: {e}")

    # ===================================================================
    # Test 12: instantiate_scene with non-existent scene returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "instantiate_scene", {
            "scene_path": "res://nonexistent.tscn"
        })
        has_error = "error" in data
        ok = has_error
        report(12, "instantiate_scene with non-existent scene returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(12, "instantiate_scene with non-existent scene returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 13: get_scene_tree shows instantiated child (cross-validation)
    # ===================================================================
    try:
        data, _ = call_tool(client, "get_scene_tree")
        data_str = json.dumps(data)
        ok = "InstancedChild" in data_str
        report(13, "get_scene_tree shows instantiated child", ok,
               f"Tree contains InstancedChild: {ok}\n"
               f"         Tree: {data_str[:500]}")
    except Exception as e:
        report(13, "get_scene_tree shows instantiated child", False, f"Error: {e}")

    # ===================================================================
    # Cleanup: remove instantiated node
    # ===================================================================
    try:
        call_tool(client, "delete_node", {"node_path": "InstancedChild"})
    except Exception:
        pass

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 6 UAT Test Results")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  Passed: {passed}/{total}  Failed: {failed}/{total}")
    print("=" * 60)

    if failed > 0:
        print("\n  Some tests failed. Check detailed output above.")
        sys.exit(1)
    else:
        print("\n  All Phase 6 automated tests passed!")
        sys.exit(0)


if __name__ == "__main__":
    run_tests()
