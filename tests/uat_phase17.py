#!/usr/bin/env python3
"""
Phase 17 UAT - Reliable Game Output (DX-03)
Verifies companion-side log forwarding captures print() output within 1 second.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings (project/test_root.tscn)
  4. Run: python tests/uat_phase17.py

Requirements covered:
  - DX-03: print() output captured within 1 second via companion log forwarding

Tests cover:
  1. MCP connection and handshake
  2. run_game with wait_for_bridge=true succeeds and bridge connects
  3. Print output capture via eval_in_game (meow_test_17)
  4. Output capture latency under 1 second (0.5s sleep)
  5. Level classification for push_error() (meow_err_17)
  6. Level filtering (level=error returns only error entries)
  7. Keyword filtering (keyword=meow_test_17)
  8. clear_after_read incremental reads (incremental_17)
  9. Structured response shape (success, lines, count, total_buffered, game_running)
  10. stop_game succeeds and bridge disconnects
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

    # ===================================================================
    # Test 1: MCP connection and handshake
    # ===================================================================
    print("\nConnecting to Godot MCP server (127.0.0.1:6800)...")
    try:
        client.connect()
        init_resp = client.request("initialize")
        server_name = init_resp["result"]["serverInfo"]["name"]
        server_version = init_resp["result"]["serverInfo"]["version"]
        client.notify("notifications/initialized")
        time.sleep(0.3)
        ok = server_name == "meow-godot-mcp"
        report(1, "MCP connection and handshake", ok,
               f"Server: {server_name} v{server_version}")
    except Exception as e:
        report(1, "MCP connection and handshake", False, f"Error: {e}")
        print("\n  FATAL: Cannot connect to MCP server. Aborting.")
        print("  Please ensure the Godot editor is open with the MCP Meow plugin enabled.")
        _print_summary()
        sys.exit(1)

    print()
    print("=" * 60)
    print("  Phase 17 Reliable Game Output UAT (DX-03)")
    print("=" * 60)

    # ===================================================================
    # Test 2: run_game with wait_for_bridge=true succeeds and bridge connects
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
        already_running = data.get("already_running") == True
        got_bridge = data.get("bridge_connected") == True
        bridge_connected = got_bridge
        ok = not is_error and (success or already_running) and got_bridge
        report(2, "run_game with wait_for_bridge=true succeeds, bridge connects", ok,
               f"isError: {is_error}\n"
               f"         success: {success}, already_running: {already_running}\n"
               f"         bridge_connected: {got_bridge}\n"
               f"         Response: {raw_text[:400]}")
    except Exception as e:
        report(2, "run_game with wait_for_bridge=true succeeds, bridge connects", False,
               f"Error: {e}")

    # --- EARLY EXIT if bridge not connected ---
    if not bridge_connected:
        print("\n  FATAL: Bridge not connected after wait_for_bridge. Cannot run game output tests.")
        print("  EARLY EXIT: Skipping remaining bridge-dependent tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        _print_summary()
        sys.exit(1)

    # Drain any existing buffered output before tests
    try:
        call_tool_text(client, "get_game_output", {"clear_after_read": True}, timeout=15)
    except Exception:
        pass
    time.sleep(0.3)

    # ===================================================================
    # Test 3: Print output capture via eval_in_game
    # ===================================================================
    try:
        # Trigger a print in the game process
        eval_data, _ = call_tool_text(client, "eval_in_game", {
            "expression": 'print("meow_test_17")'
        }, timeout=15)
        time.sleep(1.0)  # Well within 200ms poll + processing margin

        # Read output
        data, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # Check if meow_test_17 appears in any line
        found_marker = False
        for line in lines:
            if "meow_test_17" in line.get("text", ""):
                found_marker = True
                break

        ok = has_success and found_marker
        report(3, "Print output capture via eval_in_game (meow_test_17)", ok,
               f"success: {has_success}\n"
               f"         found 'meow_test_17' in output: {found_marker}\n"
               f"         line count: {data.get('count', 'N/A')}\n"
               f"         eval result: {json.dumps(eval_data)}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(3, "Print output capture via eval_in_game (meow_test_17)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: Output capture latency -- sub-1s requirement
    # ===================================================================
    try:
        # Drain buffer first
        call_tool_text(client, "get_game_output", {"clear_after_read": True}, timeout=15)
        time.sleep(0.2)

        # Trigger a print
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("latency_check")'
        }, timeout=15)

        # Wait exactly 0.5 seconds (testing sub-1s requirement)
        time.sleep(0.5)

        # Read output
        data, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        found_latency = False
        for line in lines:
            if "latency_check" in line.get("text", ""):
                found_latency = True
                break

        ok = has_success and found_latency
        report(4, "Output capture latency under 1 second (0.5s sleep)", ok,
               f"success: {has_success}\n"
               f"         found 'latency_check' after 0.5s: {found_latency}\n"
               f"         line count: {data.get('count', 'N/A')}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(4, "Output capture latency under 1 second (0.5s sleep)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 5: Level classification -- push_error()
    # ===================================================================
    try:
        # Drain buffer first
        call_tool_text(client, "get_game_output", {"clear_after_read": True}, timeout=15)
        time.sleep(0.2)

        # Trigger a push_error in the game
        call_tool_text(client, "eval_in_game", {
            "expression": 'push_error("meow_err_17")'
        }, timeout=15)
        time.sleep(0.5)

        # Read output
        data, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # Check for an error-level line containing meow_err_17
        found_error = False
        error_level = None
        for line in lines:
            if "meow_err_17" in line.get("text", ""):
                found_error = True
                error_level = line.get("level", "")
                break

        ok = has_success and found_error and error_level == "error"
        report(5, "Level classification -- push_error() classified as error (meow_err_17)", ok,
               f"success: {has_success}\n"
               f"         found 'meow_err_17': {found_error}\n"
               f"         level: {error_level} (expected: error)\n"
               f"         line count: {data.get('count', 'N/A')}\n"
               f"         line details: {[(l.get('level',''), l.get('text','')[:60]) for l in lines[:5]]}")
    except Exception as e:
        report(5, "Level classification -- push_error() classified as error (meow_err_17)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 6: Level filtering (level=error)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_output", {
            "level": "error",
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # All returned lines must have level=error
        all_error = all(line.get("level") == "error" for line in lines) if lines else True
        ok = has_success and all_error
        report(6, "Level filtering (level=error returns only error entries)", ok,
               f"success: {has_success}\n"
               f"         lines returned: {len(lines)}\n"
               f"         all level=error: {all_error}\n"
               f"         line levels: {[l.get('level', '') for l in lines[:5]]}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(6, "Level filtering (level=error returns only error entries)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 7: Keyword filtering (keyword=meow_test_17)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "get_game_output", {
            "keyword": "meow_test_17",
            "clear_after_read": False
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # All returned lines must contain the keyword
        all_match = all("meow_test_17" in line.get("text", "") for line in lines) if lines else True
        has_results = len(lines) > 0
        ok = has_success and all_match and has_results
        report(7, "Keyword filtering (keyword=meow_test_17)", ok,
               f"success: {has_success}\n"
               f"         lines returned: {len(lines)}\n"
               f"         all contain 'meow_test_17': {all_match}\n"
               f"         has results: {has_results}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(7, "Keyword filtering (keyword=meow_test_17)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 8: clear_after_read incremental reads
    # ===================================================================
    try:
        # Drain all existing buffer
        call_tool_text(client, "get_game_output", {"clear_after_read": True}, timeout=15)
        time.sleep(0.2)

        # Trigger a new print
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("incremental_17")'
        }, timeout=15)
        time.sleep(0.5)

        # Read with clear_after_read=false to check content
        data, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": True
        }, timeout=15)
        has_success = data.get("success") == True
        lines = data.get("lines", [])

        # Should contain incremental_17
        found_incremental = False
        for line in lines:
            if "incremental_17" in line.get("text", ""):
                found_incremental = True
                break

        # Should NOT contain previous test prints (they were cleared)
        found_old = False
        for line in lines:
            text = line.get("text", "")
            # Check for test markers from test 3 (meow_test_17 was not cleared since test 3
            # used clear_after_read=False, but we drained at the top of this test)
            if "latency_check" in text or "meow_err_17" in text:
                found_old = True
                break

        ok = has_success and found_incremental and not found_old
        report(8, "clear_after_read incremental reads (incremental_17)", ok,
               f"success: {has_success}\n"
               f"         found 'incremental_17': {found_incremental}\n"
               f"         found old markers: {found_old} (expected: False)\n"
               f"         line count: {data.get('count', 'N/A')}\n"
               f"         line texts: {[l.get('text', '')[:60] for l in lines[:5]]}")
    except Exception as e:
        report(8, "clear_after_read incremental reads (incremental_17)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 9: Structured response shape
    # ===================================================================
    try:
        # Trigger fresh output for shape inspection
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("shape_test_17")'
        }, timeout=15)
        time.sleep(0.5)

        data, _ = call_tool_text(client, "get_game_output", {
            "clear_after_read": False
        }, timeout=15)

        # Verify top-level keys
        has_success_key = "success" in data
        has_lines_key = "lines" in data and isinstance(data["lines"], list)
        has_count_key = "count" in data
        has_total_buffered_key = "total_buffered" in data
        has_game_running_key = "game_running" in data

        # Verify line entry structure
        line_structure_ok = False
        if has_lines_key and len(data["lines"]) > 0:
            first_line = data["lines"][0]
            has_text = isinstance(first_line.get("text"), str)
            has_level = isinstance(first_line.get("level"), str)
            has_ts = isinstance(first_line.get("timestamp_ms"), (int, float))
            line_structure_ok = has_text and has_level and has_ts

        ok = (has_success_key and has_lines_key and has_count_key and
              has_total_buffered_key and has_game_running_key and line_structure_ok)
        report(9, "Structured response shape (success, lines, count, total_buffered, game_running)", ok,
               f"success key: {has_success_key}\n"
               f"         lines key (list): {has_lines_key}\n"
               f"         count key: {has_count_key}\n"
               f"         total_buffered key: {has_total_buffered_key}\n"
               f"         game_running key: {has_game_running_key}\n"
               f"         line structure (text/level/timestamp_ms): {line_structure_ok}\n"
               f"         Response keys: {list(data.keys())}\n"
               f"         First line keys: {list(data['lines'][0].keys()) if has_lines_key and data['lines'] else 'N/A'}")
    except Exception as e:
        report(9, "Structured response shape (success, lines, count, total_buffered, game_running)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 10: stop_game succeeds and bridge disconnects
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "stop_game")
        game_stopped = data.get("success") == True
        time.sleep(1.0)  # Allow bridge to disconnect

        status, _ = call_tool_text(client, "get_game_bridge_status")
        bridge_disconnected = status.get("connected") == False
        ok = game_stopped and bridge_disconnected
        report(10, "stop_game succeeds and bridge disconnects", ok,
               f"stop_game success: {game_stopped}\n"
               f"         bridge disconnected: {bridge_disconnected}\n"
               f"         bridge status: {json.dumps(status)}")
    except Exception as e:
        report(10, "stop_game succeeds and bridge disconnects", False, f"Error: {e}")

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
    print("  Phase 17 Reliable Game Output UAT Results (DX-03)")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  === PHASE 17 UAT SUMMARY ===")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
