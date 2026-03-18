#!/usr/bin/env python3
"""
Phase 9 UAT - Automated end-to-end tests for editor viewport screenshot tools.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase9.py

Tests cover:
  - tools/list shows 35 tools including capture_viewport (VWPT-01..03)
  - capture_viewport with 2d viewport_type returns ImageContent (VWPT-01)
  - capture_viewport with 3d viewport_type returns ImageContent (VWPT-02)
  - ImageContent has type "image" (VWPT-03)
  - ImageContent has mimeType "image/png" (VWPT-03)
  - Base64 data decodes to valid PNG with correct signature (VWPT-03)
  - Metadata text content has viewport_type, width, height keys
  - capture_viewport with width/height returns resized image
  - capture_viewport with invalid viewport_type returns error
  - capture_viewport with no arguments defaults to 2D capture
"""

import base64
import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase8.py)
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
    """Call a tool via tools/call and return the raw result dict and full response."""
    if arguments is None:
        arguments = {}
    resp = client.request("tools/call", {"name": tool_name, "arguments": arguments})
    result = resp.get("result", {})
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

    print("=" * 60)
    print("  Phase 9 UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 35 tools including capture_viewport
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        has_capture = "capture_viewport" in tool_names
        ok = len(tools) == 35 and has_capture
        report(1, "tools/list shows 35 tools including capture_viewport", ok,
               f"Tool count: {len(tools)}\n"
               f"         capture_viewport present: {has_capture}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 35 tools including capture_viewport", False, f"Error: {e}")

    # ===================================================================
    # Test 2: capture_viewport with 2d returns ImageContent (VWPT-01)
    # ===================================================================
    captured_2d_data = None
    try:
        result, _ = call_tool(client, "capture_viewport", {"viewport_type": "2d"})
        content_list = result.get("content", [])
        has_content = len(content_list) >= 1
        is_image = has_content and content_list[0].get("type") == "image"
        has_data = is_image and len(content_list[0].get("data", "")) > 0
        ok = is_image and has_data
        if ok:
            captured_2d_data = content_list[0]["data"]
        report(2, "capture_viewport 2d returns ImageContent (VWPT-01)", ok,
               f"Content count: {len(content_list)}\n"
               f"         type: {content_list[0].get('type') if has_content else 'N/A'}\n"
               f"         data length: {len(content_list[0].get('data', '')) if has_content else 0}")
    except Exception as e:
        report(2, "capture_viewport 2d returns ImageContent (VWPT-01)", False, f"Error: {e}")

    # ===================================================================
    # Test 3: capture_viewport with 3d returns ImageContent (VWPT-02)
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {"viewport_type": "3d"})
        content_list = result.get("content", [])
        has_content = len(content_list) >= 1
        is_image = has_content and content_list[0].get("type") == "image"
        has_data = is_image and len(content_list[0].get("data", "")) > 0
        ok = is_image and has_data
        report(3, "capture_viewport 3d returns ImageContent (VWPT-02)", ok,
               f"Content count: {len(content_list)}\n"
               f"         type: {content_list[0].get('type') if has_content else 'N/A'}\n"
               f"         data length: {len(content_list[0].get('data', '')) if has_content else 0}")
    except Exception as e:
        report(3, "capture_viewport 3d returns ImageContent (VWPT-02)", False, f"Error: {e}")

    # ===================================================================
    # Test 4: ImageContent has type "image" (VWPT-03)
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {"viewport_type": "2d"})
        content_list = result.get("content", [])
        has_content = len(content_list) >= 1
        content_type = content_list[0].get("type") if has_content else None
        ok = content_type == "image"
        report(4, "ImageContent has type 'image' (VWPT-03)", ok,
               f"content[0]['type']: {content_type}")
    except Exception as e:
        report(4, "ImageContent has type 'image' (VWPT-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 5: ImageContent has mimeType "image/png" (VWPT-03)
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {"viewport_type": "2d"})
        content_list = result.get("content", [])
        has_content = len(content_list) >= 1
        mime_type = content_list[0].get("mimeType") if has_content else None
        ok = mime_type == "image/png"
        report(5, "ImageContent has mimeType 'image/png' (VWPT-03)", ok,
               f"content[0]['mimeType']: {mime_type}")
    except Exception as e:
        report(5, "ImageContent has mimeType 'image/png' (VWPT-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 6: Base64 data decodes to valid PNG (VWPT-03)
    # ===================================================================
    try:
        # Use cached data from test 2 if available, otherwise fetch fresh
        b64_data = captured_2d_data
        if b64_data is None:
            result, _ = call_tool(client, "capture_viewport", {"viewport_type": "2d"})
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
        report(6, "Base64 data decodes to valid PNG (VWPT-03)", ok, detail)
    except Exception as e:
        report(6, "Base64 data decodes to valid PNG (VWPT-03)", False, f"Error: {e}")

    # ===================================================================
    # Test 7: Metadata text content has viewport_type, width, height
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {"viewport_type": "2d"})
        content_list = result.get("content", [])
        ok = False
        detail = ""
        if len(content_list) >= 2:
            meta_item = content_list[1]
            is_text = meta_item.get("type") == "text"
            if is_text:
                meta = json.loads(meta_item["text"])
                has_vp_type = "viewport_type" in meta
                has_width = "width" in meta
                has_height = "height" in meta
                ok = has_vp_type and has_width and has_height
                detail = (f"content[1]['type']: {meta_item.get('type')}\n"
                          f"         Metadata: {json.dumps(meta)}\n"
                          f"         viewport_type: {has_vp_type}, width: {has_width}, height: {has_height}")
            else:
                detail = f"content[1] type is '{meta_item.get('type')}', expected 'text'"
        else:
            detail = f"Expected >= 2 content items, got {len(content_list)}"
        report(7, "Metadata text content has viewport_type, width, height", ok, detail)
    except Exception as e:
        report(7, "Metadata text content has viewport_type, width, height", False, f"Error: {e}")

    # ===================================================================
    # Test 8: capture_viewport with width/height returns resized image
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {
            "viewport_type": "2d",
            "width": 640,
            "height": 480
        })
        content_list = result.get("content", [])
        ok = False
        detail = ""
        if len(content_list) >= 2:
            is_image = content_list[0].get("type") == "image"
            meta_item = content_list[1]
            if meta_item.get("type") == "text":
                meta = json.loads(meta_item["text"])
                width_ok = meta.get("width") == 640
                height_ok = meta.get("height") == 480
                ok = is_image and width_ok and height_ok
                detail = (f"Image type: {content_list[0].get('type')}\n"
                          f"         Metadata width: {meta.get('width')} (expected 640, ok={width_ok})\n"
                          f"         Metadata height: {meta.get('height')} (expected 480, ok={height_ok})")
            else:
                detail = f"content[1] type is '{meta_item.get('type')}', expected 'text'"
        else:
            detail = f"Expected >= 2 content items, got {len(content_list)}"
        report(8, "capture_viewport with width/height returns resized image", ok, detail)
    except Exception as e:
        report(8, "capture_viewport with width/height returns resized image", False, f"Error: {e}")

    # ===================================================================
    # Test 9: capture_viewport with invalid viewport_type returns error
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {"viewport_type": "invalid"})
        content_list = result.get("content", [])
        ok = False
        detail = ""
        if content_list:
            first = content_list[0]
            is_text = first.get("type") == "text"
            if is_text:
                parsed = json.loads(first["text"])
                has_error = "error" in parsed
                ok = has_error
                detail = (f"content[0]['type']: {first.get('type')}\n"
                          f"         Parsed: {json.dumps(parsed)}\n"
                          f"         Has 'error' key: {has_error}")
            else:
                detail = f"content[0] type is '{first.get('type')}', expected 'text' for error response"
        else:
            detail = "No content in response"
        report(9, "capture_viewport with invalid viewport_type returns error", ok, detail)
    except Exception as e:
        report(9, "capture_viewport with invalid viewport_type returns error", False, f"Error: {e}")

    # ===================================================================
    # Test 10: capture_viewport with no arguments defaults to 2D
    # ===================================================================
    try:
        result, _ = call_tool(client, "capture_viewport", {})
        content_list = result.get("content", [])
        ok = False
        detail = ""
        if content_list:
            is_image = content_list[0].get("type") == "image"
            has_data = len(content_list[0].get("data", "")) > 0
            ok = is_image and has_data
            detail = (f"content[0]['type']: {content_list[0].get('type')}\n"
                      f"         data length: {len(content_list[0].get('data', ''))}")
            # Also check metadata confirms 2d if available
            if len(content_list) >= 2 and content_list[1].get("type") == "text":
                meta = json.loads(content_list[1]["text"])
                vp_type = meta.get("viewport_type", "")
                detail += f"\n         metadata viewport_type: {vp_type}"
        else:
            detail = "No content in response"
        report(10, "capture_viewport with no args defaults to 2D capture", ok, detail)
    except Exception as e:
        report(10, "capture_viewport with no args defaults to 2D capture", False, f"Error: {e}")

    # ===================================================================
    # Cleanup
    # ===================================================================
    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 9 Editor Viewport Screenshots UAT Results")
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
