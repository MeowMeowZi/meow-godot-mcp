#!/usr/bin/env python3
"""
Phase 3 UAT - Automated end-to-end tests for script tools, project tools, and MCP resources.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Run: python tests/uat_phase3.py

Tests that require manual verification (undo/redo, visual UI) are marked with [MANUAL].
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client
# ---------------------------------------------------------------------------

class MCPClient:
    def __init__(self, host="127.0.0.1", port=6800, timeout=10):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None
        self.buffer = ""
        self._id = 0
        self._initialized = False

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(self.timeout)
        self.sock.connect((self.host, self.port))
        self.buffer = ""

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None
        self._initialized = False

    def reconnect(self):
        """Reconnect and re-handshake."""
        self.close()
        time.sleep(0.5)
        self.connect()
        resp = self.request("initialize")
        self.notify("notifications/initialized")
        time.sleep(0.3)
        self._initialized = True
        return resp

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
        """Call tool with auto-reconnect on connection error."""
        try:
            return self.call_tool(name, arguments)
        except (ConnectionError, OSError):
            print("    (连接断开，正在重连...)")
            self.reconnect()
            return self.call_tool(name, arguments)

    def safe_request(self, method, params=None):
        """Request with auto-reconnect on connection error."""
        try:
            return self.request(method, params)
        except (ConnectionError, OSError):
            print("    (连接断开，正在重连...)")
            self.reconnect()
            return self.request(method, params)

    def get_tool_content(self, resp):
        """Extract the parsed JSON content from a tool result response."""
        if "error" in resp:
            return {"_error": resp["error"]["message"]}
        text = resp["result"]["content"][0]["text"]
        return json.loads(text)


# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"
MANUAL = "\033[93mMANUAL\033[0m"

results = []

def report(num, name, passed, detail="", manual=False):
    tag = MANUAL if manual else (PASS if passed else FAIL)
    results.append((num, name, passed, manual))
    print(f"  [{tag}] Test {num}: {name}")
    if detail:
        for line in detail.strip().split("\n"):
            print(f"         {line}")


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def run_tests():
    client = MCPClient()

    print("\n连接到 Godot MCP 服务器 (127.0.0.1:6800)...")
    try:
        client.connect()
    except Exception as e:
        print(f"\n  连接失败: {e}")
        print("  请确保 Godot 编辑器已打开项目并启用了 MCP Meow 插件。")
        sys.exit(1)

    # --- Handshake ---
    print("  执行 MCP 握手...")
    resp = client.request("initialize")
    assert resp["result"]["serverInfo"]["name"] == "meow-godot-mcp"
    client.notify("notifications/initialized")
    time.sleep(0.3)
    print("  握手成功: meow-godot-mcp v" + resp["result"]["serverInfo"]["version"])
    print()

    # ===================================================================
    # GROUP A: Read-only tests (no scene mutation, no file writes)
    # ===================================================================
    print("━" * 60)
    print("  --- A 组: 只读测试 ---")

    # Test 8: list_project_files
    try:
        resp = client.safe_call_tool("list_project_files")
        content = client.get_tool_content(resp)
        is_list = isinstance(content, list) or (isinstance(content, dict) and "files" in content)
        files = content if isinstance(content, list) else content.get("files", [])
        has_items = len(files) > 0
        has_fields = has_items and ("path" in files[0] or "type" in files[0])
        ok = is_list and has_items and has_fields
        report(8, "List Project Files", ok,
               f"文件数量: {len(files)}, 首项: {json.dumps(files[0], ensure_ascii=False)[:100]}" if has_items
               else f"响应: {str(content)[:200]}")
    except Exception as e:
        report(8, "List Project Files", False, f"异常: {e}")

    # Test 9: get_project_settings
    try:
        resp = client.safe_call_tool("get_project_settings")
        content = client.get_tool_content(resp)
        is_dict = isinstance(content, dict)
        content_str = json.dumps(content, ensure_ascii=False)
        has_app_name = "application/config/name" in content_str or "config/name" in content_str
        ok = is_dict and has_app_name
        report(9, "Get Project Settings", ok,
               f"包含项目名称配置: {'是' if has_app_name else '否'}, 键数: {len(content) if is_dict else '?'}")
    except Exception as e:
        report(9, "Get Project Settings", False, f"异常: {e}")

    # Test 10: get_resource_info
    try:
        resp = client.safe_call_tool("get_resource_info", {"path": "res://test_root.tscn"})
        content = client.get_tool_content(resp)
        has_type = "type" in content or "resource_type" in content
        ok = isinstance(content, dict) and has_type
        report(10, "Get Resource Info", ok,
               f"资源类型: {content.get('type', content.get('resource_type', '?'))}")
    except Exception as e:
        report(10, "Get Resource Info", False, f"异常: {e}")

    # Test 11: resources/list
    try:
        resp = client.safe_request("resources/list")
        resources = resp.get("result", {}).get("resources", [])
        uris = [r.get("uri") for r in resources]
        has_scene_tree = "godot://scene_tree" in uris
        has_project_files = "godot://project_files" in uris
        ok = has_scene_tree and has_project_files and len(resources) == 2
        report(11, "MCP Resources List", ok,
               f"资源数: {len(resources)}, URIs: {uris}")
    except Exception as e:
        report(11, "MCP Resources List", False, f"异常: {e}")

    # Test 12: resources/read
    try:
        resp1 = client.safe_request("resources/read", {"uri": "godot://scene_tree"})
        resp2 = client.safe_request("resources/read", {"uri": "godot://project_files"})
        c1 = resp1.get("result", {}).get("contents", [])
        c2 = resp2.get("result", {}).get("contents", [])
        ok1 = len(c1) > 0 and "text" in c1[0]
        ok2 = len(c2) > 0 and "text" in c2[0]
        ok = ok1 and ok2
        detail_parts = []
        if ok1:
            tree_data = json.loads(c1[0]["text"])
            detail_parts.append(f"scene_tree: 包含 {len(json.dumps(tree_data))} 字节")
        if ok2:
            files_data = json.loads(c2[0]["text"])
            n = len(files_data) if isinstance(files_data, list) else "?"
            detail_parts.append(f"project_files: {n} 个文件")
        report(12, "MCP Resources Read", ok, ", ".join(detail_parts) if detail_parts else "响应异常")
    except Exception as e:
        report(12, "MCP Resources Read", False, f"异常: {e}")

    # Test 13: IO thread (rapid calls)
    try:
        t0 = time.time()
        for _ in range(5):
            client.safe_request("tools/list")
        elapsed = time.time() - t0
        ok = elapsed < 5.0
        report(13, "IO Thread (快速连续请求)", ok,
               f"5次 tools/list 耗时 {elapsed:.2f}s\n"
               f"  [MANUAL] 请在 Godot 输出日志中确认 'IO thread started' 消息")
    except Exception as e:
        report(13, "IO Thread (快速连续请求)", False, f"异常: {e}")

    # ===================================================================
    # GROUP B: Scene mutation tests (Phase 2 regression - no file writes)
    # ===================================================================
    print("\n  --- B 组: 场景操作回归测试 ---")

    try:
        r1 = client.safe_call_tool("get_scene_tree")
        c1 = client.get_tool_content(r1)
        tree_ok = "name" in c1 or "children" in c1 or isinstance(c1, dict)

        r2 = client.safe_call_tool("create_node", {"type": "Node2D", "name": "UATRegTest"})
        c2 = client.get_tool_content(r2)
        create_ok = c2.get("success", False) or "success" in str(c2).lower()

        r3 = client.safe_call_tool("set_node_property", {
            "node_path": "UATRegTest", "property": "visible", "value": "false"
        })
        c3 = client.get_tool_content(r3)
        prop_ok = c3.get("success", False) or "success" in str(c3).lower()

        r4 = client.safe_call_tool("delete_node", {"node_path": "UATRegTest"})
        c4 = client.get_tool_content(r4)
        delete_ok = c4.get("success", False) or "success" in str(c4).lower()

        ok = tree_ok and create_ok and prop_ok and delete_ok
        report(14, "Phase 2 Regression", ok,
               f"get_scene_tree={'OK' if tree_ok else 'FAIL'}, "
               f"create_node={'OK' if create_ok else 'FAIL'}, "
               f"set_node_property={'OK' if prop_ok else 'FAIL'}, "
               f"delete_node={'OK' if delete_ok else 'FAIL'}")
    except Exception as e:
        report(14, "Phase 2 Regression", False, f"异常: {e}")

    # ===================================================================
    # GROUP C: Script file write tests
    # ===================================================================
    print("\n  --- C 组: 脚本文件写入测试 ---")
    ts = int(time.time()) % 100000
    test_script_path = f"res://test_uat_{ts}.gd"
    test_script_content = 'extends Node\n\nvar test_val := 0\n\nfunc _ready():\n\tpass\n'

    # Test 2: write_script
    try:
        resp = client.safe_call_tool("write_script", {"path": test_script_path, "content": test_script_content})
        content = client.get_tool_content(resp)
        first_ok = content.get("success", False) or "success" in str(content).lower()

        resp2 = client.safe_call_tool("write_script", {"path": test_script_path, "content": "# dup"})
        content2 = client.get_tool_content(resp2)
        dup_error = content2.get("error") or content2.get("_error") or not content2.get("success", True)

        ok = first_ok and dup_error
        report(2, "Write Script (新建 + 重复报错)", ok,
               f"首次创建={'成功' if first_ok else '失败'}, 重复写入={'报错' if dup_error else '未报错'}")
    except Exception as e:
        report(2, "Write Script (新建 + 重复报错)", False, f"异常: {e}")

    # Test 1: read_script
    try:
        resp = client.safe_call_tool("read_script", {"path": test_script_path})
        content = client.get_tool_content(resp)
        has_content = "content" in content and isinstance(content["content"], str)
        has_lines = "line_count" in content and isinstance(content["line_count"], int)
        ok = has_content and has_lines and not content.get("_error")
        report(1, "Read Script", ok,
               f"line_count={content.get('line_count')}, content长度={len(content.get('content',''))}" if ok
               else f"响应异常: {json.dumps(content, ensure_ascii=False)[:200]}")
    except Exception as e:
        report(1, "Read Script", False, f"异常: {e}")

    # Test 3: edit_script - insert
    try:
        resp = client.safe_call_tool("edit_script", {
            "path": test_script_path, "operation": "insert", "line": 3, "content": "var inserted := 42"
        })
        resp2 = client.safe_call_tool("read_script", {"path": test_script_path})
        c2 = client.get_tool_content(resp2)
        lines = c2.get("content", "").split("\n")
        ok = len(lines) >= 3 and "inserted" in lines[2]
        report(3, "Edit Script - Insert", ok,
               f"第3行: {lines[2].strip() if len(lines) >= 3 else '(无)'}")
    except Exception as e:
        report(3, "Edit Script - Insert", False, f"异常: {e}")

    # Test 4: edit_script - replace
    try:
        resp = client.safe_call_tool("edit_script", {
            "path": test_script_path, "operation": "replace", "line": 3, "content": "var replaced := 99"
        })
        resp2 = client.safe_call_tool("read_script", {"path": test_script_path})
        c2 = client.get_tool_content(resp2)
        lines = c2.get("content", "").split("\n")
        ok = len(lines) >= 3 and "replaced" in lines[2]
        report(4, "Edit Script - Replace", ok,
               f"第3行: {lines[2].strip() if len(lines) >= 3 else '(无)'}")
    except Exception as e:
        report(4, "Edit Script - Replace", False, f"异常: {e}")

    # Test 5: edit_script - delete
    try:
        resp_before = client.safe_call_tool("read_script", {"path": test_script_path})
        lc_before = client.get_tool_content(resp_before).get("line_count", 0)
        resp = client.safe_call_tool("edit_script", {
            "path": test_script_path, "operation": "delete", "line": 3
        })
        resp_after = client.safe_call_tool("read_script", {"path": test_script_path})
        lc_after = client.get_tool_content(resp_after).get("line_count", 0)
        ok = lc_after == lc_before - 1
        report(5, "Edit Script - Delete", ok,
               f"删除前行数={lc_before}, 删除后行数={lc_after}")
    except Exception as e:
        report(5, "Edit Script - Delete", False, f"异常: {e}")

    # ===================================================================
    # GROUP D: Attach/detach (after file writes, with delay for EditorFileSystem)
    # ===================================================================
    print("\n  --- D 组: 脚本挂载/卸载测试 (等待3秒让EditorFileSystem完成扫描) ---")
    time.sleep(3)

    # Create a child node for attach/detach
    attach_target = "UATScriptTarget"
    try:
        r = client.safe_call_tool("create_node", {"type": "Node2D", "name": attach_target})
        c = client.get_tool_content(r)
        if not (c.get("success", False) or "success" in str(c).lower()):
            print(f"  警告: 创建节点失败 - {json.dumps(c, ensure_ascii=False)[:100]}")
    except Exception as e:
        print(f"  警告: 创建节点异常 - {e}")

    time.sleep(0.5)

    # Test 6: attach_script
    try:
        resp = client.safe_call_tool("attach_script", {
            "node_path": attach_target, "script_path": test_script_path
        })
        content = client.get_tool_content(resp)
        ok = content.get("success", False) or "success" in str(content).lower()
        report(6, "Attach Script", ok,
               f"结果: {json.dumps(content, ensure_ascii=False)[:150]}\n"
               f"  [MANUAL] 请确认 {attach_target} 出现脚本图标，Ctrl+Z 可撤销")
    except Exception as e:
        report(6, "Attach Script", False, f"异常: {e}")

    time.sleep(0.5)

    # Test 7: detach_script
    try:
        resp = client.safe_call_tool("detach_script", {"node_path": attach_target})
        content = client.get_tool_content(resp)
        ok = content.get("success", False) or "success" in str(content).lower()
        report(7, "Detach Script", ok,
               f"结果: {json.dumps(content, ensure_ascii=False)[:150]}\n"
               f"  [MANUAL] 请确认 {attach_target} 脚本图标已消失，Ctrl+Z 可恢复")
    except Exception as e:
        report(7, "Detach Script", False, f"异常: {e}")

    # Clean up test node
    time.sleep(0.3)
    try:
        client.safe_call_tool("delete_node", {"node_path": attach_target})
    except Exception:
        pass

    # ===================================================================
    # Cleanup
    # ===================================================================
    print("━" * 60)
    print(f"\n  清理: 请手动删除 Godot 项目中的 {test_script_path}")

    client.close()

    # ===================================================================
    # Summary
    # ===================================================================
    print("\n" + "=" * 60)
    print("  Phase 3 UAT 测试结果汇总")
    print("=" * 60)

    passed = sum(1 for _, _, ok, m in results if ok and not m)
    failed = sum(1 for _, _, ok, m in results if not ok and not m)
    manual = sum(1 for _, _, _, m in results if m)
    total = len(results)

    for num, name, ok, is_manual in results:
        tag = "[MANUAL]" if is_manual else ("[PASS]" if ok else "[FAIL]")
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  通过: {passed}/{total}  失败: {failed}/{total}  需手动: {manual}/{total}")
    print("=" * 60)

    if failed > 0:
        print("\n  存在失败的测试，请检查上方详细输出。")
        sys.exit(1)
    else:
        print("\n  所有自动化测试通过！")
        if manual > 0:
            print(f"  还有 {manual} 项需要手动验证。")
        sys.exit(0)


if __name__ == "__main__":
    run_tests()
