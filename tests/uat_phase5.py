#!/usr/bin/env python3
"""
Phase 5 UAT - Automated end-to-end tests for runtime, signal, and distribution tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase5.py

Tests cover:
  - tools/list shows 18 tools including all Phase 5 tools
  - run_game starts the game (main scene mode)
  - run_game while already running returns already_running
  - get_game_output captures log output from running game
  - stop_game stops a running game
  - stop_game when not running returns error
  - get_node_signals returns signal definitions for a node
  - connect_signal creates a signal connection
  - connect_signal duplicate returns error
  - disconnect_signal removes a signal connection
  - disconnect_signal when not connected returns error
  - CI workflow syntax validation
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase4.py)
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
    assert init_resp["result"]["serverInfo"]["name"] == "godot-mcp-meow"
    client.notify("notifications/initialized")
    time.sleep(0.3)
    print("  Handshake OK: godot-mcp-meow v" + init_resp["result"]["serverInfo"]["version"])
    print()

    print("=" * 60)
    print("  Phase 5 UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 18 tools including all Phase 5 tools
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        expected_phase5 = ["run_game", "stop_game", "get_game_output",
                           "get_node_signals", "connect_signal", "disconnect_signal"]
        has_all = all(n in tool_names for n in expected_phase5)
        ok = len(tools) == 18 and has_all
        report(1, "tools/list shows 18 tools with Phase 5 tools", ok,
               f"Tool count: {len(tools)}\n"
               f"         Phase 5 tools present: {has_all}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 18 tools with Phase 5 tools", False, f"Error: {e}")

    # ===================================================================
    # Test 2: run_game (main scene)
    # ===================================================================
    try:
        data, _ = call_tool(client, "run_game", {"mode": "main"})
        ok = (data.get("success") == True and
              data.get("running") == True and
              data.get("mode") == "main")
        report(2, "run_game starts main scene", ok,
               f"Response: {json.dumps(data)}")
        time.sleep(2)  # Let game start
    except Exception as e:
        report(2, "run_game starts main scene", False, f"Error: {e}")

    # ===================================================================
    # Test 3: run_game while already running
    # ===================================================================
    try:
        data, _ = call_tool(client, "run_game", {"mode": "main"})
        ok = (data.get("success") == True and
              data.get("already_running") == True)
        report(3, "run_game while already running", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(3, "run_game while already running", False, f"Error: {e}")

    # ===================================================================
    # Test 4: get_game_output
    # ===================================================================
    try:
        data, _ = call_tool(client, "get_game_output", {})
        ok = (data.get("success") == True and
              "lines" in data and
              isinstance(data["lines"], list) and
              "count" in data and
              isinstance(data["count"], int) and
              data.get("game_running") == True)
        report(4, "get_game_output captures output", ok,
               f"Lines: {data.get('count', '?')}, game_running: {data.get('game_running', '?')}")
    except Exception as e:
        report(4, "get_game_output captures output", False, f"Error: {e}")

    # ===================================================================
    # Test 5: stop_game
    # ===================================================================
    try:
        data, _ = call_tool(client, "stop_game", {})
        ok = (data.get("success") == True and
              data.get("stopped") == True)
        report(5, "stop_game stops running game", ok,
               f"Response: {json.dumps(data)}")
        time.sleep(1)  # Let game stop
    except Exception as e:
        report(5, "stop_game stops running game", False, f"Error: {e}")

    # ===================================================================
    # Test 6: stop_game when not running
    # ===================================================================
    try:
        data, _ = call_tool(client, "stop_game", {})
        error_msg = data.get("error", "")
        ok = (data.get("success") == False and
              "not" in error_msg.lower() and
              "running" in error_msg.lower())
        report(6, "stop_game when not running returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(6, "stop_game when not running returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 7: get_node_signals (create Button first)
    # ===================================================================
    try:
        # Create a Button node for signal testing
        call_tool(client, "create_node", {"name": "TestButton", "type": "Button", "parent_path": "."})
        time.sleep(0.3)

        data, _ = call_tool(client, "get_node_signals", {"node_path": "TestButton"})
        ok = False
        if data.get("success") == True and "signals" in data:
            signals = data["signals"]
            signal_names = [s["name"] for s in signals if "name" in s]
            ok = "pressed" in signal_names
        report(7, "get_node_signals returns signals for Button", ok,
               f"Signal count: {len(data.get('signals', []))}\n"
               f"         Has 'pressed': {'pressed' in [s.get('name','') for s in data.get('signals',[])]}")
    except Exception as e:
        report(7, "get_node_signals returns signals for Button", False, f"Error: {e}")

    # ===================================================================
    # Test 8: connect_signal
    # ===================================================================
    try:
        data, _ = call_tool(client, "connect_signal", {
            "source_path": "TestButton",
            "signal_name": "pressed",
            "target_path": ".",
            "method_name": "_on_button_pressed"
        })
        ok = data.get("success") == True
        report(8, "connect_signal creates connection", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(8, "connect_signal creates connection", False, f"Error: {e}")

    # ===================================================================
    # Test 9: connect_signal duplicate
    # ===================================================================
    try:
        data, _ = call_tool(client, "connect_signal", {
            "source_path": "TestButton",
            "signal_name": "pressed",
            "target_path": ".",
            "method_name": "_on_button_pressed"
        })
        error_msg = data.get("error", "")
        ok = (data.get("success") == False and
              "already connected" in error_msg.lower())
        report(9, "connect_signal duplicate returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(9, "connect_signal duplicate returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 10: disconnect_signal
    # ===================================================================
    try:
        data, _ = call_tool(client, "disconnect_signal", {
            "source_path": "TestButton",
            "signal_name": "pressed",
            "target_path": ".",
            "method_name": "_on_button_pressed"
        })
        ok = (data.get("success") == True and
              data.get("disconnected") == True)
        report(10, "disconnect_signal removes connection", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(10, "disconnect_signal removes connection", False, f"Error: {e}")

    # ===================================================================
    # Test 11: disconnect_signal when not connected
    # ===================================================================
    try:
        data, _ = call_tool(client, "disconnect_signal", {
            "source_path": "TestButton",
            "signal_name": "pressed",
            "target_path": ".",
            "method_name": "_on_button_pressed"
        })
        error_msg = data.get("error", "")
        ok = (data.get("success") == False and
              "not connected" in error_msg.lower())
        report(11, "disconnect_signal when not connected returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(11, "disconnect_signal when not connected returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 12: CI workflow syntax validation
    # ===================================================================
    try:
        import os
        workflow_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                     ".github", "workflows", "builds.yml")
        with open(workflow_path, "r") as f:
            content = f.read()
        checks = {
            "api_version=4.3": "api_version=4.3" in content,
            "scons bridge": "scons bridge" in content,
            "platform: linux": "platform: linux" in content,
            "platform: windows": "platform: windows" in content,
            "platform: macos": "platform: macos" in content,
        }
        ok = all(checks.values())
        detail_lines = [f"{k}: {'FOUND' if v else 'MISSING'}" for k, v in checks.items()]
        report(12, "CI workflow syntax validation", ok,
               "\n         ".join(detail_lines))
    except Exception as e:
        report(12, "CI workflow syntax validation", False, f"Error: {e}")

    # ===================================================================
    # Cleanup: remove test button
    # ===================================================================
    try:
        call_tool(client, "delete_node", {"node_path": "TestButton"})
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
    print("  Phase 5 UAT Test Results")
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
        print("\n  All Phase 5 automated tests passed!")
        sys.exit(0)


if __name__ == "__main__":
    run_tests()
