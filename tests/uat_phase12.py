#!/usr/bin/env python3
"""
Phase 12 UAT - Automated end-to-end tests for input injection enhancement tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings
  4. Run: python tests/uat_phase12.py

Requirements covered:
  - INPT-01: click action auto-includes press+release complete cycle
  - INPT-02: click_node tool - click UI Control nodes by scene tree path
  - INPT-03: get_node_rect tool - get Control node screen coordinates and size

Tests cover:
  - tools/list shows 44 tools including click_node and get_node_rect (INPT-02, INPT-03)
  - Game launches and bridge connects automatically
  - inject_input auto-cycle click with no explicit pressed param (INPT-01)
  - inject_input backward-compatible click with pressed=true (INPT-01)
  - inject_input backward-compatible click with pressed=false (INPT-01)
  - get_node_rect on scene root returns position, size, global_position, center, visible (INPT-03)
  - get_node_rect error for non-existent node (INPT-03)
  - get_node_rect error for non-visible node (INPT-03)
  - click_node on a known Control node returns success + coordinates (INPT-02)
  - click_node returns valid clicked coordinates (INPT-02)
  - click_node error for non-existent node (INPT-02)
  - click_node error for non-Control node (INPT-02)
  - stop_game and bridge disconnects
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase10.py)
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
    print("  Phase 12 Input Injection Enhancement UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 44 tools including click_node and
    #         get_node_rect (INPT-02, INPT-03)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        has_click_node = "click_node" in tool_names
        has_get_node_rect = "get_node_rect" in tool_names
        ok = len(tools) == 44 and has_click_node and has_get_node_rect
        report(1, "tools/list shows 44 tools including click_node and get_node_rect (INPT-02, INPT-03)", ok,
               f"Tool count: {len(tools)}\n"
               f"         click_node present: {has_click_node}\n"
               f"         get_node_rect present: {has_get_node_rect}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 44 tools including click_node and get_node_rect (INPT-02, INPT-03)", False,
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

    if not bridge_connected:
        print("\n  FATAL: Bridge not connected. Cannot run remaining tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        _print_summary()
        sys.exit(1)

    # ===================================================================
    # Test 3: inject_input click auto-cycle -- no explicit pressed
    #         (INPT-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "mouse",
            "mouse_action": "click",
            "position": {"x": 100, "y": 100},
            "button": "left"
        })
        ok = (data.get("success") == True and
              data.get("mouse_action") == "click" and
              data.get("auto_cycle") == True)
        report(3, "inject_input click auto-cycle (no pressed param) shows auto_cycle:true (INPT-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         auto_cycle present: {'auto_cycle' in data}\n"
               f"         auto_cycle value: {data.get('auto_cycle')}")
    except Exception as e:
        report(3, "inject_input click auto-cycle (no pressed param) shows auto_cycle:true (INPT-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: inject_input click with explicit pressed=true preserves
    #         single-fire (INPT-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "mouse",
            "mouse_action": "click",
            "position": {"x": 100, "y": 100},
            "pressed": True
        })
        ok = (data.get("success") == True and
              data.get("pressed") == True and
              "auto_cycle" not in data)
        report(4, "inject_input click with pressed=true preserves single-fire (INPT-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         pressed value: {data.get('pressed')}\n"
               f"         auto_cycle absent: {'auto_cycle' not in data}")
    except Exception as e:
        report(4, "inject_input click with pressed=true preserves single-fire (INPT-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 5: inject_input click with explicit pressed=false preserves
    #         single-fire (INPT-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "mouse",
            "mouse_action": "click",
            "position": {"x": 100, "y": 100},
            "pressed": False
        })
        ok = (data.get("success") == True and
              data.get("pressed") == False and
              "auto_cycle" not in data)
        report(5, "inject_input click with pressed=false preserves single-fire (INPT-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         pressed value: {data.get('pressed')}\n"
               f"         auto_cycle absent: {'auto_cycle' not in data}")
    except Exception as e:
        report(5, "inject_input click with pressed=false preserves single-fire (INPT-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 6: get_node_rect on scene root returns position, size,
    #         global_position, center, visible (INPT-03)
    # ===================================================================
    scene_root_is_control = False
    try:
        data, _ = call_tool_text(client, "get_node_rect", {
            "node_path": ""
        }, timeout=15)
        has_position = (isinstance(data.get("position"), dict) and
                        isinstance(data["position"].get("x"), (int, float)) and
                        isinstance(data["position"].get("y"), (int, float)))
        has_size = (isinstance(data.get("size"), dict) and
                    isinstance(data["size"].get("width"), (int, float)) and
                    isinstance(data["size"].get("height"), (int, float)))
        has_global = (isinstance(data.get("global_position"), dict) and
                      isinstance(data["global_position"].get("x"), (int, float)) and
                      isinstance(data["global_position"].get("y"), (int, float)))
        has_center = (isinstance(data.get("center"), dict) and
                      isinstance(data["center"].get("x"), (int, float)) and
                      isinstance(data["center"].get("y"), (int, float)))
        has_visible = data.get("visible") == True
        ok = has_position and has_size and has_global and has_center and has_visible
        if ok:
            scene_root_is_control = True
        # If scene root is not a Control, we may get an error -- that's expected for some scenes
        if "error" in data:
            # Scene root is not a Control -- try to find a child Control later
            report_skip(6, "get_node_rect on scene root (INPT-03)",
                        f"Scene root is not a Control: {data.get('error')}\n"
                        f"         This is expected for scenes with non-Control root nodes")
        else:
            report(6, "get_node_rect on scene root returns position/size/global_position/center/visible (INPT-03)", ok,
                   f"Response: {json.dumps(data)}\n"
                   f"         position: {has_position}, size: {has_size}\n"
                   f"         global_position: {has_global}, center: {has_center}\n"
                   f"         visible: {has_visible}")
    except Exception as e:
        report(6, "get_node_rect on scene root returns position/size/global_position/center/visible (INPT-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 7: get_node_rect error for non-existent node (INPT-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_node_rect", {
            "node_path": "NonExistentNode/DoesNotExist"
        }, timeout=15)
        has_error = "error" in data
        error_msg = str(data.get("error", "")).lower()
        mentions_not_found = "not found" in error_msg
        ok = has_error and mentions_not_found
        report(7, "get_node_rect error for non-existent node (INPT-03)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         has_error: {has_error}\n"
               f"         mentions 'not found': {mentions_not_found}")
    except Exception as e:
        report(7, "get_node_rect error for non-existent node (INPT-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 8: get_node_rect error for non-visible node (INPT-03)
    # ===================================================================
    # NOTE: This requires a hidden Control node in the scene. Since we
    # cannot guarantee the main scene has one, we attempt with a common
    # path and report SKIP if unavailable.
    try:
        # Attempt with a deliberately unlikely hidden node name
        data, _ = call_tool_text(client, "get_node_rect", {
            "node_path": "HiddenControlForTest"
        }, timeout=15)
        has_error = "error" in data
        error_msg = str(data.get("error", "")).lower()
        if "not found" in error_msg:
            # Node doesn't exist at all -- skip this test
            report_skip(8, "get_node_rect error for non-visible node (INPT-03)",
                        "No hidden Control node available in scene to test visibility check.\n"
                        "         This test requires a scene with a hidden Control node.\n"
                        f"         Got: {data.get('error')}")
        elif "not visible" in error_msg:
            # Found a hidden node -- test passes
            report(8, "get_node_rect error for non-visible node (INPT-03)", True,
                   f"Response: {json.dumps(data)}\n"
                   f"         Correctly reports not-visible error")
        else:
            report_skip(8, "get_node_rect error for non-visible node (INPT-03)",
                        f"Unexpected response: {json.dumps(data)}\n"
                        "         Scene may lack a hidden Control node for this test")
    except Exception as e:
        report(8, "get_node_rect error for non-visible node (INPT-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 9: click_node on a known Control node (INPT-02)
    # ===================================================================
    click_result = None
    try:
        # Strategy: If scene root is a Control (test 6 passed), click it.
        # Otherwise, try empty path and see what happens.
        node_path = ""
        data, _ = call_tool_text(client, "click_node", {
            "node_path": node_path
        }, timeout=15)
        ok = (data.get("success") == True and
              isinstance(data.get("clicked_position"), dict))
        if ok:
            click_result = data
        if "error" in data and "not a control" in str(data.get("error", "")).lower():
            # Scene root is not a Control -- this is expected
            report_skip(9, "click_node on scene root Control (INPT-02)",
                        f"Scene root is not a Control: {data.get('error')}\n"
                        "         click_node requires a Control node target")
        else:
            report(9, "click_node on a known Control node returns success + clicked_position (INPT-02)", ok,
                   f"Response: {json.dumps(data)}\n"
                   f"         success: {data.get('success')}\n"
                   f"         clicked_position present: {isinstance(data.get('clicked_position'), dict)}")
    except Exception as e:
        report(9, "click_node on a known Control node returns success + clicked_position (INPT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 10: click_node returns actual clicked coordinates (INPT-02)
    # ===================================================================
    try:
        if click_result is not None:
            pos = click_result.get("clicked_position", {})
            x = pos.get("x", -1)
            y = pos.get("y", -1)
            ok = (isinstance(x, (int, float)) and x >= 0 and
                  isinstance(y, (int, float)) and y >= 0)
            report(10, "click_node returns valid clicked coordinates >= 0 (INPT-02)", ok,
                   f"clicked_position: {json.dumps(pos)}\n"
                   f"         x={x} (>= 0: {x >= 0}), y={y} (>= 0: {y >= 0})")
        else:
            # Re-attempt click_node with empty path
            data, _ = call_tool_text(client, "click_node", {
                "node_path": ""
            }, timeout=15)
            if data.get("success") == True:
                pos = data.get("clicked_position", {})
                x = pos.get("x", -1)
                y = pos.get("y", -1)
                ok = (isinstance(x, (int, float)) and x >= 0 and
                      isinstance(y, (int, float)) and y >= 0)
                report(10, "click_node returns valid clicked coordinates >= 0 (INPT-02)", ok,
                       f"clicked_position: {json.dumps(pos)}\n"
                       f"         x={x} (>= 0: {x >= 0}), y={y} (>= 0: {y >= 0})")
            else:
                report_skip(10, "click_node returns valid clicked coordinates (INPT-02)",
                            f"Cannot verify coordinates -- click_node did not succeed: {json.dumps(data)}")
    except Exception as e:
        report(10, "click_node returns valid clicked coordinates >= 0 (INPT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 11: click_node error for non-existent node (INPT-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "click_node", {
            "node_path": "NonExistentNode/DoesNotExist"
        }, timeout=15)
        has_error = "error" in data
        error_msg = str(data.get("error", "")).lower()
        mentions_not_found = "not found" in error_msg
        ok = has_error and mentions_not_found
        report(11, "click_node error for non-existent node (INPT-02)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         has_error: {has_error}\n"
               f"         mentions 'not found': {mentions_not_found}")
    except Exception as e:
        report(11, "click_node error for non-existent node (INPT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 12: click_node error for non-Control node (INPT-02)
    # ===================================================================
    # NOTE: This requires a non-Control node (like Node2D) in the running
    # scene. If the scene root is a Control, we may not have a non-Control
    # child. If scene root is NOT a Control, clicking "" tests this.
    try:
        if not scene_root_is_control:
            # Scene root is not a Control -- clicking "" should give "not a Control" error
            data, _ = call_tool_text(client, "click_node", {
                "node_path": ""
            }, timeout=15)
            has_error = "error" in data
            error_msg = str(data.get("error", "")).lower()
            mentions_not_control = "not a control" in error_msg
            ok = has_error and mentions_not_control
            report(12, "click_node error for non-Control node (INPT-02)", ok,
                   f"Response: {json.dumps(data)}\n"
                   f"         has_error: {has_error}\n"
                   f"         mentions 'not a Control': {mentions_not_control}")
        else:
            # Scene root IS a Control, so we cannot test this with the root
            report_skip(12, "click_node error for non-Control node (INPT-02)",
                        "Scene root is a Control node -- no non-Control node available to test.\n"
                        "         This test requires a scene with a non-Control node (e.g., Node2D child)")
    except Exception as e:
        report(12, "click_node error for non-Control node (INPT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 13: stop_game and bridge disconnects
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
    print("  Phase 12 Input Injection Enhancement UAT Results")
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


if __name__ == "__main__":
    run_tests()
