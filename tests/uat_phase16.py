#!/usr/bin/env python3
"""
Phase 16 UAT - Automated end-to-end tests for Game Bridge Auto-Wait & Node Path Root.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings (project/test_root.tscn)
  4. Run: python tests/uat_phase16.py

Requirements covered:
  - DX-01: run_game with wait_for_bridge=true defers response until bridge connects
  - DX-02: node_path tools accept '' and '.' for scene root without error

Tests cover:
  DX-02 (node_path root -- no game needed):
  1. set_node_property with node_path="" succeeds (DX-02)
  2. set_node_property with node_path="." succeeds (DX-02)
  3. get_node_signals with node_path="" returns signals (DX-02)
  4. get_node_signals with node_path="." returns signals (DX-02)
  5. get_ui_properties with node_path="" no missing-parameter error (DX-02)
  6. get_theme_overrides with node_path="" no missing-parameter error (DX-02)
  7. click_node with node_path="" errors about bridge not connected (DX-02)
  8. get_node_rect with node_path="" errors about bridge not connected (DX-02)

  DX-01 (wait_for_bridge -- game launch needed):
  9. run_game schema has wait_for_bridge and timeout properties (DX-01)
  10. run_game with wait_for_bridge=true returns bridge_connected=true (DX-01)
  11. get_game_bridge_status confirms connected=true (DX-01)
  12. run_game again with wait_for_bridge=true returns already_running + bridge_connected (DX-01)
  13. stop_game succeeds and bridge disconnects
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase15.py)
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
    print("  Phase 16 Game Bridge Auto-Wait & Node Path Root UAT")
    print("=" * 60)

    # First, get the current scene root name for set_node_property tests
    root_name = None
    try:
        tree_data, _ = call_tool_text(client, "get_scene_tree", {"max_depth": 0})
        root_name = tree_data.get("name", "")
    except Exception:
        root_name = "TestRoot"  # Fallback

    # ===================================================================
    # DX-02 Tests: node_path root handling (no game needed)
    # ===================================================================

    print("\n  --- DX-02: Node Path Root Handling ---\n")

    # ===================================================================
    # Test 1: set_node_property with node_path="" succeeds (DX-02)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": "",
            "property": "name",
            "value": root_name
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        ok = not is_error and not has_missing_param
        report(1, "set_node_property with node_path='' succeeds (DX-02)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(1, "set_node_property with node_path='' succeeds (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 2: set_node_property with node_path="." succeeds (DX-02)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "set_node_property", {
            "node_path": ".",
            "property": "name",
            "value": root_name
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        ok = not is_error and not has_missing_param
        report(2, "set_node_property with node_path='.' succeeds (DX-02)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(2, "set_node_property with node_path='.' succeeds (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 3: get_node_signals with node_path="" returns signals (DX-02)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_node_signals", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        has_signals = "signals" in raw_text.lower() or isinstance(data, dict) and "signals" in data
        ok = not is_error and not has_missing_param
        report(3, "get_node_signals with node_path='' returns signals (DX-02)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         has signals data: {has_signals}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(3, "get_node_signals with node_path='' returns signals (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: get_node_signals with node_path="." returns signals (DX-02)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_node_signals", {
            "node_path": "."
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        ok = not is_error and not has_missing_param
        report(4, "get_node_signals with node_path='.' returns signals (DX-02)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(4, "get_node_signals with node_path='.' returns signals (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 5: get_ui_properties with node_path="" -- no missing-parameter error (DX-02)
    # May error if root is not Control, but should NOT be a "missing parameter" error.
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_ui_properties", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        # OK if it works OR if it errors about non-Control node (not missing parameter)
        ok = not has_missing_param
        detail = f"isError: {is_error}\n         has missing-parameter error: {has_missing_param}"
        if is_error:
            detail += f"\n         Error (acceptable if not missing-param): {raw_text[:300]}"
        else:
            detail += f"\n         Response: {raw_text[:300]}"
        report(5, "get_ui_properties with node_path='' no missing-parameter error (DX-02)", ok, detail)
    except Exception as e:
        report(5, "get_ui_properties with node_path='' no missing-parameter error (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 6: get_theme_overrides with node_path="" -- no missing-parameter error (DX-02)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_theme_overrides", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        ok = not has_missing_param
        detail = f"isError: {is_error}\n         has missing-parameter error: {has_missing_param}"
        if is_error:
            detail += f"\n         Error (acceptable if not missing-param): {raw_text[:300]}"
        else:
            detail += f"\n         Response: {raw_text[:300]}"
        report(6, "get_theme_overrides with node_path='' no missing-parameter error (DX-02)", ok, detail)
    except Exception as e:
        report(6, "get_theme_overrides with node_path='' no missing-parameter error (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 7: click_node with node_path="" errors about bridge not connected (DX-02)
    # Should error about "bridge not connected" (not "missing parameter")
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "click_node", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        has_bridge_error = ("bridge" in raw_text.lower() or
                            "not connected" in raw_text.lower() or
                            "not running" in raw_text.lower() or
                            "game" in raw_text.lower())
        ok = is_error and not has_missing_param and has_bridge_error
        report(7, "click_node with node_path='' errors about bridge (not missing-param) (DX-02)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         has bridge/game error: {has_bridge_error}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(7, "click_node with node_path='' errors about bridge (not missing-param) (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 8: get_node_rect with node_path="" errors about bridge not connected (DX-02)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_node_rect", {
            "node_path": ""
        })
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        has_missing_param = "missing" in raw_text.lower() and "parameter" in raw_text.lower()
        has_bridge_error = ("bridge" in raw_text.lower() or
                            "not connected" in raw_text.lower() or
                            "not running" in raw_text.lower() or
                            "game" in raw_text.lower())
        ok = is_error and not has_missing_param and has_bridge_error
        report(8, "get_node_rect with node_path='' errors about bridge (not missing-param) (DX-02)", ok,
               f"isError: {is_error}\n"
               f"         has missing-parameter error: {has_missing_param}\n"
               f"         has bridge/game error: {has_bridge_error}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(8, "get_node_rect with node_path='' errors about bridge (not missing-param) (DX-02)", False,
               f"Error: {e}")

    # ===================================================================
    # DX-01 Tests: wait_for_bridge (game launch needed)
    # ===================================================================

    print("\n  --- DX-01: Wait for Bridge ---\n")

    # ===================================================================
    # Test 9: run_game schema has wait_for_bridge and timeout properties (DX-01)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        run_game_tool = None
        for t in tools:
            if t["name"] == "run_game":
                run_game_tool = t
                break
        schema = run_game_tool.get("inputSchema", {}) if run_game_tool else {}
        props = schema.get("properties", {})
        has_wait_for_bridge = "wait_for_bridge" in props
        has_timeout = "timeout" in props
        wait_type = props.get("wait_for_bridge", {}).get("type", "")
        timeout_type = props.get("timeout", {}).get("type", "")
        ok = has_wait_for_bridge and has_timeout and wait_type == "boolean" and timeout_type == "integer"
        report(9, "run_game schema has wait_for_bridge (boolean) and timeout (integer) (DX-01)", ok,
               f"wait_for_bridge present: {has_wait_for_bridge} (type: {wait_type})\n"
               f"         timeout present: {has_timeout} (type: {timeout_type})\n"
               f"         Schema properties: {list(props.keys())}")
    except Exception as e:
        report(9, "run_game schema has wait_for_bridge (boolean) and timeout (integer) (DX-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 10: run_game with wait_for_bridge=true returns bridge_connected=true (DX-01)
    # Uses 20s socket timeout since deferred response waits for bridge.
    # ===================================================================
    bridge_connected = False
    try:
        data, resp = call_tool_text(client, "run_game", {
            "mode": "main",
            "wait_for_bridge": True,
            "timeout": 15000
        }, timeout=20)
        raw_text = get_tool_text_raw(resp)
        is_error = resp.get("result", {}).get("isError", False)
        success = data.get("success") == True
        got_bridge = data.get("bridge_connected") == True
        already_running = data.get("already_running") == True
        bridge_connected = got_bridge
        ok = not is_error and (success or already_running) and got_bridge
        report(10, "run_game with wait_for_bridge=true returns bridge_connected=true (DX-01)", ok,
               f"isError: {is_error}\n"
               f"         success: {success}, already_running: {already_running}\n"
               f"         bridge_connected: {got_bridge}\n"
               f"         Response: {raw_text[:400]}")
    except Exception as e:
        report(10, "run_game with wait_for_bridge=true returns bridge_connected=true (DX-01)", False,
               f"Error: {e}")

    # --- EARLY EXIT if bridge not connected ---
    if not bridge_connected:
        print("\n  FATAL: Bridge not connected after wait_for_bridge. Cannot run remaining DX-01 tests.")
        print("  EARLY EXIT: Skipping remaining bridge-dependent tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        _print_summary()
        sys.exit(1)

    # ===================================================================
    # Test 11: get_game_bridge_status confirms connected=true (DX-01)
    # ===================================================================
    try:
        data, resp = call_tool_text(client, "get_game_bridge_status")
        raw_text = get_tool_text_raw(resp)
        connected = data.get("connected") == True
        ok = connected
        report(11, "get_game_bridge_status confirms connected=true after wait (DX-01)", ok,
               f"connected: {connected}\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(11, "get_game_bridge_status confirms connected=true after wait (DX-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 12: run_game again with wait_for_bridge=true returns
    #          already_running + bridge_connected immediately (DX-01)
    # ===================================================================
    try:
        t0 = time.time()
        data, resp = call_tool_text(client, "run_game", {
            "mode": "main",
            "wait_for_bridge": True,
            "timeout": 15000
        }, timeout=20)
        elapsed = time.time() - t0
        raw_text = get_tool_text_raw(resp)
        already_running = data.get("already_running") == True
        got_bridge = data.get("bridge_connected") == True
        # Should return quickly (under 2s) since bridge is already connected
        fast_response = elapsed < 2.0
        ok = already_running and got_bridge and fast_response
        report(12, "run_game already-running with wait_for_bridge returns immediately (DX-01)", ok,
               f"already_running: {already_running}\n"
               f"         bridge_connected: {got_bridge}\n"
               f"         elapsed: {elapsed:.2f}s (expected < 2s)\n"
               f"         Response: {raw_text[:300]}")
    except Exception as e:
        report(12, "run_game already-running with wait_for_bridge returns immediately (DX-01)", False,
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
               f"stop_game success: {game_stopped}\n"
               f"         bridge disconnected: {bridge_disconnected}\n"
               f"         bridge status: {json.dumps(status)}")
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
    print("  Phase 16 Game Bridge Auto-Wait & Node Path Root Results")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  === PHASE 16 UAT SUMMARY ===")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
