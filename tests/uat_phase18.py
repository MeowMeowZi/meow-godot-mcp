#!/usr/bin/env python3
"""
Phase 18 UAT - Automated end-to-end tests for DX-04 (set_layout_preset root node).

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase18.py

Requirements covered:
  - DX-04: set_layout_preset supports scene root node via '' and '.' node_path

Tests cover:
  DX-04 (set_layout_preset root node):
  1. set_layout_preset with node_path="" and preset="full_rect" succeeds (DX-04)
  2. set_layout_preset with node_path="." and preset="center" succeeds (DX-04)
  3. get_ui_properties with node_path="" returns correct anchor values after "center" preset (DX-04)
  4. set_layout_preset with node_path="" preset="full_rect" -- verify anchors 0/0/1/1 (DX-04)
  5. set_layout_preset with node_path="" and invalid preset returns error (DX-04)
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase16.py)
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

results = []

def report(num, name, passed, detail=""):
    tag = PASS if passed else FAIL
    results.append((num, name, passed))
    print(f"  [{tag}] Test {num}: {name}")
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


def get_tool_text_raw(resp):
    """Extract raw text from a tools/call response (for error messages)."""
    result = resp.get("result", {})
    content_list = result.get("content", [])
    if content_list and "text" in content_list[0]:
        return content_list[0]["text"]
    return str(result)


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
    print("  Phase 18 Tool Ergonomics UAT: DX-04 set_layout_preset root")
    print("=" * 60)

    # ===================================================================
    # Pre-requisite: Create a test scene with a Control root node
    # ===================================================================
    print("\n  --- Setup: Creating Control root test scene ---\n")

    try:
        data, resp = call_tool_text(client, "create_scene", {
            "root_type": "Control",
            "root_name": "TestControl18"
        })
        raw_text = get_tool_text_raw(resp)
        print(f"  create_scene result: {raw_text[:200]}")
    except Exception as e:
        print(f"  FATAL: Could not create test scene: {e}")
        client.close()
        sys.exit(1)

    time.sleep(0.3)  # Allow scene to settle

    print("\n  --- DX-04: set_layout_preset on scene root node ---\n")

    # ===================================================================
    # Test 1: set_layout_preset with node_path="" and preset="full_rect"
    #         succeeds (DX-04 core verification)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "set_layout_preset", {
            "node_path": "",
            "preset": "full_rect"
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        success = data.get("success") == True if isinstance(data, dict) else False
        ok = not is_error and success
        report(1, "set_layout_preset with node_path='' preset=full_rect succeeds (DX-04)", ok,
               f"isError: {is_error}\n"
               f"         success: {success}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(1, "set_layout_preset with node_path='' preset=full_rect succeeds (DX-04)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 2: set_layout_preset with node_path="." and preset="center"
    #         succeeds (DX-04)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "set_layout_preset", {
            "node_path": ".",
            "preset": "center"
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        success = data.get("success") == True if isinstance(data, dict) else False
        ok = not is_error and success
        report(2, "set_layout_preset with node_path='.' preset=center succeeds (DX-04)", ok,
               f"isError: {is_error}\n"
               f"         success: {success}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(2, "set_layout_preset with node_path='.' preset=center succeeds (DX-04)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 3: get_ui_properties with node_path="" returns correct anchor
    #         values after "center" preset (all anchors 0.5) (DX-04)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_ui_properties", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)

        # Extract anchor values -- center preset should set all anchors to 0.5
        anchor_left = data.get("anchor_left", -1) if isinstance(data, dict) else -1
        anchor_top = data.get("anchor_top", -1) if isinstance(data, dict) else -1
        anchor_right = data.get("anchor_right", -1) if isinstance(data, dict) else -1
        anchor_bottom = data.get("anchor_bottom", -1) if isinstance(data, dict) else -1

        anchors_correct = (
            abs(float(anchor_left) - 0.5) < 0.01 and
            abs(float(anchor_top) - 0.5) < 0.01 and
            abs(float(anchor_right) - 0.5) < 0.01 and
            abs(float(anchor_bottom) - 0.5) < 0.01
        )
        ok = not is_error and anchors_correct
        report(3, "get_ui_properties node_path='' anchors match center preset (DX-04)", ok,
               f"isError: {is_error}\n"
               f"         anchor_left: {anchor_left} (expected 0.5)\n"
               f"         anchor_top: {anchor_top} (expected 0.5)\n"
               f"         anchor_right: {anchor_right} (expected 0.5)\n"
               f"         anchor_bottom: {anchor_bottom} (expected 0.5)\n"
               f"         anchors_correct: {anchors_correct}")
    except Exception as e:
        report(3, "get_ui_properties node_path='' anchors match center preset (DX-04)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: set_layout_preset with node_path="" preset="full_rect"
    #         then verify anchors are 0/0/1/1 (DX-04)
    # ===================================================================
    try:
        # Apply full_rect preset
        data, resp = call_tool_text(client, "set_layout_preset", {
            "node_path": "",
            "preset": "full_rect"
        })
        is_error_set = resp.get("result", {}).get("isError", False)

        # Read back anchor values
        data, resp = call_tool_text(client, "get_ui_properties", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error_get = resp.get("result", {}).get("isError", False)

        anchor_left = data.get("anchor_left", -1) if isinstance(data, dict) else -1
        anchor_top = data.get("anchor_top", -1) if isinstance(data, dict) else -1
        anchor_right = data.get("anchor_right", -1) if isinstance(data, dict) else -1
        anchor_bottom = data.get("anchor_bottom", -1) if isinstance(data, dict) else -1

        anchors_correct = (
            abs(float(anchor_left) - 0.0) < 0.01 and
            abs(float(anchor_top) - 0.0) < 0.01 and
            abs(float(anchor_right) - 1.0) < 0.01 and
            abs(float(anchor_bottom) - 1.0) < 0.01
        )
        ok = not is_error_set and not is_error_get and anchors_correct
        report(4, "set_layout_preset node_path='' full_rect anchors 0/0/1/1 (DX-04)", ok,
               f"set isError: {is_error_set}, get isError: {is_error_get}\n"
               f"         anchor_left: {anchor_left} (expected 0)\n"
               f"         anchor_top: {anchor_top} (expected 0)\n"
               f"         anchor_right: {anchor_right} (expected 1)\n"
               f"         anchor_bottom: {anchor_bottom} (expected 1)\n"
               f"         anchors_correct: {anchors_correct}")
    except Exception as e:
        report(4, "set_layout_preset node_path='' full_rect anchors 0/0/1/1 (DX-04)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 5: set_layout_preset with node_path="" and invalid preset
    #         returns error about unknown preset (not missing parameter) (DX-04)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "set_layout_preset", {
            "node_path": "",
            "preset": "nonexistent"
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        has_preset_error = ("unknown" in raw_text.lower() or
                            "invalid" in raw_text.lower() or
                            "preset" in raw_text.lower())
        # Should be an error about the preset, NOT about missing parameter
        ok = is_error and not has_missing_param and has_preset_error
        report(5, "set_layout_preset node_path='' invalid preset returns preset error (DX-04)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         has preset-related error: {has_preset_error}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(5, "set_layout_preset node_path='' invalid preset returns preset error (DX-04)", False,
               f"Error: {e}")

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
    print("  Phase 18 Tool Ergonomics DX-04 Results")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  === PHASE 18 UAT SUMMARY ===")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
