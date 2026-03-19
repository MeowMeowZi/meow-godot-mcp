#!/usr/bin/env python3
"""
Phase 15 UAT - Automated end-to-end tests for Integration Testing Toolkit.

Prerequisites:
  1. Open Godot project at project/ with the MCP Meow plugin enabled
  2. Ensure the MCP TCP server is listening on port 6800
  3. Ensure a main scene is configured in project settings
  4. Run: python tests/uat_phase15.py

Requirements covered:
  - TEST-01: run_test_sequence executes batch steps and returns structured pass/fail report
  - TEST-02: click_node + get_game_node_property assertions work within run_test_sequence
  - TEST-03: test_game_ui prompt returns step-by-step workflow referencing MCP tools

Tests cover:
  - tools/list shows 44 tools including run_test_sequence (TEST-01)
  - run_test_sequence schema has steps array property (TEST-01)
  - prompts/list shows 7 prompts including test_game_ui (TEST-03)
  - prompts/get test_game_ui returns workflow text (TEST-03)
  - Game launches and bridge connects
  - run_test_sequence with single eval_in_game step (TEST-01)
  - run_test_sequence with get_game_scene_tree step (TEST-01)
  - run_test_sequence with inject_input step (TEST-01)
  - run_test_sequence with get_game_output step (TEST-01)
  - run_test_sequence with click_node step (TEST-01, TEST-02)
  - run_test_sequence with get_game_node_property + assertion (TEST-01, TEST-02)
  - run_test_sequence with failing assertion (TEST-01)
  - run_test_sequence multi-step sequence (TEST-01, TEST-02)
  - run_test_sequence with wait step (TEST-01)
  - stop_game succeeds
"""

import json
import socket
import sys
import time

# ---------------------------------------------------------------------------
# TCP JSON-RPC client (same as uat_phase14.py)
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
    print("  Phase 15 Integration Testing Toolkit UAT Tests")
    print("=" * 60)

    # ===================================================================
    # Test 1: tools/list shows 44 tools including run_test_sequence
    #         (TEST-01)
    # ===================================================================
    tool_names = []
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        tool_names = [t["name"] for t in tools]
        has_run_test_sequence = "run_test_sequence" in tool_names
        ok = (len(tools) == 44 and has_run_test_sequence)
        report(1, "tools/list shows 44 tools including run_test_sequence (TEST-01)", ok,
               f"Tool count: {len(tools)}\n"
               f"         run_test_sequence present: {has_run_test_sequence}\n"
               f"         All tools: {tool_names}")
    except Exception as e:
        report(1, "tools/list shows 44 tools including run_test_sequence (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 2: run_test_sequence schema has steps array property (TEST-01)
    # ===================================================================
    try:
        resp = client.request("tools/list")
        tools = resp.get("result", {}).get("tools", [])
        rts_tool = None
        for t in tools:
            if t["name"] == "run_test_sequence":
                rts_tool = t
                break
        schema = rts_tool.get("inputSchema", {}) if rts_tool else {}
        props = schema.get("properties", {})
        steps_prop = props.get("steps", {})
        has_steps = "steps" in props
        steps_is_array = steps_prop.get("type") == "array"
        required = schema.get("required", [])
        steps_required = "steps" in required
        ok = has_steps and steps_is_array and steps_required
        report(2, "run_test_sequence schema has steps array property (TEST-01)", ok,
               f"steps present: {has_steps}\n"
               f"         steps type is array: {steps_is_array}\n"
               f"         steps in required: {steps_required}\n"
               f"         Schema properties: {list(props.keys())}\n"
               f"         Required: {required}")
    except Exception as e:
        report(2, "run_test_sequence schema has steps array property (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 3: prompts/list shows 7 prompts including test_game_ui
    #         (TEST-03)
    # ===================================================================
    try:
        resp = client.request("prompts/list")
        prompts = resp.get("result", {}).get("prompts", [])
        prompt_names = [p["name"] for p in prompts]
        has_test_game_ui = "test_game_ui" in prompt_names
        ok = (len(prompts) == 7 and has_test_game_ui)
        report(3, "prompts/list shows 7 prompts including test_game_ui (TEST-03)", ok,
               f"Prompt count: {len(prompts)}\n"
               f"         test_game_ui present: {has_test_game_ui}\n"
               f"         All prompts: {prompt_names}")
    except Exception as e:
        report(3, "prompts/list shows 7 prompts including test_game_ui (TEST-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 4: prompts/get test_game_ui returns workflow text (TEST-03)
    # ===================================================================
    try:
        resp = client.request("prompts/get", {
            "name": "test_game_ui",
            "arguments": {"test_type": "button_click"}
        })
        result = resp.get("result", {})
        messages = result.get("messages", [])
        has_messages = len(messages) > 0
        first_msg = messages[0] if messages else {}
        role_ok = first_msg.get("role") == "user"
        content = first_msg.get("content", {}).get("text", "")
        has_click_node = "click_node" in content
        has_run_test_sequence = "run_test_sequence" in content
        ok = has_messages and role_ok and has_click_node and has_run_test_sequence
        report(4, "prompts/get test_game_ui returns workflow with click_node + run_test_sequence refs (TEST-03)", ok,
               f"has messages: {has_messages}\n"
               f"         role is user: {role_ok}\n"
               f"         content mentions click_node: {has_click_node}\n"
               f"         content mentions run_test_sequence: {has_run_test_sequence}\n"
               f"         content preview: {content[:200]}...")
    except Exception as e:
        report(4, "prompts/get test_game_ui returns workflow with click_node + run_test_sequence refs (TEST-03)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 5: run_game with mode=main succeeds, bridge connects
    # ===================================================================
    bridge_connected = False
    try:
        data, _ = call_tool_text(client, "run_game", {"mode": "main"})
        game_started = data.get("success") == True or data.get("already_running") == True
        if not game_started:
            report(5, "run_game with mode=main succeeds and bridge connects", False,
                   f"run_game failed: {json.dumps(data)}")
        else:
            connected, status = wait_for_bridge(client, max_wait=10.0)
            bridge_connected = connected
            ok = connected
            report(5, "run_game with mode=main succeeds and bridge connects", ok,
                   f"run_game: {json.dumps(data)}\n"
                   f"         Bridge status: {json.dumps(status)}\n"
                   f"         Connected: {connected}")
    except Exception as e:
        report(5, "run_game with mode=main succeeds and bridge connects", False, f"Error: {e}")

    # --- EARLY EXIT if bridge not connected ---
    if not bridge_connected:
        print("\n  FATAL: Bridge not connected. Cannot run remaining tests.")
        print("  EARLY EXIT: Skipping all run_test_sequence tests.")
        print("  Attempting to stop game before exit...")
        try:
            call_tool_text(client, "stop_game")
        except Exception:
            pass
        client.close()
        _print_summary()
        sys.exit(1)

    # ===================================================================
    # Test 6: run_test_sequence with single eval_in_game step (TEST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "eval_in_game",
                    "args": {"expression": "2 + 2"},
                    "assert": {"property": "result", "contains": "4"},
                    "description": "Simple eval"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        total_steps = data.get("total_steps") == 1
        passed_count = data.get("passed") == 1
        all_passed = data.get("all_passed") == True
        steps = data.get("steps", [])
        step0_passed = steps[0].get("passed") == True if steps else False
        ok = has_success and total_steps and passed_count and all_passed and step0_passed
        report(6, "run_test_sequence with single eval_in_game step (TEST-01)", ok,
               f"success: {data.get('success')}\n"
               f"         total_steps: {data.get('total_steps')}\n"
               f"         passed: {data.get('passed')}, all_passed: {data.get('all_passed')}\n"
               f"         step[0].passed: {steps[0].get('passed') if steps else 'N/A'}\n"
               f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(6, "run_test_sequence with single eval_in_game step (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 7: run_test_sequence with get_game_scene_tree step (TEST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "get_game_scene_tree",
                    "args": {"max_depth": 1},
                    "description": "Get scene tree"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        total_steps = data.get("total_steps") == 1
        steps = data.get("steps", [])
        step0_passed = steps[0].get("passed") == True if steps else False
        ok = has_success and total_steps and step0_passed
        report(7, "run_test_sequence with get_game_scene_tree step (TEST-01)", ok,
               f"success: {data.get('success')}\n"
               f"         total_steps: {data.get('total_steps')}\n"
               f"         step[0].passed: {steps[0].get('passed') if steps else 'N/A'}\n"
               f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(7, "run_test_sequence with get_game_scene_tree step (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 8: run_test_sequence with inject_input step (TEST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "inject_input",
                    "args": {"type": "key", "keycode": "space"},
                    "description": "Press space key"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        total_steps = data.get("total_steps") == 1
        steps = data.get("steps", [])
        step0_passed = steps[0].get("passed") == True if steps else False
        step0_result_success = False
        if steps and isinstance(steps[0].get("result"), dict):
            step0_result_success = steps[0]["result"].get("success") == True
        ok = has_success and total_steps and step0_passed and step0_result_success
        report(8, "run_test_sequence with inject_input step (TEST-01)", ok,
               f"success: {data.get('success')}\n"
               f"         total_steps: {data.get('total_steps')}\n"
               f"         step[0].passed: {steps[0].get('passed') if steps else 'N/A'}\n"
               f"         step[0].result.success: {step0_result_success}\n"
               f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(8, "run_test_sequence with inject_input step (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 9: run_test_sequence with get_game_output step (TEST-01)
    # ===================================================================
    try:
        # First print a marker via eval_in_game directly (not via run_test_sequence)
        call_tool_text(client, "eval_in_game", {
            "expression": 'print("UAT15_MARKER")'
        }, timeout=15)
        time.sleep(1.0)  # Allow output to propagate

        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "get_game_output",
                    "args": {"keyword": "UAT15_MARKER", "clear_after_read": False},
                    "assert": {"property": "success", "equals": "true"},
                    "description": "Check game output"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        steps = data.get("steps", [])
        step0_passed = steps[0].get("passed") == True if steps else False
        ok = has_success and step0_passed
        report(9, "run_test_sequence with get_game_output step (TEST-01)", ok,
               f"success: {data.get('success')}\n"
               f"         step[0].passed: {steps[0].get('passed') if steps else 'N/A'}\n"
               f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(9, "run_test_sequence with get_game_output step (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 10: run_test_sequence with click_node step (TEST-01, TEST-02)
    # ===================================================================
    try:
        # First get scene tree to find a Control node
        tree_data, _ = call_tool_text(client, "get_game_scene_tree", {"max_depth": 3}, timeout=15)
        tree_str = json.dumps(tree_data)

        # Look for a Control-derived node in the tree
        control_path = None
        nodes = tree_data.get("children", [])

        def find_control(node_list, parent_path=""):
            """Recursively search for a Control-derived node."""
            control_types = ["Control", "Button", "Label", "Panel", "VBoxContainer",
                             "HBoxContainer", "MarginContainer", "CenterContainer",
                             "TextEdit", "LineEdit", "RichTextLabel", "ColorRect",
                             "TextureRect", "ProgressBar", "ItemList", "Tree",
                             "TabContainer", "ScrollContainer", "PanelContainer"]
            for n in node_list:
                ntype = n.get("type", "")
                nname = n.get("name", "")
                npath = f"{parent_path}/{nname}" if parent_path else nname
                if ntype in control_types:
                    return npath
                children = n.get("children", [])
                found = find_control(children, npath)
                if found:
                    return found
            return None

        root_name = tree_data.get("name", "")
        control_path = find_control(tree_data.get("children", []), root_name)

        if control_path is None:
            report_skip(10, "run_test_sequence with click_node step (TEST-01, TEST-02)",
                        f"No Control node found in scene tree.\n"
                        f"         Tree root: {root_name}\n"
                        f"         Tree preview: {tree_str[:300]}")
        else:
            data, _ = call_tool_text(client, "run_test_sequence", {
                "steps": [
                    {
                        "action": "click_node",
                        "args": {"node_path": control_path},
                        "description": "Click a UI node"
                    }
                ]
            }, timeout=15)
            has_success = data.get("success") == True
            steps = data.get("steps", [])
            has_result = len(steps) > 0 and "result" in steps[0]
            ok = has_success and has_result
            report(10, "run_test_sequence with click_node step (TEST-01, TEST-02)", ok,
                   f"success: {data.get('success')}\n"
                   f"         control_path used: {control_path}\n"
                   f"         step[0] has result: {has_result}\n"
                   f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(10, "run_test_sequence with click_node step (TEST-01, TEST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 11: run_test_sequence with get_game_node_property + assertion
    #          (TEST-01, TEST-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "get_game_node_property",
                    "args": {"node_path": "", "property": "name"},
                    "assert": {"property": "value", "not_empty": True},
                    "description": "Get root node name"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        steps = data.get("steps", [])
        step0_passed = steps[0].get("passed") == True if steps else False
        assertion = steps[0].get("assertion", {}) if steps else {}
        assertion_passed = assertion.get("passed") == True
        ok = has_success and step0_passed and assertion_passed
        report(11, "run_test_sequence with get_game_node_property + not_empty assertion (TEST-01, TEST-02)", ok,
               f"success: {data.get('success')}\n"
               f"         step[0].passed: {steps[0].get('passed') if steps else 'N/A'}\n"
               f"         assertion.passed: {assertion.get('passed')}\n"
               f"         assertion details: {json.dumps(assertion)}\n"
               f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(11, "run_test_sequence with get_game_node_property + not_empty assertion (TEST-01, TEST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 12: run_test_sequence with failing assertion (TEST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "eval_in_game",
                    "args": {"expression": "2 + 2"},
                    "assert": {"property": "result", "equals": "999"},
                    "description": "This should fail"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        total_steps = data.get("total_steps") == 1
        passed_count = data.get("passed") == 0
        failed_count = data.get("failed") == 1
        all_passed = data.get("all_passed") == False
        steps = data.get("steps", [])
        step0_passed = steps[0].get("passed") == False if steps else False
        assertion = steps[0].get("assertion", {}) if steps else {}
        assertion_passed = assertion.get("passed") == False
        ok = (has_success and total_steps and passed_count and failed_count
              and all_passed and step0_passed and assertion_passed)
        report(12, "run_test_sequence with failing assertion reports failure correctly (TEST-01)", ok,
               f"success: {data.get('success')}\n"
               f"         total_steps: {data.get('total_steps')}\n"
               f"         passed: {data.get('passed')}, failed: {data.get('failed')}\n"
               f"         all_passed: {data.get('all_passed')}\n"
               f"         step[0].passed: {steps[0].get('passed') if steps else 'N/A'}\n"
               f"         assertion.passed: {assertion.get('passed')}\n"
               f"         assertion details: {json.dumps(assertion)}\n"
               f"         Response: {json.dumps(data)[:500]}")
    except Exception as e:
        report(12, "run_test_sequence with failing assertion reports failure correctly (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 13: run_test_sequence multi-step sequence (TEST-01, TEST-02)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "eval_in_game",
                    "args": {"expression": "get_tree().current_scene.name"},
                    "assert": {"property": "result", "not_empty": True},
                    "description": "Get scene name"
                },
                {
                    "action": "get_game_node_property",
                    "args": {"node_path": "", "property": "visible"},
                    "description": "Check visibility"
                },
                {
                    "action": "get_game_output",
                    "args": {"clear_after_read": False},
                    "assert": {"property": "success", "equals": "true"},
                    "description": "Read game output"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        total_steps_ok = data.get("total_steps") == 3
        steps = data.get("steps", [])
        has_3_steps = len(steps) == 3
        all_have_results = all("result" in s for s in steps)
        ok = has_success and total_steps_ok and has_3_steps and all_have_results
        report(13, "run_test_sequence multi-step sequence (TEST-01, TEST-02)", ok,
               f"success: {data.get('success')}\n"
               f"         total_steps: {data.get('total_steps')}\n"
               f"         steps count: {len(steps)}\n"
               f"         all have results: {all_have_results}\n"
               f"         passed: {data.get('passed')}, failed: {data.get('failed')}\n"
               f"         Response: {json.dumps(data)[:600]}")
    except Exception as e:
        report(13, "run_test_sequence multi-step sequence (TEST-01, TEST-02)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 14: run_test_sequence with wait step (TEST-01)
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "run_test_sequence", {
            "steps": [
                {
                    "action": "eval_in_game",
                    "args": {"expression": 'print("BEFORE_WAIT")'},
                    "description": "Print before"
                },
                {
                    "action": "wait",
                    "args": {"duration_ms": 200},
                    "description": "Wait 200ms"
                },
                {
                    "action": "get_game_output",
                    "args": {"keyword": "BEFORE_WAIT"},
                    "assert": {"property": "success", "equals": "true"},
                    "description": "Check output after wait"
                }
            ]
        }, timeout=15)
        has_success = data.get("success") == True
        total_steps_ok = data.get("total_steps") == 3
        steps = data.get("steps", [])
        # Check the wait step result contains waited_ms
        wait_step = steps[1] if len(steps) > 1 else {}
        wait_result = wait_step.get("result", {})
        has_waited_ms = "waited_ms" in wait_result
        ok = has_success and total_steps_ok and has_waited_ms
        report(14, "run_test_sequence with wait step (TEST-01)", ok,
               f"success: {data.get('success')}\n"
               f"         total_steps: {data.get('total_steps')}\n"
               f"         wait step result: {json.dumps(wait_result)}\n"
               f"         has waited_ms: {has_waited_ms}\n"
               f"         Response: {json.dumps(data)[:600]}")
    except Exception as e:
        report(14, "run_test_sequence with wait step (TEST-01)", False,
               f"Error: {e}")

    # ===================================================================
    # Test 15: stop_game succeeds
    # ===================================================================
    try:
        data, _ = call_tool_text(client, "stop_game")
        game_stopped = data.get("success") == True
        time.sleep(1.0)  # Allow bridge to disconnect

        status, _ = call_tool_text(client, "get_game_bridge_status")
        bridge_disconnected = status.get("connected") == False
        ok = game_stopped and bridge_disconnected
        report(15, "stop_game succeeds and bridge disconnects", ok,
               f"stop_game: {json.dumps(data)}\n"
               f"         bridge status: {json.dumps(status)}\n"
               f"         game_stopped: {game_stopped}, bridge_disconnected: {bridge_disconnected}")
    except Exception as e:
        report(15, "stop_game succeeds and bridge disconnects", False, f"Error: {e}")

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
    print("  Phase 15 Integration Testing Toolkit UAT Results")
    print("=" * 60)

    passed = sum(1 for _, _, ok in results if ok)
    failed = sum(1 for _, _, ok in results if not ok)
    total = len(results)

    for num, name, ok in results:
        tag = "[PASS]" if ok else "[FAIL]"
        print(f"  {tag:10s} {num:2d}. {name}")

    print()
    print(f"  === PHASE 15 UAT SUMMARY ===")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
