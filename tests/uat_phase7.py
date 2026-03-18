#!/usr/bin/env python3
"""
Phase 7 UAT - Automated end-to-end tests for UI system tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase7.py

Tests cover:
  - tools/list shows 29 tools including all Phase 7 UI tools
  - set_layout_preset applies full_rect preset (UISYS-01)
  - set_layout_preset applies center preset (UISYS-01)
  - set_layout_preset with invalid preset returns error
  - set_layout_preset on non-Control returns error
  - get_ui_properties returns anchors, offsets, size_flags, etc. (UISYS-04)
  - Round-trip: set_layout_preset then get_ui_properties confirms anchor values
  - set_theme_override batch-sets color and font_size (UISYS-02)
  - get_theme_overrides returns set overrides (UISYS-06)
  - Round-trip: set_theme_override then get_theme_overrides confirms values
  - create_stylebox creates and applies StyleBoxFlat (UISYS-03)
  - set_container_layout sets separation on VBoxContainer (UISYS-05)
  - set_container_layout with alignment on BoxContainer
  - set_container_layout on non-Container returns error
  - UISYS-06 focus neighbors: get_ui_properties returns focus_neighbors
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase6.py)
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

    # --- Setup: Create test scene with UI nodes ---
    print("  Setting up test scene...")
    setup_data, _ = call_tool(client, "create_scene", {
        "root_type": "Control",
        "path": "res://test_ui_phase7.tscn",
        "root_name": "UIRoot"
    })
    if not setup_data.get("success"):
        print(f"  FATAL: Could not create test scene: {json.dumps(setup_data)}")
        client.close()
        sys.exit(1)
    time.sleep(0.5)

    # Create child nodes for testing
    setup_nodes = [
        {"type": "Button", "parent_path": "", "name": "TestButton"},
        {"type": "Panel", "parent_path": "", "name": "TestPanel"},
        {"type": "VBoxContainer", "parent_path": "", "name": "TestVBox"},
        {"type": "Button", "parent_path": "TestVBox", "name": "Child1"},
        {"type": "Button", "parent_path": "TestVBox", "name": "Child2"},
        {"type": "Label", "parent_path": "", "name": "TestLabel"},
        {"type": "Node2D", "parent_path": "", "name": "NonControl"},
    ]
    for node_def in setup_nodes:
        data, _ = call_tool(client, "create_node", node_def)
        if not data.get("success"):
            print(f"  WARNING: Could not create {node_def['name']}: {json.dumps(data)}")
        time.sleep(0.2)

    print("  Setup complete.\n")

    print("=" * 60)
    print("  Phase 7 UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 29 tools including all Phase 7 UI tools
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        expected_phase7 = ["set_layout_preset", "set_theme_override",
                           "create_stylebox", "get_ui_properties",
                           "set_container_layout", "get_theme_overrides"]
        has_all = all(n in tool_names for n in expected_phase7)
        ok = len(tools) == 29 and has_all
        report(1, "tools/list shows 29 tools with Phase 7 tools", ok,
               f"Tool count: {len(tools)}\n"
               f"         Phase 7 tools present: {has_all}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 29 tools with Phase 7 tools", False, f"Error: {e}")

    # ===================================================================
    # Test 2: set_layout_preset full_rect (UISYS-01)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_layout_preset", {
            "node_path": "TestButton",
            "preset": "full_rect"
        })
        ok = data.get("success") == True
        report(2, "set_layout_preset full_rect (UISYS-01)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(2, "set_layout_preset full_rect (UISYS-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 3: set_layout_preset center (UISYS-01)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_layout_preset", {
            "node_path": "TestPanel",
            "preset": "center"
        })
        ok = data.get("success") == True
        report(3, "set_layout_preset center (UISYS-01)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(3, "set_layout_preset center (UISYS-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 4: set_layout_preset with invalid preset returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_layout_preset", {
            "node_path": "TestButton",
            "preset": "invalid_preset"
        })
        has_error = "error" in data
        ok = has_error
        report(4, "set_layout_preset invalid preset returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(4, "set_layout_preset invalid preset returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 5: set_layout_preset on non-Control returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_layout_preset", {
            "node_path": "NonControl",
            "preset": "full_rect"
        })
        has_error = "error" in data
        error_msg = data.get("error", "")
        ok = has_error and "not a control" in error_msg.lower()
        report(5, "set_layout_preset on non-Control returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(5, "set_layout_preset on non-Control returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 6: get_ui_properties returns comprehensive data (UISYS-04)
    # ===================================================================
    try:
        data, _ = call_tool(client, "get_ui_properties", {
            "node_path": "TestButton"
        })
        ok = False
        detail = ""
        if data.get("success") == True:
            has_anchors = ("anchors" in data and
                           all(k in data["anchors"] for k in ("left", "top", "right", "bottom")))
            has_offsets = "offsets" in data
            has_size_flags = ("size_flags" in data and
                              "horizontal" in data.get("size_flags", {}) and
                              "vertical" in data.get("size_flags", {}))
            has_min_size = "minimum_size" in data
            has_focus = ("focus_neighbors" in data and
                         all(k in data["focus_neighbors"] for k in ("left", "top", "right", "bottom")))
            has_layout_dir = "layout_direction" in data
            ok = (has_anchors and has_offsets and has_size_flags and
                  has_min_size and has_focus and has_layout_dir)
            detail = (f"anchors: {has_anchors}, offsets: {has_offsets}\n"
                      f"         size_flags: {has_size_flags}, min_size: {has_min_size}\n"
                      f"         focus_neighbors: {has_focus}, layout_dir: {has_layout_dir}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(6, "get_ui_properties returns comprehensive data (UISYS-04)", ok, detail)
    except Exception as e:
        report(6, "get_ui_properties returns comprehensive data (UISYS-04)", False, f"Error: {e}")

    # ===================================================================
    # Test 7: Round-trip: set_layout_preset full_rect then get_ui_properties
    #         confirms anchor values (UISYS-01 + UISYS-04)
    # ===================================================================
    try:
        # Set full_rect on TestLabel
        set_data, _ = call_tool(client, "set_layout_preset", {
            "node_path": "TestLabel",
            "preset": "full_rect"
        })
        time.sleep(0.3)

        # Query anchors
        data, _ = call_tool(client, "get_ui_properties", {
            "node_path": "TestLabel"
        })
        ok = False
        detail = ""
        if data.get("success") == True and "anchors" in data:
            anchors = data["anchors"]
            # full_rect: left=0.0, top=0.0, right=1.0, bottom=1.0
            ok = (abs(anchors.get("left", -1) - 0.0) < 0.01 and
                  abs(anchors.get("top", -1) - 0.0) < 0.01 and
                  abs(anchors.get("right", -1) - 1.0) < 0.01 and
                  abs(anchors.get("bottom", -1) - 1.0) < 0.01)
            detail = (f"Anchors: L={anchors.get('left')}, T={anchors.get('top')}, "
                      f"R={anchors.get('right')}, B={anchors.get('bottom')}\n"
                      f"         Expected: L=0.0, T=0.0, R=1.0, B=1.0")
        else:
            detail = f"set_data: {json.dumps(set_data)}\n         get_data: {json.dumps(data)}"
        report(7, "Round-trip: full_rect preset -> get_ui_properties (UISYS-01+04)", ok, detail)
    except Exception as e:
        report(7, "Round-trip: full_rect preset -> get_ui_properties (UISYS-01+04)", False, f"Error: {e}")

    # ===================================================================
    # Test 8: set_theme_override batch-sets color and font_size (UISYS-02)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_theme_override", {
            "node_path": "TestButton",
            "overrides": {"font_color": "#ff0000", "font_size": 24}
        })
        ok = data.get("success") == True
        report(8, "set_theme_override batch color+font_size (UISYS-02)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(8, "set_theme_override batch color+font_size (UISYS-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 9: get_theme_overrides returns set overrides (UISYS-06)
    # ===================================================================
    try:
        data, _ = call_tool(client, "get_theme_overrides", {
            "node_path": "TestButton"
        })
        ok = False
        detail = ""
        if data.get("success") == True:
            has_colors = "colors" in data
            has_font_sizes = "font_sizes" in data
            ok = has_colors and has_font_sizes
            detail = (f"colors present: {has_colors}, font_sizes present: {has_font_sizes}\n"
                      f"         colors: {json.dumps(data.get('colors', {}))}\n"
                      f"         font_sizes: {json.dumps(data.get('font_sizes', {}))}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(9, "get_theme_overrides returns overrides (UISYS-06)", ok, detail)
    except Exception as e:
        report(9, "get_theme_overrides returns overrides (UISYS-06)", False, f"Error: {e}")

    # ===================================================================
    # Test 10: Round-trip: set_theme_override then get_theme_overrides
    #          confirms values (UISYS-02 + UISYS-06)
    # ===================================================================
    try:
        # Set overrides on TestLabel
        set_data, _ = call_tool(client, "set_theme_override", {
            "node_path": "TestLabel",
            "overrides": {"font_color": "#00ff00", "font_size": 32}
        })
        time.sleep(0.3)

        # Query overrides
        data, _ = call_tool(client, "get_theme_overrides", {
            "node_path": "TestLabel"
        })
        ok = False
        detail = ""
        if data.get("success") == True:
            colors = data.get("colors", {})
            font_sizes = data.get("font_sizes", {})
            has_font_color = "font_color" in colors
            has_font_size = "font_size" in font_sizes
            font_size_val = font_sizes.get("font_size")
            ok = has_font_color and has_font_size and font_size_val == 32
            detail = (f"font_color present: {has_font_color}\n"
                      f"         font_size present: {has_font_size}, value: {font_size_val} (expected 32)")
        else:
            detail = f"set_data: {json.dumps(set_data)}\n         get_data: {json.dumps(data)}"
        report(10, "Round-trip: set_theme_override -> get confirms (UISYS-02+06)", ok, detail)
    except Exception as e:
        report(10, "Round-trip: set_theme_override -> get confirms (UISYS-02+06)", False, f"Error: {e}")

    # ===================================================================
    # Test 11: create_stylebox creates and applies StyleBoxFlat (UISYS-03)
    # ===================================================================
    try:
        data, _ = call_tool(client, "create_stylebox", {
            "node_path": "TestPanel",
            "override_name": "panel",
            "bg_color": "#3366cc",
            "corner_radius": 8,
            "border_width": 2,
            "border_color": "#ffffff"
        })
        ok = data.get("success") == True
        detail = f"Response: {json.dumps(data)}"

        # Verify via get_theme_overrides
        if ok:
            time.sleep(0.3)
            verify_data, _ = call_tool(client, "get_theme_overrides", {
                "node_path": "TestPanel"
            })
            styles = verify_data.get("styles", {})
            has_panel = "panel" in styles
            ok = ok and has_panel
            detail += f"\n         Styles after: {json.dumps(styles)}, panel present: {has_panel}"

        report(11, "create_stylebox creates+applies StyleBoxFlat (UISYS-03)", ok, detail)
    except Exception as e:
        report(11, "create_stylebox creates+applies StyleBoxFlat (UISYS-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 12: set_container_layout separation on VBoxContainer (UISYS-05)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_container_layout", {
            "node_path": "TestVBox",
            "separation": 20
        })
        ok = data.get("success") == True
        report(12, "set_container_layout separation on VBox (UISYS-05)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(12, "set_container_layout separation on VBox (UISYS-05)", False, f"Error: {e}")

    # ===================================================================
    # Test 13: set_container_layout with alignment (UISYS-05)
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_container_layout", {
            "node_path": "TestVBox",
            "alignment": "center"
        })
        ok = data.get("success") == True
        report(13, "set_container_layout alignment center (UISYS-05)", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(13, "set_container_layout alignment center (UISYS-05)", False, f"Error: {e}")

    # ===================================================================
    # Test 14: set_container_layout on non-Container returns error
    # ===================================================================
    try:
        data, _ = call_tool(client, "set_container_layout", {
            "node_path": "TestButton"
        })
        has_error = "error" in data
        error_msg = data.get("error", "")
        ok = has_error and "not a container" in error_msg.lower()
        report(14, "set_container_layout on non-Container returns error", ok,
               f"Response: {json.dumps(data)}")
    except Exception as e:
        report(14, "set_container_layout on non-Container returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 15: UISYS-06 focus neighbors in get_ui_properties
    # ===================================================================
    try:
        # Set focus neighbor via existing set_node_property
        set_data, _ = call_tool(client, "set_node_property", {
            "node_path": "TestButton",
            "property": "focus_neighbor_left",
            "value": "TestPanel"
        })
        time.sleep(0.3)

        # Query via get_ui_properties
        data, _ = call_tool(client, "get_ui_properties", {
            "node_path": "TestButton"
        })
        ok = False
        detail = ""
        if data.get("success") == True and "focus_neighbors" in data:
            focus = data["focus_neighbors"]
            left_val = focus.get("left", "")
            # The value should be non-empty (some path reference to TestPanel)
            ok = left_val != "" and len(left_val) > 0
            detail = (f"focus_neighbors.left: '{left_val}'\n"
                      f"         Expected: non-empty path reference to TestPanel\n"
                      f"         set_node_property result: {json.dumps(set_data)}")
        else:
            detail = f"Response: {json.dumps(data)}"
        report(15, "UISYS-06 focus neighbors in get_ui_properties", ok, detail)
    except Exception as e:
        report(15, "UISYS-06 focus neighbors in get_ui_properties", False, f"Error: {e}")

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 7 UAT Test Results")
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
