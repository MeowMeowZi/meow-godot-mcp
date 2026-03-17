#!/usr/bin/env python3
"""
Phase 4 UAT - Automated end-to-end tests for MCP prompts protocol and Phase 4 features.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase4.py

Tests cover:
  - Prompts capability in initialize response
  - prompts/list returns 4 prompts with correct structure
  - prompts/get returns messages with argument substitution
  - prompts/get with unknown name returns error
  - tools/list still returns tools (version filtering active)
  - Each prompt has required fields
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase3.py)
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

    # ===================================================================
    # Test 1: Initialize response includes prompts capability
    # ===================================================================
    print("=" * 60)
    print("  Phase 4 UAT Tests")
    print("=" * 60)

    try:
        caps = init_resp["result"]["capabilities"]
        has_prompts = "prompts" in caps
        has_list_changed = has_prompts and "listChanged" in caps["prompts"]
        ok = has_prompts and has_list_changed and caps["prompts"]["listChanged"] == False
        report(1, "Initialize has prompts capability", ok,
               f"capabilities.prompts = {json.dumps(caps.get('prompts', 'MISSING'))}")
    except Exception as e:
        report(1, "Initialize has prompts capability", False, f"Error: {e}")

    # ===================================================================
    # Test 2: prompts/list returns 4 prompts with expected names
    # ===================================================================
    try:
        resp = client.request("prompts/list")
        prompts = resp.get("result", {}).get("prompts", [])
        names = [p["name"] for p in prompts]
        expected_names = ["create_player_controller", "setup_scene_structure", "debug_physics", "create_ui_interface"]
        has_all = all(n in names for n in expected_names)
        ok = len(prompts) == 4 and has_all
        report(2, "prompts/list returns 4 prompts", ok,
               f"Count: {len(prompts)}, Names: {names}")
    except Exception as e:
        report(2, "prompts/list returns 4 prompts", False, f"Error: {e}")

    # ===================================================================
    # Test 3: prompts/get with valid name returns messages
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "create_player_controller",
            "arguments": {"movement_type": "2d_platformer"}
        })
        result = resp.get("result", {})
        has_desc = "description" in result and len(result["description"]) > 0
        has_msgs = "messages" in result and len(result["messages"]) > 0
        msg_ok = False
        if has_msgs:
            msg = result["messages"][0]
            msg_ok = msg.get("role") == "user" and "text" in msg.get("content", {})
            text = msg["content"]["text"]
            has_movement_type = "2d_platformer" in text
        else:
            has_movement_type = False
        ok = has_desc and has_msgs and msg_ok and has_movement_type
        report(3, "prompts/get returns messages with args", ok,
               f"description: {result.get('description', 'MISSING')[:60]}...\n"
               f"         messages: {len(result.get('messages', []))} message(s)\n"
               f"         contains '2d_platformer': {has_movement_type}")
    except Exception as e:
        report(3, "prompts/get returns messages with args", False, f"Error: {e}")

    # ===================================================================
    # Test 4: prompts/get with unknown name returns error
    # ===================================================================
    try:
        resp = client.request("prompts/get", {"name": "nonexistent"})
        has_error = "error" in resp
        error_code = resp.get("error", {}).get("code", 0)
        error_msg = resp.get("error", {}).get("message", "")
        ok = has_error and error_code == -32602 and "nonexistent" in error_msg
        report(4, "prompts/get unknown name returns error", ok,
               f"error.code: {error_code}, error.message: {error_msg}")
    except Exception as e:
        report(4, "prompts/get unknown name returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 5: tools/list still returns tools (version filtering active)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        ok = len(tools) >= 12
        tool_names = [t["name"] for t in tools[:5]]
        report(5, "tools/list returns tools (version filtering)", ok,
               f"Tool count: {len(tools)}, First 5: {tool_names}")
    except Exception as e:
        report(5, "tools/list returns tools (version filtering)", False, f"Error: {e}")

    # ===================================================================
    # Test 6: Each prompt has required fields (name, description, arguments)
    # ===================================================================
    try:
        resp = client.request("prompts/list")
        prompts = resp.get("result", {}).get("prompts", [])
        all_valid = True
        details = []
        for p in prompts:
            has_name = "name" in p and isinstance(p["name"], str) and len(p["name"]) > 0
            has_desc = "description" in p and isinstance(p["description"], str) and len(p["description"]) > 0
            has_args = "arguments" in p and isinstance(p["arguments"], list)
            valid = has_name and has_desc and has_args
            if not valid:
                all_valid = False
                details.append(f"INVALID: {p.get('name', '?')} name={has_name} desc={has_desc} args={has_args}")
            else:
                arg_names = [a["name"] for a in p["arguments"]]
                details.append(f"OK: {p['name']} (args: {arg_names})")
        ok = all_valid and len(prompts) == 4
        report(6, "Each prompt has required fields", ok,
               "\n         ".join(details))
    except Exception as e:
        report(6, "Each prompt has required fields", False, f"Error: {e}")

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 4 UAT Test Results")
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
        print("\n  All Phase 4 automated tests passed!")
        sys.exit(0)


if __name__ == "__main__":
    run_tests()
