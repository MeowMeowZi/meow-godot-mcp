#!/usr/bin/env python3
"""
Phase 11 UAT - Automated end-to-end tests for Prompt Templates (PMPT-01, PMPT-02).

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase11.py

Tests cover:
  - prompts/list returns 6 prompts (4 v1.0 + 2 v1.1)
  - build_ui_layout prompt returns workflow with UI tool names (PMPT-01)
  - build_ui_layout with layout_type parameter customizes output
  - setup_animation prompt returns workflow with animation tool names (PMPT-02)
  - setup_animation with animation_type parameter customizes output
  - Both new prompts have correct argument schemas
  - Existing v1.0 prompts still work
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

    # ===================================================================
    print("=" * 60)
    print("  Phase 11 UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: prompts/list returns 6 prompts including new ones
    # ===================================================================
    try:
        resp = client.request("prompts/list")
        prompts = resp.get("result", {}).get("prompts", [])
        names = [p["name"] for p in prompts]
        expected_v10 = ["create_player_controller", "setup_scene_structure", "debug_physics", "create_ui_interface"]
        expected_v11 = ["build_ui_layout", "setup_animation"]
        has_v10 = all(n in names for n in expected_v10)
        has_v11 = all(n in names for n in expected_v11)
        ok = len(prompts) == 6 and has_v10 and has_v11
        report(1, "prompts/list returns 6 prompts including new ones", ok,
               f"Count: {len(prompts)}, Names: {names}\n"
               f"         v1.0 present: {has_v10}, v1.1 present: {has_v11}")
    except Exception as e:
        report(1, "prompts/list returns 6 prompts including new ones", False, f"Error: {e}")

    # ===================================================================
    # Test 2: build_ui_layout returns UI workflow (PMPT-01)
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "build_ui_layout",
            "arguments": {"layout_type": "main_menu"}
        })
        result = resp.get("result", {})
        has_desc = "description" in result and len(result["description"]) > 0
        has_msgs = "messages" in result and len(result["messages"]) > 0
        text = ""
        msg_ok = False
        if has_msgs:
            msg = result["messages"][0]
            msg_ok = msg.get("role") == "user" and "text" in msg.get("content", {})
            text = msg["content"]["text"]
        checks = {
            "create_node": "create_node" in text,
            "set_layout_preset": "set_layout_preset" in text,
            "set_theme_override": "set_theme_override" in text,
            "create_stylebox": "create_stylebox" in text,
            "set_container_layout": "set_container_layout" in text,
            "get_ui_properties": "get_ui_properties" in text,
            "save_scene": "save_scene" in text,
            "Main Menu": "Main Menu" in text,
        }
        all_tools = all(checks.values())
        ok = has_desc and has_msgs and msg_ok and all_tools
        missing = [k for k, v in checks.items() if not v]
        report(2, "build_ui_layout returns UI workflow (PMPT-01)", ok,
               f"description: {result.get('description', 'MISSING')[:60]}...\n"
               f"         messages: {len(result.get('messages', []))} message(s)\n"
               f"         tool checks: {sum(checks.values())}/{len(checks)} passed" +
               (f"\n         missing: {missing}" if missing else ""))
    except Exception as e:
        report(2, "build_ui_layout returns UI workflow (PMPT-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 3: build_ui_layout with default parameter
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "build_ui_layout",
            "arguments": {}
        })
        result = resp.get("result", {})
        text = result.get("messages", [{}])[0].get("content", {}).get("text", "")
        ok = "Main Menu" in text
        report(3, "build_ui_layout with default parameter", ok,
               f"Contains 'Main Menu': {ok}")
    except Exception as e:
        report(3, "build_ui_layout with default parameter", False, f"Error: {e}")

    # ===================================================================
    # Test 4: build_ui_layout with hud variant
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "build_ui_layout",
            "arguments": {"layout_type": "hud"}
        })
        result = resp.get("result", {})
        text = result.get("messages", [{}])[0].get("content", {}).get("text", "")
        has_create_node = "create_node" in text
        has_set_layout = "set_layout_preset" in text
        not_main_menu = "Main Menu" not in text
        ok = has_create_node and has_set_layout and not_main_menu
        report(4, "build_ui_layout with hud variant", ok,
               f"Has create_node: {has_create_node}, Has set_layout_preset: {has_set_layout}\n"
               f"         Does NOT contain 'Main Menu': {not_main_menu}")
    except Exception as e:
        report(4, "build_ui_layout with hud variant", False, f"Error: {e}")

    # ===================================================================
    # Test 5: setup_animation returns animation workflow (PMPT-02)
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "setup_animation",
            "arguments": {"animation_type": "ui_transition"}
        })
        result = resp.get("result", {})
        has_desc = "description" in result and len(result["description"]) > 0
        has_msgs = "messages" in result and len(result["messages"]) > 0
        text = ""
        if has_msgs:
            text = result["messages"][0].get("content", {}).get("text", "")
        checks = {
            "create_animation": "create_animation" in text,
            "add_animation_track": "add_animation_track" in text,
            "set_keyframe": "set_keyframe" in text,
            "set_animation_properties": "set_animation_properties" in text,
            "get_animation_info": "get_animation_info" in text,
        }
        all_tools = all(checks.values())
        ok = has_desc and has_msgs and all_tools
        missing = [k for k, v in checks.items() if not v]
        report(5, "setup_animation returns animation workflow (PMPT-02)", ok,
               f"description: {result.get('description', 'MISSING')[:60]}...\n"
               f"         messages: {len(result.get('messages', []))} message(s)\n"
               f"         tool checks: {sum(checks.values())}/{len(checks)} passed" +
               (f"\n         missing: {missing}" if missing else ""))
    except Exception as e:
        report(5, "setup_animation returns animation workflow (PMPT-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 6: setup_animation with default parameter
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "setup_animation",
            "arguments": {}
        })
        result = resp.get("result", {})
        text = result.get("messages", [{}])[0].get("content", {}).get("text", "")
        ok = "UI Transition" in text
        report(6, "setup_animation with default parameter", ok,
               f"Contains 'UI Transition': {ok}")
    except Exception as e:
        report(6, "setup_animation with default parameter", False, f"Error: {e}")

    # ===================================================================
    # Test 7: setup_animation with walk_cycle variant
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "setup_animation",
            "arguments": {"animation_type": "walk_cycle"}
        })
        result = resp.get("result", {})
        text = result.get("messages", [{}])[0].get("content", {}).get("text", "")
        has_create_anim = "create_animation" in text
        has_keyframe = "set_keyframe" in text
        has_loop = "loop" in text.lower()
        ok = has_create_anim and has_keyframe and has_loop
        report(7, "setup_animation with walk_cycle variant", ok,
               f"Has create_animation: {has_create_anim}, Has set_keyframe: {has_keyframe}\n"
               f"         Contains 'loop': {has_loop}")
    except Exception as e:
        report(7, "setup_animation with walk_cycle variant", False, f"Error: {e}")

    # ===================================================================
    # Test 8: Both prompts have correct argument schemas
    # ===================================================================
    try:
        resp = client.request("prompts/list")
        prompts = resp.get("result", {}).get("prompts", [])
        prompt_map = {p["name"]: p for p in prompts}

        # build_ui_layout schema
        bui = prompt_map.get("build_ui_layout", {})
        bui_args = bui.get("arguments", [])
        bui_ok = (len(bui_args) == 1
                  and bui_args[0].get("name") == "layout_type"
                  and bui_args[0].get("required") == False)

        # setup_animation schema
        sa = prompt_map.get("setup_animation", {})
        sa_args = sa.get("arguments", [])
        sa_ok = (len(sa_args) == 1
                 and sa_args[0].get("name") == "animation_type"
                 and sa_args[0].get("required") == False)

        ok = bui_ok and sa_ok
        report(8, "Both prompts have correct argument schemas", ok,
               f"build_ui_layout args: {bui_args}\n"
               f"         setup_animation args: {sa_args}")
    except Exception as e:
        report(8, "Both prompts have correct argument schemas", False, f"Error: {e}")

    # ===================================================================
    # Test 9: Existing v1.0 prompt still works (regression)
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "create_player_controller",
            "arguments": {"movement_type": "2d_platformer"}
        })
        result = resp.get("result", {})
        has_msgs = "messages" in result and len(result["messages"]) > 0
        text = ""
        if has_msgs:
            text = result["messages"][0].get("content", {}).get("text", "")
        has_movement = "2d_platformer" in text
        ok = has_msgs and has_movement
        report(9, "Existing v1.0 prompt still works (regression)", ok,
               f"Has messages: {has_msgs}, Contains '2d_platformer': {has_movement}")
    except Exception as e:
        report(9, "Existing v1.0 prompt still works (regression)", False, f"Error: {e}")

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 11 UAT Test Results")
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
        print("\n  All Phase 11 automated tests passed!")
        sys.exit(0)


if __name__ == "__main__":
    run_tests()
