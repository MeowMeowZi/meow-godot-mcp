#!/usr/bin/env python3
"""Isolate attach_script crash: new file vs pre-existing file."""

import json
import socket
import time
import sys

def send(sock, obj):
    sock.sendall((json.dumps(obj) + "\n").encode())

def recv(sock, buf):
    while "\n" not in buf[0]:
        chunk = sock.recv(65536).decode()
        if not chunk: raise ConnectionError("closed")
        buf[0] += chunk
    line, buf[0] = buf[0].split("\n", 1)
    return json.loads(line)

def request(sock, buf, method, params=None, rid=[0]):
    rid[0] += 1
    msg = {"jsonrpc": "2.0", "id": rid[0], "method": method}
    if params: msg["params"] = params
    send(sock, msg)
    return recv(sock, buf)

def call_tool(sock, buf, name, args=None):
    params = {"name": name}
    if args: params["arguments"] = args
    return request(sock, buf, "tools/call", params)

def get_content(r):
    if "error" in r: return {"_error": r["error"]["message"]}
    return json.loads(r["result"]["content"][0]["text"])

def step(label, fn):
    print(f"  [{label}] ", end="", flush=True)
    try:
        result = fn()
        print(f"OK - {result}" if result else "OK")
        return True
    except Exception as e:
        print(f"CRASH - {e}")
        return False

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10)
buf = [""]

print("连接...")
sock.connect(("127.0.0.1", 6800))
request(sock, buf, "initialize")
send(sock, {"jsonrpc": "2.0", "method": "notifications/initialized"})
time.sleep(0.3)
print("已连接\n")

# Step 1: Write a new .gd file
ts = int(time.time()) % 100000
new_path = f"res://test_repro_{ts}.gd"
print(f"=== Step 1: 创建新脚本 {new_path} ===")
step("write_script", lambda: get_content(call_tool(sock, buf, "write_script", {"path": new_path, "content": "extends Node\n\nfunc _ready():\n\tpass\n"})))

# Step 2: Create a node
print(f"\n=== Step 2: 创建节点 ===")
step("create_node", lambda: get_content(call_tool(sock, buf, "create_node", {"type": "Node2D", "name": "TestTarget"})))

# Step 3: Try attach with the NEW file
print(f"\n=== Step 3: attach_script (新创建的文件) ===")
time.sleep(1)
ok3 = step("attach_script (new file)", lambda: get_content(call_tool(sock, buf, "attach_script", {"node_path": "TestTarget", "script_path": new_path})))

if not ok3:
    print("\n  >>> 新文件导致崩溃! <<<")
    print(f"  请重新打开 Godot 后手动删除 {new_path}")
    sys.exit(1)

# Step 4: Detach
print(f"\n=== Step 4: detach ===")
step("detach_script", lambda: get_content(call_tool(sock, buf, "detach_script", {"node_path": "TestTarget"})))

# Step 5: Delete test node
step("delete_node", lambda: get_content(call_tool(sock, buf, "delete_node", {"node_path": "TestTarget"})))

print(f"\n全部通过! 请手动删除 {new_path}")
sock.close()
