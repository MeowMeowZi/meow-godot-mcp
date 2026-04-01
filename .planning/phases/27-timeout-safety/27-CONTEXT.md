# Phase 27: Timeout Safety - Context

**Gathered:** 2026-04-01
**Status:** Ready for planning

<domain>
## Phase Boundary

MCP tool calls and game bridge requests have bounded response times. The IO thread never blocks forever, and late responses from timed-out requests cannot corrupt the next request.

</domain>

<decisions>
## Implementation Decisions

### IO Thread Timeout
- Use std::condition_variable_any::wait_for() with 30 second timeout (replacing unbounded wait())
- Tag each request with its JSON-RPC request ID; when response arrives, compare ID — discard mismatches (stale responses from timed-out requests)
- On timeout: return JSON-RPC error code -32001, message "Tool execution timed out (30s)"
- Add io_pending_request_id field to MCPServer to track which request is being waited on

### Game Bridge Timeout
- 15 second timeout for deferred game bridge requests (viewport capture, eval_in_game, etc.)
- Check deadline in MCPServer::poll() — same pattern as existing bridge_wait_deadline
- Add pending_deadline field (std::chrono::steady_clock::time_point) to GameBridge
- Add has_pending_timeout() and expire_pending() methods to GameBridge
- On timeout: first clear pending_type to NONE, then construct and deliver error response — prevents race with _capture() arriving same frame
- Timeout error uses deferred_callback to deliver JSON-RPC error to IO thread

### Claude's Discretion
- Exact error message wording for game bridge timeouts (per-PendingType messages)
- Whether to make timeout values configurable via ProjectSettings (recommended: hardcode for now)

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `mcp_server.cpp:277-286` — Current unbounded wait() + response_queue pattern (modify in place)
- `mcp_server.cpp:320-340` — Existing bridge_wait_deadline pattern in poll() (copy pattern for deferred timeout)
- `game_bridge.h:84-90` — Pending deferred request state (pending_type, pending_id, deferred_callback)
- `game_bridge.cpp:49-87` — Game disconnect handler already clears pending state (reference pattern)

### Established Patterns
- response_cv uses std::condition_variable_any with std::recursive_mutex
- Deferred responses delivered via deferred_callback function
- poll() called from main thread, checks state and delivers responses

### Integration Points
- mcp_server.h/cpp — IO thread wait, response_queue, io_pending_request_id
- game_bridge.h/cpp — pending_deadline, has_pending_timeout(), expire_pending()
- MCPServer::poll() — add deferred timeout check alongside existing bridge_wait_deadline check

</code_context>

<specifics>
## Specific Ideas

- Research confirmed: IO thread response_cv.wait() at mcp_server.cpp:278 has NO timeout — this is the root cause of AI client hangs
- 9 out of 10 deferred operations have no timeout (only run_game has bridge_wait_deadline)
- Must handle the race where a response arrives at the exact moment timeout fires

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>
