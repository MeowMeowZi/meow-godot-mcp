#!/usr/bin/env python3
"""
Phase 13 UAT - Automated end-to-end tests for runtime state query tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings
  4. Run: python tests/uat_phase13.py

Requirements covered:
  - RTST-01: get_game_node_property reads property values from running game nodes
  - RTST-02: eval_in_game evaluates GDScript expressions in running game context
  - RTST-03: get_game_scene_tree returns full scene tree structure from running game

Tests cover:
  - tools/list shows 44 tools including get_game_node_property, eval_in_game,
    get_game_scene_tree (RTST-01, RTST-02, RTST-03)
  - Game launches and bridge connects automatically
  - get_game_scene_tree returns tree with name, type, path fields (RTST-03)
  - get_game_scene_tree with max_depth=0 returns root only (RTST-03)
  - get_game_node_property reads position from scene root (RTST-01)
  - get_game_node_property reads visible from scene root (RTST-01)
  - get_game_node_property error for non-existent node (RTST-01)
  - get_game_node_property error for non-existent property (RTST-01)
  - eval_in_game evaluates simple math "2 + 2" (RTST-02)
  - eval_in_game evaluates get_children().size() (RTST-02)
  - eval_in_game evaluates string concatenation (RTST-02)
  - eval_in_game returns error for invalid expression (RTST-02)
  - stop_game succeeds and bridge disconnects
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase12.py)
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


def report_skip(num, name, detail=""):
    results.append((num, name, True))  # Count skips as pass
    print(f"  [{SKIP}] Test {num}: {name}")
    if detail:
        for line in detail.strip().split("\n"):
            print(f"         {line}")


def call_tool(client, tool_name, arguments=None, timeout=None):
    """Call a tool via tools/call and return the raw result dict and full response."""
    if arguments is None:
        arguments = {}
    resp = client.request("tools/call", {"name": tool_name, "arguments": arguments},
                          timeout=timeout)
    result = resp.get("result", {})
    return result, resp


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


def wait_for_bridge(client, max_wait=10.0, poll_interval=0.5):
    """Poll get_game_bridge_status until connected=true or timeout."""
    elapsed = 0.0
    data = {}
    while elapsed < max_wait:
        data, _ = call_tool_text(client, "get_game_bridge_status")
        if data.get("connected") == True:
            return True, data
        time.sleep(poll_interval)
        elapsed += poll_interval
    return False, data


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
    print("  Phase 13 Runtime State Query UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 44 tools including runtime state query
    #         tools (RTST-01, RTST-02, RTST-03)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        has_get_game_node_property = "get_game_node_property" in tool_names
        has_eval_in_game = "eval_in_game" in tool_names
        has_get_game_scene_tree = "get_game_scene_tree" in tool_names
        ok = (len(tools) == 44 and
              has_get_game_node_property and
              has_eval_in_game and
              has_get_game_scene_tree)
        report(1, "tools/list shows 44 tools including runtime state query tools (RTST-01, RTST-02, RTST-03)", ok,
               f"Tool count: {len(tools)}\n"
               f"         get_game_node_property present: {has_get_game_node_property}\n"
               f"         eval_in_game present: {has_eval_in_game}\n"
               f"         get_game_scene_tree present: {has_get_game_scene_tree}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 44 tools including runtime state query tools (RTST-01, RTST-02, RTST-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 2: run_game and bridge connects within 10s
    # ===================================================================
    bridge_connected = False
    try:
        data, _ = call_tool_text(client, "run_game", {"mode": "main"})
        game_started = data.get("success") == True or data.get("already_running") == True
        if not game_started:
            report(2, "run_game and bridge connects within 10s", False,
                   f"run_game failed: {json.dumps(data)}")
        else:
            connected, status = wait_for_bridge(client, max_wait=10.0)
            bridge_connected = connected
            ok = connected
            report(2, "run_game and bridge connects within 10s", ok,
                   f"run_game: {json.dumps(data)}\n"
                   f"         Bridge status: {json.dumps(status)}\n"
                   f"         Connected: {connected}")
    except Exception as e:
        report(2, "run_game and bridge connects within 10s", False, f"Error: {e}")

    # --- EARLY EXIT if bridge not connected ---
    if not bridge_connected:
        print("\n  FATAL: Bridge not connected. Cannot run remaining tests.")
        print("  EARLY EXIT: Skipping all runtime state query tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        _print_summary()
        sys.exit(1)

    # ===================================================================
    # Test 3: get_game_scene_tree returns tree with name, type, path
    #         fields (RTST-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_scene_tree", {}, timeout=15)
        has_name = isinstance(data.get("name"), str) and len(data.get("name", "")) > 0
        has_type = isinstance(data.get("type"), str) and len(data.get("type", "")) > 0
        has_path = isinstance(data.get("path"), str) and len(data.get("path", "")) > 0
        ok = has_name and has_type and has_path
        report(3, "get_game_scene_tree returns tree with name, type, path fields (RTST-03)", ok,
               f"name: {data.get('name')} (present: {has_name})\n"
               f"         type: {data.get('type')} (present: {has_type})\n"
               f"         path: {data.get('path')} (present: {has_path})\n"
               f"         Full response keys: {list(data.keys())}")
    except Exception as e:
        report(3, "get_game_scene_tree returns tree with name, type, path fields (RTST-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: get_game_scene_tree with max_depth=0 returns root only,
    #         no children (RTST-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_scene_tree", {
            "max_depth": 0
        }, timeout=15)
        has_name = isinstance(data.get("name"), str) and len(data.get("name", "")) > 0
        has_type = isinstance(data.get("type"), str) and len(data.get("type", "")) > 0
        # max_depth=0 should return root with no children key or empty children
        children = data.get("children", [])
        no_children = (children is None or
                       (isinstance(children, list) and len(children) == 0) or
                       "children" not in data)
        ok = has_name and has_type and no_children
        report(4, "get_game_scene_tree with max_depth=0 returns root only (RTST-03)", ok,
               f"name: {data.get('name')}, type: {data.get('type')}\n"
               f"         children key present: {'children' in data}\n"
               f"         children value: {data.get('children', 'NOT_PRESENT')}\n"
               f"         no_children: {no_children}")
    except Exception as e:
        report(4, "get_game_scene_tree with max_depth=0 returns root only (RTST-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 5: get_game_node_property reads position from scene root
    #         (RTST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_node_property", {
            "node_path": "",
            "property": "position"
        }, timeout=15)
        has_value = "value" in data
        has_type = "type" in data
        ok = has_value and has_type
        report(5, "get_game_node_property reads position from scene root (RTST-01)", ok,
               f"value: {data.get('value')} (present: {has_value})\n"
               f"         type: {data.get('type')} (present: {has_type})\n"
               f"         Full response: {json.dumps(data)}")
    except Exception as e:
        report(5, "get_game_node_property reads position from scene root (RTST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 6: get_game_node_property reads visible from scene root
    #         (RTST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_node_property", {
            "node_path": "",
            "property": "visible"
        }, timeout=15)
        has_value = "value" in data
        has_type = "type" in data
        value_str = str(data.get("value", "")).lower()
        type_str = str(data.get("type", "")).lower()
        value_contains_true = "true" in value_str
        type_contains_bool = "bool" in type_str
        ok = has_value and has_type and value_contains_true and type_contains_bool
        report(6, "get_game_node_property reads visible=true with bool type from scene root (RTST-01)", ok,
               f"value: {data.get('value')} (contains 'true': {value_contains_true})\n"
               f"         type: {data.get('type')} (contains 'bool': {type_contains_bool})\n"
               f"         Full response: {json.dumps(data)}")
    except Exception as e:
        report(6, "get_game_node_property reads visible=true with bool type from scene root (RTST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 7: get_game_node_property error for non-existent node
    #         (RTST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_node_property", {
            "node_path": "NonExistentNode12345",
            "property": "position"
        }, timeout=15)
        has_error = "error" in data
        error_msg = str(data.get("error", "")).lower()
        mentions_not_found = "not found" in error_msg
        ok = has_error and mentions_not_found
        report(7, "get_game_node_property error for non-existent node (RTST-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         has_error: {has_error}\n"
               f"         mentions 'not found': {mentions_not_found}")
    except Exception as e:
        report(7, "get_game_node_property error for non-existent node (RTST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 8: get_game_node_property error for non-existent property
    #         (RTST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_node_property", {
            "node_path": "",
            "property": "nonexistent_property_xyz"
        }, timeout=15)
        has_error = "error" in data
        error_msg = str(data.get("error", "")).lower()
        mentions_not_found = "not found" in error_msg
        ok = has_error and mentions_not_found
        report(8, "get_game_node_property error for non-existent property (RTST-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         has_error: {has_error}\n"
               f"         mentions 'not found': {mentions_not_found}")
    except Exception as e:
        report(8, "get_game_node_property error for non-existent property (RTST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 9: eval_in_game simple math "2 + 2" returns "4" (RTST-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "eval_in_game", {
            "expression": "2 + 2"
        }, timeout=15)
        has_result = "result" in data
        result_val = str(data.get("result", ""))
        contains_four = "4" in result_val
        ok = has_result and contains_four
        report(9, "eval_in_game simple math '2 + 2' returns result containing '4' (RTST-02)", ok,
               f"result: {data.get('result')} (contains '4': {contains_four})\n"
               f"         has_result: {has_result}\n"
               f"         Full response: {json.dumps(data)}")
    except Exception as e:
        report(9, "eval_in_game simple math '2 + 2' returns result containing '4' (RTST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 10: eval_in_game get_children().size() returns a number >= 0
    #          (RTST-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "eval_in_game", {
            "expression": "get_children().size()"
        }, timeout=15)
        has_result = "result" in data
        result_str = str(data.get("result", ""))
        # The result should be a non-negative integer as string
        try:
            result_num = int(result_str)
            is_non_negative = result_num >= 0
        except (ValueError, TypeError):
            is_non_negative = False
        ok = has_result and is_non_negative
        report(10, "eval_in_game get_children().size() returns number >= 0 (RTST-02)", ok,
               f"result: {data.get('result')}\n"
               f"         parsed as int: {result_str}\n"
               f"         non-negative: {is_non_negative}\n"
               f"         Full response: {json.dumps(data)}")
    except Exception as e:
        report(10, "eval_in_game get_children().size() returns number >= 0 (RTST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 11: eval_in_game string concatenation (RTST-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "eval_in_game", {
            "expression": '"hello" + " " + "world"'
        }, timeout=15)
        has_result = "result" in data
        result_val = str(data.get("result", ""))
        contains_hello_world = "hello world" in result_val
        ok = has_result and contains_hello_world
        report(11, "eval_in_game string concatenation returns 'hello world' (RTST-02)", ok,
               f"result: {data.get('result')} (contains 'hello world': {contains_hello_world})\n"
               f"         has_result: {has_result}\n"
               f"         Full response: {json.dumps(data)}")
    except Exception as e:
        report(11, "eval_in_game string concatenation returns 'hello world' (RTST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 12: eval_in_game returns error for invalid expression
    #          (RTST-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "eval_in_game", {
            "expression": "invalid syntax {{{{"
        }, timeout=15)
        has_error = "error" in data
        error_msg = str(data.get("error", "")).lower()
        mentions_error = "error" in error_msg or "parse" in error_msg
        ok = has_error and mentions_error
        report(12, "eval_in_game returns error for invalid expression syntax (RTST-02)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         has_error: {has_error}\n"
               f"         error message: {data.get('error', '')}")
    except Exception as e:
        report(12, "eval_in_game returns error for invalid expression syntax (RTST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 13: stop_game succeeds and bridge disconnects
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "stop_game")
        game_stopped = data.get("success") == True
        time.sleep(1.0)  # Allow bridge to disconnect

        status, _ = call_tool_text(client, "get_game_bridge_status")
        bridge_disconnected = status.get("connected") == False
        ok = game_stopped and bridge_disconnected
        report(13, "stop_game succeeds and bridge disconnects", ok,
               f"stop_game: {json.dumps(data)}\n"
               f"         bridge status: {json.dumps(status)}\n"
               f"         game_stopped: {game_stopped}, bridge_disconnected: {bridge_disconnected}")
    except Exception as e:
        report(13, "stop_game succeeds and bridge disconnects", False, f"Error: {e}")

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    _print_summary()

    passed = sum(1 for _, _, ok in results if ok)
    total = len(results)
    sys.exit(0 if passed == total else 1)


def _print_summary():
    print("\n" + "=" * 60)
    print("  Phase 13 Runtime State Query UAT Results")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  === PHASE 13 UAT SUMMARY ===")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
