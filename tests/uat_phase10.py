#!/usr/bin/env python3
"""
Phase 10 UAT - Automated end-to-end tests for running game bridge tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings
  4. Run: python tests/uat_phase10.py

Tests cover:
  - tools/list shows 38 tools including 3 game bridge tools (BRDG-01)
  - Game launches and bridge connects automatically (BRDG-01)
  - Bridge status shows connected with session_id (BRDG-01)
  - Key input injection press/release (BRDG-02)
  - Mouse input injection click/move/scroll (BRDG-03)
  - Input action injection press/release (BRDG-04)
  - Game viewport capture as ImageContent PNG (BRDG-05)
  - Base64 PNG signature validation (BRDG-05)
  - Game viewport capture with resize parameter (BRDG-05)
  - Error handling for missing parameters
  - Bridge disconnects when game stops
"""

import base64
import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase9.py)
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


def wait_for_bridge(client, max_wait=10.0, poll_interval=0.5):
    """Poll get_game_bridge_status until connected=true or timeout."""
    elapsed = 0.0
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
    print("  Phase 10 UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 38 tools including game bridge tools
    #         (BRDG-01)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        expected_bridge = ["inject_input", "capture_game_viewport", "get_game_bridge_status"]
        has_all = all(n in tool_names for n in expected_bridge)
        ok = len(tools) == 38 and has_all
        report(1, "tools/list shows 38 tools including 3 game bridge tools (BRDG-01)", ok,
               f"Tool count: {len(tools)}\n"
               f"         Bridge tools present: {has_all}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 38 tools including 3 game bridge tools (BRDG-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 2: run_game and bridge connects (BRDG-01)
    # ===================================================================
    bridge_connected = False
    try:
        data, _ = call_tool_text(client, "run_game", {"mode": "main"})
        game_started = data.get("success") == True or data.get("already_running") == True
        if not game_started:
            report(2, "run_game and bridge connects within 10s (BRDG-01)", False,
                   f"run_game failed: {json.dumps(data)}")
        else:
            # Poll for bridge connection
            connected, status = wait_for_bridge(client, max_wait=10.0)
            bridge_connected = connected
            ok = connected
            report(2, "run_game and bridge connects within 10s (BRDG-01)", ok,
                   f"run_game: {json.dumps(data)}\n"
                   f"         Bridge status: {json.dumps(status)}\n"
                   f"         Connected: {connected}")
    except Exception as e:
        report(2, "run_game and bridge connects within 10s (BRDG-01)", False, f"Error: {e}")

    if not bridge_connected:
        print("\n  FATAL: Bridge not connected. Cannot run remaining tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        # Print summary so far
        _print_summary()
        sys.exit(1)

    # ===================================================================
    # Test 3: get_game_bridge_status shows connected (BRDG-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_bridge_status")
        is_connected = data.get("connected") == True
        has_session = isinstance(data.get("session_id"), int) and data["session_id"] >= 0
        ok = is_connected and has_session
        report(3, "get_game_bridge_status shows connected with session_id (BRDG-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         connected: {is_connected}, session_id >= 0: {has_session}")
    except Exception as e:
        report(3, "get_game_bridge_status shows connected with session_id (BRDG-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: inject_input type=key, pressed=true (BRDG-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "key",
            "keycode": "W",
            "pressed": True
        })
        ok = (data.get("success") == True and
              data.get("type") == "key" and
              data.get("keycode") == "W" and
              data.get("pressed") == True)
        report(4, "inject_input key press W (BRDG-02)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(4, "inject_input key press W (BRDG-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 5: inject_input type=key, pressed=false (BRDG-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "key",
            "keycode": "W",
            "pressed": False
        })
        ok = (data.get("success") == True and
              data.get("type") == "key" and
              data.get("pressed") == False)
        report(5, "inject_input key release W (BRDG-02)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(5, "inject_input key release W (BRDG-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 6: inject_input type=mouse, click (BRDG-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "mouse",
            "mouse_action": "click",
            "position": {"x": 100, "y": 100},
            "button": "left",
            "pressed": True
        })
        ok = (data.get("success") == True and
              data.get("type") == "mouse" and
              data.get("mouse_action") == "click")
        report(6, "inject_input mouse click at (100,100) (BRDG-03)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(6, "inject_input mouse click at (100,100) (BRDG-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 7: inject_input type=mouse, move (BRDG-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "mouse",
            "mouse_action": "move",
            "position": {"x": 200, "y": 200}
        })
        ok = (data.get("success") == True and
              data.get("type") == "mouse" and
              data.get("mouse_action") == "move")
        report(7, "inject_input mouse move to (200,200) (BRDG-03)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(7, "inject_input mouse move to (200,200) (BRDG-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 8: inject_input type=mouse, scroll (BRDG-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "mouse",
            "mouse_action": "scroll",
            "position": {"x": 100, "y": 100},
            "direction": "up"
        })
        ok = (data.get("success") == True and
              data.get("type") == "mouse" and
              data.get("mouse_action") == "scroll")
        report(8, "inject_input mouse scroll up (BRDG-03)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(8, "inject_input mouse scroll up (BRDG-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 9: inject_input type=action, pressed=true (BRDG-04)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "action",
            "action_name": "ui_accept",
            "pressed": True
        })
        ok = (data.get("success") == True and
              data.get("type") == "action" and
              data.get("action_name") == "ui_accept" and
              data.get("pressed") == True)
        report(9, "inject_input action press ui_accept (BRDG-04)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(9, "inject_input action press ui_accept (BRDG-04)", False, f"Error: {e}")

    # ===================================================================
    # Test 10: inject_input type=action, pressed=false (BRDG-04)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {
            "type": "action",
            "action_name": "ui_accept",
            "pressed": False
        })
        ok = (data.get("success") == True and
              data.get("type") == "action" and
              data.get("pressed") == False)
        report(10, "inject_input action release ui_accept (BRDG-04)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(10, "inject_input action release ui_accept (BRDG-04)", False, f"Error: {e}")

    # ===================================================================
    # Test 11: capture_game_viewport returns ImageContent (BRDG-05)
    # ===================================================================
    captured_data = None
    try:
        # Use higher timeout for cross-process viewport capture
        result, _ = call_tool(client, "capture_game_viewport", {}, timeout=15)
        content_list = result.get("content", [])
        has_content = len(content_list) >= 1
        is_image = has_content and content_list[0].get("type") == "image"
        has_data = is_image and len(content_list[0].get("data", "")) > 0
        has_mime = is_image and content_list[0].get("mimeType") == "image/png"
        ok = is_image and has_data and has_mime
        if ok:
            captured_data = content_list[0]["data"]
        report(11, "capture_game_viewport returns ImageContent PNG (BRDG-05)", ok,
               f"Content count: {len(content_list)}\n"
               f"         type: {content_list[0].get('type') if has_content else 'N/A'}\n"
               f"         mimeType: {content_list[0].get('mimeType') if has_content else 'N/A'}\n"
               f"         data length: {len(content_list[0].get('data', '')) if has_content else 0}")
    except Exception as e:
        report(11, "capture_game_viewport returns ImageContent PNG (BRDG-05)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 12: Base64 data decodes to valid PNG (BRDG-05)
    # ===================================================================
    try:
        b64_data = captured_data
        if b64_data is None:
            # Fetch fresh if test 11 failed
            result, _ = call_tool(client, "capture_game_viewport", {}, timeout=15)
            content_list = result.get("content", [])
            if content_list and content_list[0].get("type") == "image":
                b64_data = content_list[0].get("data", "")

        ok = False
        detail = ""
        if b64_data:
            raw = base64.b64decode(b64_data)
            png_signature = b'\x89PNG\r\n\x1a\n'
            ok = raw[:8] == png_signature
            detail = (f"Decoded {len(raw)} bytes\n"
                      f"         First 8 bytes: {raw[:8]!r}\n"
                      f"         PNG signature: {png_signature!r}\n"
                      f"         Match: {ok}")
        else:
            detail = "No base64 data available to decode"
        report(12, "Base64 data decodes to valid PNG signature (BRDG-05)", ok, detail)
    except Exception as e:
        report(12, "Base64 data decodes to valid PNG signature (BRDG-05)", False, f"Error: {e}")

    # ===================================================================
    # Test 13: capture_game_viewport with resize (BRDG-05)
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_game_viewport", {"width": 320}, timeout=15)
        content_list = result.get("content", [])
        has_content = len(content_list) >= 1
        is_image = has_content and content_list[0].get("type") == "image"
        has_data = is_image and len(content_list[0].get("data", "")) > 0
        ok = is_image and has_data
        detail = ""
        if has_content:
            detail = (f"type: {content_list[0].get('type')}\n"
                      f"         data length: {len(content_list[0].get('data', ''))}")
            # Check metadata if present
            if len(content_list) >= 2 and content_list[1].get("type") == "text":
                meta = json.loads(content_list[1]["text"])
                detail += f"\n         metadata: {json.dumps(meta)}"
        report(13, "capture_game_viewport with width=320 returns ImageContent (BRDG-05)", ok,
               detail)
    except Exception as e:
        report(13, "capture_game_viewport with width=320 returns ImageContent (BRDG-05)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 14: inject_input without type returns error
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "inject_input", {"keycode": "W"})
        has_error = "error" in data
        ok = has_error
        report(14, "inject_input without type returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(14, "inject_input without type returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 15: stop_game and bridge disconnects (BRDG-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "stop_game")
        game_stopped = data.get("success") == True
        time.sleep(1.0)  # Allow bridge to disconnect

        status, _ = call_tool_text(client, "get_game_bridge_status")
        bridge_disconnected = status.get("connected") == False
        ok = game_stopped and bridge_disconnected
        report(15, "stop_game succeeds and bridge disconnects (BRDG-01)", ok,
               f"stop_game: {json.dumps(data)}\n"
               f"         bridge status: {json.dumps(status)}\n"
               f"         game_stopped: {game_stopped}, bridge_disconnected: {bridge_disconnected}")
    except Exception as e:
        report(15, "stop_game succeeds and bridge disconnects (BRDG-01)", False, f"Error: {e}")

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
    print("  Phase 10 Running Game Bridge UAT Results")
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
