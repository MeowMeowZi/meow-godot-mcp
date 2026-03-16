---
phase: 01-foundation-first-tool
plan: 03
subsystem: infra
tags: [bridge, tcp-client, stdio-transport, winsock, c++17, relay, cli]

# Dependency graph
requires:
  - phase: 01-01
    provides: SConstruct build system with bridge placeholder target
provides:
  - Bridge executable (godot-mcp-bridge) that relays stdio to TCP
  - StdioTransport class for newline-delimited JSON on stdin/stdout
  - TcpClient class for TCP connection with fixed-interval retry
  - SConstruct bridge target (scons bridge)
affects: [01-04]

# Tech tracking
tech-stack:
  added: [winsock2 (ws2_32), std::thread, std::mutex, std::atomic]
  patterns: [two-thread relay (stdin reader + main TCP poller), thread-safe message queue, fixed-interval retry]

key-files:
  created:
    - bridge/main.cpp
    - bridge/stdio_transport.h
    - bridge/stdio_transport.cpp
    - bridge/tcp_client.h
    - bridge/tcp_client.cpp
  modified:
    - SConstruct

key-decisions:
  - "Bridge uses separate SCons Environment() (not env.Clone()) to avoid any godot-cpp dependency"
  - "Two-thread design: blocking stdin reader thread + main thread TCP polling with 10ms sleep to avoid busy-wait"
  - "MessageQueue uses simple mutex+queue (no condition_variable) with try_pop for non-blocking drain"
  - "TCP receive uses select() with 100ms timeout for non-blocking readability check"
  - "Reconnect on TCP disconnect stays alive indefinitely with fixed 2-second interval per user decision"

patterns-established:
  - "Bridge is a transparent relay: does not parse, validate, or modify JSON-RPC messages"
  - "All bridge diagnostic logging goes to stderr via [godot-mcp-bridge] prefix, never stdout"
  - "Windows binary mode on stdin/stdout via _setmode(_fileno(), _O_BINARY) to prevent CRLF corruption"
  - "Platform-specific code guarded by #ifdef _WIN32 throughout bridge sources"

requirements-completed: [MCP-01]

# Metrics
duration: 3min
completed: 2026-03-16
---

# Phase 1 Plan 03: Bridge Executable Summary

**Standalone stdio-to-TCP bridge executable with two-thread relay loop, fixed 2-second retry, and CLI argument parsing for AI client subprocess spawning**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-16T03:16:48Z
- **Completed:** 2026-03-16T03:20:00Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Bridge executable compiles as standalone C++17 binary (315KB debug, no godot-cpp dependency)
- StdioTransport handles Windows binary mode and newline-delimited JSON framing on stdin/stdout
- TcpClient connects to localhost with fixed 2-second retry (15 attempts, 30s timeout) using platform sockets
- Main relay loop uses two-thread architecture: stdin reader thread + main thread TCP polling
- CLI accepts --port (default 6800), --host, and --help arguments
- SConstruct updated with production bridge target (replaces placeholder from Plan 01)

## Task Commits

Each task was committed atomically:

1. **Task 1: Stdio transport and TCP client modules** - `15b2e9a` (feat)
2. **Task 2: Bridge main entry point, CLI args, relay loop, and SConstruct bridge target** - `3aa9ebb` (feat)

## Files Created/Modified
- `bridge/stdio_transport.h` - StdioTransport class: stdin/stdout message reading/writing with binary mode
- `bridge/stdio_transport.cpp` - Newline-delimited JSON framing, stderr logging with [godot-mcp-bridge] prefix
- `bridge/tcp_client.h` - TcpClient class: TCP connection with retry, send/receive with newline framing
- `bridge/tcp_client.cpp` - Platform socket implementation (Winsock/POSIX), select() timeout, buffered receive
- `bridge/main.cpp` - Bridge entry point with CLI parsing, two-thread relay loop, reconnect logic
- `SConstruct` - Replaced bridge placeholder with standalone Environment() target (scons bridge)

## Decisions Made
- Used separate `Environment()` in SConstruct instead of `env.Clone()` to ensure zero godot-cpp dependency
- Two-thread relay design: stdin reader thread blocks on std::getline, main thread polls TCP and stdin queue -- avoids impossible select() on Windows console stdin handles
- Thread-safe MessageQueue with std::mutex and try_pop (no condition_variable needed since main loop polls with 10ms sleep)
- select() with 100ms timeout for TCP readability check -- balances responsiveness with CPU efficiency
- Reconnect on TCP disconnect stays alive indefinitely per user decision (bridge outlives Godot restarts)
- Debug build size (315KB) acceptable for development; release/optimized build would be much smaller

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Bridge executable proven: compiles, runs, shows help with correct defaults
- Ready for Plan 04: end-to-end integration (bridge + GDExtension TCP server + get_scene_tree tool)
- Bridge binary output at project/addons/godot_mcp_meow/bin/godot-mcp-bridge.exe alongside GDExtension DLL

## Self-Check: PASSED

All 6 created/modified files verified present. Both task commits (15b2e9a, 3aa9ebb) verified in git log.

---
*Phase: 01-foundation-first-tool*
*Completed: 2026-03-16*
