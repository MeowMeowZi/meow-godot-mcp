#!/usr/bin/env python3
"""
Phase 8 UAT - Automated end-to-end tests for animation system tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase8.py

Tests cover:
  - tools/list shows 34 tools including all Phase 8 animation tools (ANIM-01..05)
  - create_animation creates AnimationPlayer + animation (ANIM-01)
  - create_animation on existing player adds new animation (ANIM-01)
  - create_animation duplicate name returns error (ANIM-01)
  - add_animation_track adds value track (ANIM-02)
  - add_animation_track adds position_3d track (ANIM-02)
  - add_animation_track with invalid type returns error (ANIM-02)
  - set_keyframe inserts keyframe on value track (ANIM-03)
  - set_keyframe inserts keyframe on position_3d track (ANIM-03)
  - set_keyframe updates existing keyframe (ANIM-03)
  - set_keyframe removes keyframe (ANIM-03)
  - get_animation_info returns full structure (ANIM-04)
  - get_animation_info specific animation query (ANIM-04)
  - set_animation_properties sets duration, loop, step (ANIM-05)
  - Round-trip: set_animation_properties then get_animation_info confirms values (ANIM-05)
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase7.py)
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
    assert init_resp["result"]["serverInfo"]["name"] == "meow-godot-mcp"
    client.notify("notifications/initialized")
    time.sleep(0.3)
    print("  Handshake OK: meow-godot-mcp v" + init_resp["result"]["serverInfo"]["version"])
    print()

    # --- Setup: Create test scene with a node to animate ---
    print("  Setting up test scene...")
    setup_data, _ = call_tool(client, "create_scene", {
        "root_type": "Node2D",
        "path": "res://test_anim_phase8.tscn",
        "root_name": "AnimRoot"
    })
    if not setup_data.get("success"):
        print(f"  FATAL: Could not create test scene: {json.dumps(setup_data)}")
        client.close()
        sys.exit(1)
    time.sleep(0.5)

    # Create a child node to animate
    setup_node, _ = call_tool(client, "create_node", {
        "type": "Sprite2D",
        "parent_path": "",
        "name": "TestSprite"
    })
    if not setup_node.get("success"):
        print(f"  WARNING: Could not create TestSprite: {json.dumps(setup_node)}")
    time.sleep(0.3)

    print("  Setup complete.\n")

    print("=" * 60)
    print("  Phase 8 UAT Tests")
    print("=" * 60)

    # Track player_path across tests
    player_path = ""

    # ===================================================================
    # Test 1: tools/list shows 34 tools including all Phase 8 animation
    #         tools (ANIM-01..05)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        expected_phase8 = [
            "create_animation", "add_animation_track",
            "set_keyframe", "get_animation_info",
            "set_animation_properties"
        ]
        has_all = all(n in tool_names for n in expected_phase8)
        ok = len(tools) == 34 and has_all
        report(1, "tools/list shows 34 tools with Phase 8 animation tools (ANIM-01..05)", ok,
               f"Tool count: {len(tools)}\n"
               f"         Phase 8 tools present: {has_all}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 34 tools with Phase 8 animation tools (ANIM-01..05)", False, f"Error: {e}")

    # ===================================================================
    # Test 2: create_animation creates new player + animation (ANIM-01)
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_animation", {
            "animation_name": "idle"
        })
        ok = data.get("success") == True
        has_player_path = "player_path" in data
        has_anim_name = data.get("animation_name") == "idle"
        ok = ok and has_player_path and has_anim_name
        if has_player_path:
            player_path = data["player_path"]
        report(2, "create_animation creates new player + animation (ANIM-01)", ok,
               f"Response: {json.dumps(data)}\n"
               f"         player_path: {player_path}")
    except Exception as e:
        report(2, "create_animation creates new player + animation (ANIM-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 3: create_animation on existing player adds animation (ANIM-01)
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_animation", {
            "animation_name": "walk",
            "player_path": player_path
        })
        ok = data.get("success") == True and data.get("animation_name") == "walk"
        report(3, "create_animation on existing player adds animation (ANIM-01)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(3, "create_animation on existing player adds animation (ANIM-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 4: create_animation duplicate name returns error (ANIM-01)
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_animation", {
            "animation_name": "idle",
            "player_path": player_path
        })
        has_error = "error" in data
        error_msg = data.get("error", "")
        ok = has_error and "already exists" in error_msg.lower()
        report(4, "create_animation duplicate name returns error (ANIM-01)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(4, "create_animation duplicate name returns error (ANIM-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 5: add_animation_track adds value track (ANIM-02)
    # ===================================================================
    try:
        data, _ = call_tool(client, "add_animation_track", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_type": "value",
            "track_path": "TestSprite:modulate"
        })
        ok = (data.get("success") == True and
              data.get("track_index") == 0 and
              data.get("track_type") == "value")
        report(5, "add_animation_track adds value track (ANIM-02)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(5, "add_animation_track adds value track (ANIM-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 6: add_animation_track adds position_3d track (ANIM-02)
    # ===================================================================
    try:
        data, _ = call_tool(client, "add_animation_track", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_type": "position_3d",
            "track_path": "TestSprite"
        })
        ok = data.get("success") == True and data.get("track_index") == 1
        report(6, "add_animation_track adds position_3d track (ANIM-02)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(6, "add_animation_track adds position_3d track (ANIM-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 7: add_animation_track invalid type returns error (ANIM-02)
    # ===================================================================
    try:
        data, _ = call_tool(client, "add_animation_track", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_type": "invalid_type",
            "track_path": "TestSprite"
        })
        has_error = "error" in data
        ok = has_error
        report(7, "add_animation_track invalid type returns error (ANIM-02)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(7, "add_animation_track invalid type returns error (ANIM-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 8: set_keyframe inserts on value track (ANIM-03)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_keyframe", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_index": 0,
            "time": 0.0,
            "action": "insert",
            "value": "Color(1, 0, 0, 1)"
        })
        ok = data.get("success") == True and data.get("action") == "insert"
        report(8, "set_keyframe inserts on value track (ANIM-03)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(8, "set_keyframe inserts on value track (ANIM-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 9: set_keyframe inserts on position_3d track (ANIM-03)
    # ===================================================================
    try:
        # Insert first keyframe at t=0
        data1, _ = call_tool(client, "set_keyframe", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_index": 1,
            "time": 0.0,
            "action": "insert",
            "value": "Vector3(0, 0, 0)"
        })
        ok1 = data1.get("success") == True

        # Insert second keyframe at t=1.0
        data2, _ = call_tool(client, "set_keyframe", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_index": 1,
            "time": 1.0,
            "action": "insert",
            "value": "Vector3(100, 0, 0)"
        })
        ok2 = data2.get("success") == True

        ok = ok1 and ok2
        report(9, "set_keyframe inserts on position_3d track (ANIM-03)", ok,
               f"Key1: {json.dumps(data1)}\n"
               f"         Key2: {json.dumps(data2)}")
    except Exception as e:
        report(9, "set_keyframe inserts on position_3d track (ANIM-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 10: set_keyframe updates existing keyframe (ANIM-03)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_keyframe", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_index": 1,
            "time": 0.0,
            "action": "update",
            "value": "Vector3(10, 10, 0)"
        })
        ok = data.get("success") == True and data.get("action") == "update"
        report(10, "set_keyframe updates existing keyframe (ANIM-03)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(10, "set_keyframe updates existing keyframe (ANIM-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 11: set_keyframe removes keyframe (ANIM-03)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_keyframe", {
            "player_path": player_path,
            "animation_name": "idle",
            "track_index": 1,
            "time": 1.0,
            "action": "remove"
        })
        ok = data.get("success") == True and data.get("action") == "remove"
        report(11, "set_keyframe removes keyframe (ANIM-03)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(11, "set_keyframe removes keyframe (ANIM-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 12: get_animation_info full query (ANIM-04)
    # ===================================================================
    try:
        data, _ = call_tool(client, "get_animation_info", {
            "player_path": player_path
        })
        ok = False
        detail = ""
        if data.get("success") == True:
            animations = data.get("animations", [])
            anim_names = [a["name"] for a in animations]
            has_idle = "idle" in anim_names
            has_walk = "walk" in anim_names
            # Check idle animation has >= 2 tracks
            idle_anim = next((a for a in animations if a["name"] == "idle"), None)
            has_tracks = idle_anim is not None and idle_anim.get("track_count", 0) >= 2
            ok = has_idle and has_walk and has_tracks
            detail = (f"Animation names: {anim_names}\n"
                      f"         idle present: {has_idle}, walk present: {has_walk}\n"
                      f"         idle track_count: {idle_anim.get('track_count', 'N/A') if idle_anim else 'N/A'}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(12, "get_animation_info full query (ANIM-04)", ok, detail)
    except Exception as e:
        report(12, "get_animation_info full query (ANIM-04)", False, f"Error: {e}")

    # ===================================================================
    # Test 13: get_animation_info specific animation (ANIM-04)
    # ===================================================================
    try:
        data, _ = call_tool(client, "get_animation_info", {
            "player_path": player_path,
            "animation_name": "idle"
        })
        ok = False
        detail = ""
        if data.get("success") == True:
            animations = data.get("animations", [])
            # Should return only idle
            anim_names = [a["name"] for a in animations]
            only_idle = len(animations) == 1 and anim_names[0] == "idle"
            # Check keyframe data on tracks
            has_keyframes = False
            if animations:
                tracks = animations[0].get("tracks", [])
                for t in tracks:
                    keys = t.get("keys", [])
                    if keys:
                        # Check keyframe fields: time, value, transition
                        first_key = keys[0]
                        has_keyframes = ("time" in first_key and
                                         "value" in first_key and
                                         "transition" in first_key)
                        break
            ok = only_idle and has_keyframes
            detail = (f"Animation names: {anim_names}, only_idle: {only_idle}\n"
                      f"         has_keyframe_data: {has_keyframes}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(13, "get_animation_info specific animation query (ANIM-04)", ok, detail)
    except Exception as e:
        report(13, "get_animation_info specific animation query (ANIM-04)", False, f"Error: {e}")

    # ===================================================================
    # Test 14: set_animation_properties (ANIM-05)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_animation_properties", {
            "player_path": player_path,
            "animation_name": "idle",
            "length": 2.5,
            "loop_mode": "linear",
            "step": 0.05
        })
        ok = data.get("success") == True
        report(14, "set_animation_properties sets duration, loop, step (ANIM-05)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(14, "set_animation_properties sets duration, loop, step (ANIM-05)", False, f"Error: {e}")

    # ===================================================================
    # Test 15: Round-trip: set_animation_properties then get_animation_info
    #          confirms values (ANIM-05)
    # ===================================================================
    try:
        time.sleep(0.3)
        data, _ = call_tool(client, "get_animation_info", {
            "player_path": player_path,
            "animation_name": "idle"
        })
        ok = False
        detail = ""
        if data.get("success") == True:
            animations = data.get("animations", [])
            if animations:
                anim = animations[0]
                length = anim.get("length", -1)
                loop_mode = anim.get("loop_mode", "")
                step = anim.get("step", -1)
                length_ok = abs(length - 2.5) < 0.01
                loop_ok = loop_mode == "linear"
                step_ok = abs(step - 0.05) < 0.001
                ok = length_ok and loop_ok and step_ok
                detail = (f"length: {length} (expected 2.5, ok={length_ok})\n"
                          f"         loop_mode: {loop_mode} (expected linear, ok={loop_ok})\n"
                          f"         step: {step} (expected 0.05, ok={step_ok})")
            else:
                detail = "No animations in response"
        else:
            detail = f"Response: {json.dumps(data)}"
        report(15, "Round-trip: set_animation_properties -> get confirms values (ANIM-05)", ok, detail)
    except Exception as e:
        report(15, "Round-trip: set_animation_properties -> get confirms values (ANIM-05)", False, f"Error: {e}")

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 8 Animation System UAT Results")
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

    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    run_tests()
