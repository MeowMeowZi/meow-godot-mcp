#!/usr/bin/env python3
"""
v1.4 UAT - 2D Game Development Core Tools

Tests:
  Phase 19: Resource property support (res:// path loading, new: inline creation)
  Phase 20: TileMap operations (set/erase/query cells, get info)
  Phase 21: Collision shape quick-create

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_v14_2d_tools.py
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as other UAT scripts)
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

    def reconnect(self):
        self.close()
        time.sleep(0.5)
        self.connect()
        self.request("initialize")
        self.notify("notifications/initialized")
        time.sleep(0.3)

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

    def call_tool(self, name, arguments=None):
        params = {"name": name}
        if arguments:
            params["arguments"] = arguments
        return self.request("tools/call", params)

    def safe_call_tool(self, name, arguments=None):
        try:
            return self.call_tool(name, arguments)
        except (ConnectionError, OSError):
            print("    (reconnecting...)")
            self.reconnect()
            return self.call_tool(name, arguments)

    def get_tool_content(self, resp):
        if "error" in resp:
            return {"_error": resp["error"]["message"]}
        text = resp["result"]["content"][0]["text"]
        return json.loads(text)


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
# Phase 19 Tests: Resource Property Support
# ---------------------------------------------------------------------------

def test_phase19(client):
    print("\n=== Phase 19: Resource Property Support ===\n")

    # Setup: create a test scene
    resp = client.safe_call_tool("create_scene", {
        "root_type": "Node2D",
        "root_name": "TestScene",
        "path": "res://test_v14.tscn"
    })
    content = client.get_tool_content(resp)
    report("19-01", "Create test scene", content.get("success", False),
           json.dumps(content, indent=2) if not content.get("success") else "")

    # Test 1: Create Sprite2D and set texture via res:// path
    resp = client.safe_call_tool("create_node", {
        "type": "Sprite2D",
        "name": "TestSprite"
    })
    content = client.get_tool_content(resp)
    report("19-02", "Create Sprite2D node", content.get("success", False))

    resp = client.safe_call_tool("set_node_property", {
        "node_path": "TestSprite",
        "property": "texture",
        "value": "res://icon.svg"
    })
    content = client.get_tool_content(resp)
    report("19-03", "Set Sprite2D.texture via res://icon.svg",
           content.get("success", False),
           json.dumps(content, indent=2) if not content.get("success") else "")

    # Test 2: Create CollisionShape2D and set shape via new: syntax
    resp = client.safe_call_tool("create_node", {
        "type": "CollisionShape2D",
        "name": "TestCollision"
    })
    content = client.get_tool_content(resp)
    report("19-04", "Create CollisionShape2D node", content.get("success", False))

    resp = client.safe_call_tool("set_node_property", {
        "node_path": "TestCollision",
        "property": "shape",
        "value": "new:RectangleShape2D(size=Vector2(100, 50))"
    })
    content = client.get_tool_content(resp)
    report("19-05", "Set shape via new:RectangleShape2D(size=Vector2(100,50))",
           content.get("success", False),
           json.dumps(content, indent=2) if not content.get("success") else "")

    # Test 3: new: with CircleShape2D
    resp = client.safe_call_tool("set_node_property", {
        "node_path": "TestCollision",
        "property": "shape",
        "value": "new:CircleShape2D(radius=25)"
    })
    content = client.get_tool_content(resp)
    report("19-06", "Set shape via new:CircleShape2D(radius=25)",
           content.get("success", False),
           json.dumps(content, indent=2) if not content.get("success") else "")

    # Test 4: Invalid res:// path should not crash (returns success but property stays nil)
    resp = client.safe_call_tool("set_node_property", {
        "node_path": "TestSprite",
        "property": "texture",
        "value": "res://nonexistent_file_xyz.png"
    })
    content = client.get_tool_content(resp)
    # This should "succeed" but the property value will be nil since the file doesn't exist
    report("19-07", "Invalid res:// path doesn't crash",
           content.get("success", False),
           "Returns success but texture set to nil (expected behavior)")

    # Test 5: Create node with resource property in initial properties
    resp = client.safe_call_tool("create_node", {
        "type": "Sprite2D",
        "name": "SpriteWithTexture",
        "properties": {
            "texture": "res://icon.svg",
            "position": "Vector2(200, 100)"
        }
    })
    content = client.get_tool_content(resp)
    report("19-08", "create_node with texture in initial properties",
           content.get("success", False),
           json.dumps(content, indent=2) if not content.get("success") else "")


# ---------------------------------------------------------------------------
# Phase 20 Tests: TileMap Operations
# ---------------------------------------------------------------------------

def test_phase20(client):
    print("\n=== Phase 20: TileMap Operations ===\n")

    # Create a TileMapLayer node
    resp = client.safe_call_tool("create_node", {
        "type": "TileMapLayer",
        "name": "TestTileMap"
    })
    content = client.get_tool_content(resp)
    report("20-01", "Create TileMapLayer node", content.get("success", False))

    # Test get_tilemap_info on empty tilemap (no tileset)
    resp = client.safe_call_tool("get_tilemap_info", {
        "node_path": "TestTileMap"
    })
    content = client.get_tool_content(resp)
    ok = content.get("success", False)
    has_ts = content.get("has_tile_set", True)
    report("20-02", "get_tilemap_info (no tileset)",
           ok and not has_ts,
           f"has_tile_set={has_ts}, used_cells_count={content.get('used_cells_count', '?')}")

    # Test get_tilemap_cell_info on empty cell
    resp = client.safe_call_tool("get_tilemap_cell_info", {
        "node_path": "TestTileMap",
        "coords": [{"x": 0, "y": 0}]
    })
    content = client.get_tool_content(resp)
    cells = content.get("cells", [])
    ok = content.get("success", False) and len(cells) > 0 and cells[0].get("empty", False)
    report("20-03", "get_tilemap_cell_info (empty cell)",
           ok,
           f"cell data: {json.dumps(cells[0]) if cells else 'no data'}")

    # Test set_tilemap_cells without tileset (should succeed but cells may not render)
    resp = client.safe_call_tool("set_tilemap_cells", {
        "node_path": "TestTileMap",
        "cells": [
            {"x": 0, "y": 0, "source_id": 0, "atlas_x": 0, "atlas_y": 0},
            {"x": 1, "y": 0, "source_id": 0, "atlas_x": 0, "atlas_y": 0},
            {"x": 2, "y": 0, "source_id": 0, "atlas_x": 0, "atlas_y": 0}
        ]
    })
    content = client.get_tool_content(resp)
    report("20-04", "set_tilemap_cells (3 cells)",
           content.get("success", False) and content.get("cells_set", 0) == 3,
           f"cells_set={content.get('cells_set', '?')}")

    # Verify cells were placed
    resp = client.safe_call_tool("get_tilemap_cell_info", {
        "node_path": "TestTileMap",
        "coords": [{"x": 0, "y": 0}, {"x": 1, "y": 0}, {"x": 5, "y": 5}]
    })
    content = client.get_tool_content(resp)
    cells = content.get("cells", [])
    cell_0_ok = len(cells) >= 1 and not cells[0].get("empty", True)
    cell_1_ok = len(cells) >= 2 and not cells[1].get("empty", True)
    cell_5_empty = len(cells) >= 3 and cells[2].get("empty", False)
    report("20-05", "Verify placed cells + empty cell",
           cell_0_ok and cell_1_ok and cell_5_empty,
           f"cell(0,0).empty={cells[0].get('empty') if cells else '?'}, "
           f"cell(1,0).empty={cells[1].get('empty') if len(cells)>1 else '?'}, "
           f"cell(5,5).empty={cells[2].get('empty') if len(cells)>2 else '?'}")

    # Test erase_tilemap_cells
    resp = client.safe_call_tool("erase_tilemap_cells", {
        "node_path": "TestTileMap",
        "coords": [{"x": 0, "y": 0}, {"x": 1, "y": 0}]
    })
    content = client.get_tool_content(resp)
    report("20-06", "erase_tilemap_cells (2 cells)",
           content.get("success", False) and content.get("cells_erased", 0) == 2,
           f"cells_erased={content.get('cells_erased', '?')}")

    # Verify erasure
    resp = client.safe_call_tool("get_tilemap_cell_info", {
        "node_path": "TestTileMap",
        "coords": [{"x": 0, "y": 0}, {"x": 2, "y": 0}]
    })
    content = client.get_tool_content(resp)
    cells = content.get("cells", [])
    erased = len(cells) >= 1 and cells[0].get("empty", False)
    kept = len(cells) >= 2 and not cells[1].get("empty", True)
    report("20-07", "Verify erase: (0,0) empty, (2,0) kept",
           erased and kept,
           f"cell(0,0).empty={cells[0].get('empty') if cells else '?'}, "
           f"cell(2,0).empty={cells[1].get('empty') if len(cells)>1 else '?'}")

    # Test error on non-TileMapLayer node
    resp = client.safe_call_tool("get_tilemap_info", {
        "node_path": "TestSprite"
    })
    content = client.get_tool_content(resp)
    report("20-08", "get_tilemap_info on non-TileMapLayer returns error",
           "error" in content,
           f"error={content.get('error', 'none')}")


# ---------------------------------------------------------------------------
# Phase 21 Tests: Collision Shape Quick-Create
# ---------------------------------------------------------------------------

def test_phase21(client):
    print("\n=== Phase 21: Collision Shape Quick-Create ===\n")

    # Test 1: Rectangle shape
    resp = client.safe_call_tool("create_collision_shape", {
        "parent_path": "",
        "shape_type": "rectangle",
        "shape_params": {"width": 100, "height": 50},
        "name": "RectShape"
    })
    content = client.get_tool_content(resp)
    report("21-01", "create_collision_shape: rectangle",
           content.get("success", False) and content.get("shape_class") == "RectangleShape2D",
           f"path={content.get('path')}, shape_class={content.get('shape_class')}")

    # Test 2: Circle shape
    resp = client.safe_call_tool("create_collision_shape", {
        "parent_path": "",
        "shape_type": "circle",
        "shape_params": {"radius": 30},
        "name": "CircleShape"
    })
    content = client.get_tool_content(resp)
    report("21-02", "create_collision_shape: circle",
           content.get("success", False) and content.get("shape_class") == "CircleShape2D",
           f"path={content.get('path')}, shape_class={content.get('shape_class')}")

    # Test 3: Capsule shape
    resp = client.safe_call_tool("create_collision_shape", {
        "parent_path": "",
        "shape_type": "capsule",
        "shape_params": {"radius": 15, "height": 40},
        "name": "CapsuleShape"
    })
    content = client.get_tool_content(resp)
    report("21-03", "create_collision_shape: capsule",
           content.get("success", False) and content.get("shape_class") == "CapsuleShape2D",
           f"path={content.get('path')}, shape_class={content.get('shape_class')}")

    # Test 4: 3D box shape
    resp = client.safe_call_tool("create_collision_shape", {
        "parent_path": "",
        "shape_type": "box",
        "shape_params": {"width": 2, "height": 3, "depth": 1},
        "name": "BoxShape"
    })
    content = client.get_tool_content(resp)
    report("21-04", "create_collision_shape: box (3D)",
           content.get("success", False) and content.get("shape_class") == "BoxShape3D",
           f"path={content.get('path')}, collision_node_type={content.get('collision_node_type')}")

    # Test 5: Default params (no shape_params)
    resp = client.safe_call_tool("create_collision_shape", {
        "parent_path": "",
        "shape_type": "circle"
    })
    content = client.get_tool_content(resp)
    report("21-05", "create_collision_shape: circle with defaults",
           content.get("success", False),
           f"path={content.get('path')}")

    # Test 6: Invalid shape_type
    resp = client.safe_call_tool("create_collision_shape", {
        "parent_path": "",
        "shape_type": "invalid_shape_xyz"
    })
    content = client.get_tool_content(resp)
    report("21-06", "create_collision_shape: invalid type returns error",
           "error" in content,
           f"error={content.get('error', 'none')[:80]}")

    # Test 7: Verify shape is visible in scene tree
    resp = client.safe_call_tool("get_scene_tree", {})
    content = client.get_tool_content(resp)
    tree_str = json.dumps(content)
    has_rect = "RectShape" in tree_str
    has_circle = "CircleShape" in tree_str
    has_capsule = "CapsuleShape" in tree_str
    report("21-07", "All collision shapes visible in scene tree",
           has_rect and has_circle and has_capsule,
           f"RectShape={has_rect}, CircleShape={has_circle}, CapsuleShape={has_capsule}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    client = MCPClient()

    print("Connecting to Godot MCP server on port 6800...")
    try:
        client.connect()
    except ConnectionRefusedError:
        print("ERROR: Cannot connect. Is Godot running with MCP plugin enabled?")
        sys.exit(1)

    # Handshake
    resp = client.request("initialize")
    client.notify("notifications/initialized")
    time.sleep(0.3)

    print("Connected! Running v1.4 2D Tools UAT...\n")

    try:
        test_phase19(client)
        test_phase20(client)
        test_phase21(client)
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
    finally:
        client.close()

    # Summary
    print("\n" + "=" * 60)
    total = len(results)
    passed = sum(1 for _, _, p in results if p)
    failed = total - passed
    print(f"Results: {passed}/{total} passed, {failed} failed")

    if failed > 0:
        print("\nFailed tests:")
        for num, name, p in results:
            if not p:
                print(f"  - Test {num}: {name}")

    print("=" * 60)
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
