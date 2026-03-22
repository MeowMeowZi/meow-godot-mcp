---
status: investigating
trigger: "After Claude connects to the MCP server, Godot keeps spamming 'MCP Meow: 客户端已连接' (client connected) messages at extreme speed (dozens per second)"
created: 2026-03-19T00:00:00Z
updated: 2026-03-19T00:00:00Z
---

## Current Focus

hypothesis: Connection accept or status check runs every frame without proper guard
test: Search for the Chinese text and trace the logging logic
expecting: Find a loop or polling function that logs on every iteration
next_action: Search codebase for "客户端已连接" to find the source of the message

## Symptoms

expected: Connection prints "客户端已连接" once, then stays quiet waiting for MCP commands
actual: "MCP Meow: 客户端已连接" message repeats dozens of times per second, non-stop scrolling in Godot output
errors: No actual errors, just spam logging
reproduction: Connect Claude to the MCP server - the spam starts immediately
started: Previously worked correctly (printed once). Recently started spamming.

## Eliminated

## Evidence

## Resolution

root_cause:
fix:
verification:
files_changed: []
