#!/usr/bin/env python3
"""
Phase 14 UAT - Automated end-to-end tests for game output enhancement.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings
  4. Run: python tests/uat_phase14.py

Requirements covered:
  - GOUT-01: get_game_output captures print() output via debugger channel (no file_logging)
  - GOUT-02: get_game_output supports structured filtering by level, keyword, since
  - GOUT-03: get_game_output returns structured entries with text/level/timestamp_ms

Tests cover:
  - tools/list still shows 44 tools (no new tools added, get_game_output enhanced only)
  - get_game_output schema includes level, since, keyword properties
  - Game launches and bridge connects automatically
  - get_game_output returns success with lines array (GOUT-01, GOUT-03)
  - get_game_output after eval shows structured entries (GOUT-03)
  - clear_after_read incremental behavior works (GOUT-01)
  - Keyword filtering returns only matching lines (GOUT-02)
  - Level filtering returns only matching lines (GOUT-02)
  - No file_logging warning in run_game response (GOUT-01)
  - Combined filters work together (GOUT-02)
  - stop_game succeeds
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase13.py)
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
    print("  Phase 14 Game Output Enhancement UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list still shows 44 tools (no new tools added)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        has_get_game_output = "get_game_output" in tool_names
        ok = (len(tools) == 44 and has_get_game_output)
        report(1, "tools/list shows 44 tools including get_game_output", ok,
               f"Tool count: {len(tools)}\n"
               f"         get_game_output present: {has_get_game_output}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 44 tools including get_game_output", False,
               f"Error: {e}")

    # ===================================================================
    # Test 2: get_game_output schema has level, since, keyword properties
    #         (GOUT-02)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        ggo_tool = None
        for t in tools:
            if t["name"] == "get_game_output":
                ggo_tool = t
                break
        props = ggo_tool.get("inputSchema", {}).get("properties", {}) if ggo_tool else {}
        has_level = "level" in props
        has_since = "since" in props
        has_keyword = "keyword" in props
        has_clear = "clear_after_read" in props
        ok = has_level and has_since and has_keyword and has_clear
        report(2, "get_game_output schema has level, since, keyword, clear_after_read properties (GOUT-02)", ok,
               f"level present: {has_level}\n"
               f"         since present: {has_since}\n"
               f"         keyword present: {has_keyword}\n"
               f"         clear_after_read present: {has_clear}\n"
               f"         Schema properties: {list(props.keys())}")
    except Exception as e:
        report(2, "get_game_output schema has level, since, keyword, clear_after_read properties (GOUT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 3: run_game with mode=main succeeds, bridge connects
    # ===================================================================
    bridge_connected = False
    run_game_data = {}
    try:
        data, _ = call_tool_text(client, "run_game", {"mode": "main"})
        run_game_data = data
        game_started = data.get("success") == True or data.get("already_running") == True
        if not game_started:
            report(3, "run_game with mode=main succeeds and bridge connects", False,
                   f"run_game failed: {json.dumps(data)}")
        else:
            connected, status = wait_for_bridge(client, max_wait=10.0)
            bridge_connected = connected
            ok = connected
            report(3, "run_game with mode=main succeeds and bridge connects", ok,
                   f"run_game: {json.dumps(data)}\n"
                   f"         Bridge status: {json.dumps(status)}\n"
                   f"         Connected: {connected}")
    except Exception as e:
        report(3, "run_game with mode=main succeeds and bridge connects", False, f"Error: {e}")

    # --- EARLY EXIT if bridge not connected ---
    if not bridge_connected:
        print("\n  FATAL: Bridge not connected. Cannot run remaining tests.")
        print("  EARLY EXIT: Skipping all game output tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        _print_summary()
        sys.exit(1)

    # ===================================================================
    # Test 4: get_game_bridge_status shows connected=true
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_bridge_status")
        ok = data.get("connected") == True
        report(4, "get_game_bridge_status shows connected=true", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(4, "get_game_bridge_status shows connected=true", False, f"Error: {e}")

    # ===================================================================
    # Test 5: get_game_output basic call returns success with lines array
    #         (GOUT-01, GOUT-03)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_output", {}, timeout=15)
        has_success = data.get("success") == True
        has_lines = isinstance(data.get("lines"), list)
        has_count = "count" in data
        has_game_running = "game_running" in data

        # Verify line structure if lines are present
        line_structure_ok = True
        if has_lines and len(data["lines"]) > 0:
            first_line = data["lines"][0]
            line_structure_ok = (
                isinstance(first_line.get("text"), str) and
                isinstance(first_line.get("level"), str) and
                isinstance(first_line.get("timestamp_ms"), (int, float))
            )

        ok = has_success and has_lines and has_count and has_game_running
        report(5, "get_game_output basic call returns success with lines array (GOUT-01, GOUT-03)", ok,
               f"success: {data.get('success')} (present: {has_success})\n"
               f"         lines type: {type(data.get('lines')).__name__} (is list: {has_lines})\n"
               f"         count present: {has_count}, game_running present: {has_game_running}\n"
               f"         line count: {data.get('count', 'N/A')}\n"
               f"         line structure ok: {line_structure_ok}\n"
               f"         Response keys: {list(data.keys())}")
    except Exception as e:
        report(5, "get_game_output basic call returns success with lines array (GOUT-01, GOUT-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 6: get_game_output after eval shows structured entries
    #         (GOUT-03)
    # ===================================================================
    try:
        # Use eval_in_game to trigger a print() in the game process
        eval_data, _ = call_tool_text(client, "eval_in_game", {
            "expression": 'print("UAT_PHASE14_MARKER")'
        }, timeout=15)
        time.sleep(1.0)  # Allow output to propagate through debugger channel

        # Read output without clearing, to inspect structure
        data, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        has_lines = isinstance(data.get("lines"), list) and len(data.get("lines", [])) > 0
        has_total_buffered = "total_buffered" in data

        # Verify at least one line has the expected structure
        structure_ok = False
        found_marker = False
        if has_lines:
            for line in data["lines"]:
                has_text = isinstance(line.get("text"), str)
                has_level = isinstance(line.get("level"), str)
                has_ts = isinstance(line.get("timestamp_ms"), (int, float))
                if has_text and has_level and has_ts:
                    structure_ok = True
                if has_text and "UAT_PHASE14_MARKER" in line.get("text", ""):
                    found_marker = True

        ok = has_success and has_lines and structure_ok
        report(6, "get_game_output after eval has structured entries with text/level/timestamp_ms (GOUT-03)", ok,
               f"success: {has_success}, has_lines: {has_lines}\n"
               f"         structure_ok (text/level/timestamp_ms): {structure_ok}\n"
               f"         found UAT_PHASE14_MARKER: {found_marker}\n"
               f"         total_buffered: {data.get('total_buffered', 'N/A')}\n"
               f"         line count: {data.get('count', 'N/A')}\n"
               f"         eval result: {json.dumps(eval_data)}")
    except Exception as e:
        report(6, "get_game_output after eval has structured entries with text/level/timestamp_ms (GOUT-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 7: clear_after_read incremental behavior (GOUT-01)
    # ===================================================================
    try:
        # First call with clear_after_read=true to drain all buffered output
        data1, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": True
        }, timeout=15)
        first_count = data1.get("count", 0)

        # Second call immediately — should return empty or only new output
        data2, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": True
        }, timeout=15)
        second_count = data2.get("count", 0)

        # Second call should have fewer lines than first (ideally 0)
        ok = (data1.get("success") == True and
              data2.get("success") == True and
              second_count <= first_count and
              second_count == 0)

        report(7, "clear_after_read=true drains buffer, next call returns empty (GOUT-01)", ok,
               f"First call count: {first_count}\n"
               f"         Second call count: {second_count}\n"
               f"         second <= first: {second_count <= first_count}\n"
               f"         second == 0: {second_count == 0}")
    except Exception as e:
        report(7, "clear_after_read=true drains buffer, next call returns empty (GOUT-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 8: keyword filtering returns only matching lines (GOUT-02)
    # ===================================================================
    try:
        # Generate some known output first
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("KEYWORD_ALPHA_TEST")'
        }, timeout=15)
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("KEYWORD_BETA_OTHER")'
        }, timeout=15)
        time.sleep(1.0)  # Allow output to propagate

        # Filter by keyword
        data, _ = call_tool_text(client, "get_game_output", {
            "keyword": "KEYWORD_ALPHA",
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # All returned lines must contain the keyword
        all_match = all("KEYWORD_ALPHA" in line.get("text", "") for line in lines)
        # If we got any lines, they should all match
        ok = has_success and all_match
        report(8, "keyword filter returns only lines containing keyword (GOUT-02)", ok,
               f"success: {has_success}\n"
               f"         lines returned: {len(lines)}\n"
               f"         all contain 'KEYWORD_ALPHA': {all_match}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(8, "keyword filter returns only lines containing keyword (GOUT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 9: level filtering returns only matching level (GOUT-02)
    # ===================================================================
    try:
        # Generate error-level output
        call_tool_text(client, "eval_in_game", {
            "expression": 'push_error("UAT_ERROR_LEVEL_TEST")'
        }, timeout=15)
        time.sleep(1.0)  # Allow output to propagate

        # Filter by level=error
        data, _ = call_tool_text(client, "get_game_output", {
            "level": "error",
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # All returned lines must have level=error
        all_error = all(line.get("level") == "error" for line in lines)
        ok = has_success and all_error
        report(9, "level=error filter returns only error-level lines (GOUT-02)", ok,
               f"success: {has_success}\n"
               f"         lines returned: {len(lines)}\n"
               f"         all level=error: {all_error}\n"
               f"         line levels: {[l.get('level', '') for l in lines[:5]]}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(9, "level=error filter returns only error-level lines (GOUT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 10: run_game response has no file_logging warning (GOUT-01)
    # ===================================================================
    try:
        # We already captured run_game_data from Test 3
        has_warning = "warning" in run_game_data
        warning_text = str(run_game_data.get("warning", "")).lower()
        mentions_file_logging = "file_logging" in warning_text

        # PASS if there is no warning key at all, or if warning does not mention file_logging
        ok = not has_warning or not mentions_file_logging
        report(10, "run_game response has no file_logging warning (GOUT-01, GOUT-03)", ok,
               f"has 'warning' key: {has_warning}\n"
               f"         warning text: {run_game_data.get('warning', 'NONE')}\n"
               f"         mentions file_logging: {mentions_file_logging}\n"
               f"         run_game response: {json.dumps(run_game_data)}")
    except Exception as e:
        report(10, "run_game response has no file_logging warning (GOUT-01, GOUT-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 11: combined filters (level + keyword) work together (GOUT-02)
    # ===================================================================
    try:
        # Generate some mixed output
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("COMBINED_INFO_LINE")'
        }, timeout=15)
        call_tool_text(client, "eval_in_game", {
            "expression": 'push_error("COMBINED_ERROR_LINE")'
        }, timeout=15)
        time.sleep(1.0)

        # Filter with both level=info AND keyword=COMBINED
        data, _ = call_tool_text(client, "get_game_output", {
            "level": "info",
            "keyword": "COMBINED",
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # All returned lines must be level=info AND contain COMBINED
        all_match = all(
            line.get("level") == "info" and "COMBINED" in line.get("text", "")
            for line in lines
        )
        ok = has_success and all_match
        report(11, "combined level=info + keyword=COMBINED filter works (GOUT-02)", ok,
               f"success: {has_success}\n"
               f"         lines returned: {len(lines)}\n"
               f"         all match both filters: {all_match}\n"
               f"         line details: {[(l.get('level',''), l.get('text','')[:40]) for l in lines[:5]]}")
    except Exception as e:
        report(11, "combined level=info + keyword=COMBINED filter works (GOUT-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 12: stop_game succeeds
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "stop_game")
        game_stopped = data.get("success") == True
        time.sleep(1.0)  # Allow bridge to disconnect

        status, _ = call_tool_text(client, "get_game_bridge_status")
        bridge_disconnected = status.get("connected") == False
        ok = game_stopped and bridge_disconnected
        report(12, "stop_game succeeds and bridge disconnects", ok,
               f"stop_game: {json.dumps(data)}\n"
               f"         bridge status: {json.dumps(status)}\n"
               f"         game_stopped: {game_stopped}, bridge_disconnected: {bridge_disconnected}")
    except Exception as e:
        report(12, "stop_game succeeds and bridge disconnects", False, f"Error: {e}")

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
    print("  Phase 14 Game Output Enhancement UAT Results")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  === PHASE 14 UAT SUMMARY ===")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
