---
phase: 27-timeout-safety
verified: 2026-03-31T10:00:00Z
status: passed
score: 3/3 must-haves verified
re_verification: false
---

# Phase 27: Timeout Safety Verification Report

**Phase Goal:** MCP tool calls and game bridge requests have bounded response times -- the IO thread never blocks forever, and late responses from timed-out requests cannot corrupt the next request
**Verified:** 2026-03-31T10:00:00Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                                                                  | Status     | Evidence                                                                                                                                                                          |
|----|------------------------------------------------------------------------------------------------------------------------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1  | When the Godot main thread hangs, the AI client receives a JSON-RPC timeout error within 30 seconds                   | VERIFIED   | `response_cv.wait_for(lock, std::chrono::seconds(30), ...)` at mcp_server.cpp:291; on timeout sends `create_error_response(io_pending_request_id, -32001, "Tool execution timed out (30s)")` at lines 297-300 |
| 2  | When a deferred game bridge request gets no response, the AI client receives a timeout error within 15 seconds        | VERIFIED   | All 9 deferred functions set `pending_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15)` (game_bridge.cpp lines 665, 684, 703, 725, 746, 766, 799, 1019, 1041); `poll()` calls `game_bridge->expire_pending()` via `has_pending_timeout()` check (mcp_server.cpp lines 372-374) |
| 3  | A late response arriving after timeout has fired is silently discarded and does not corrupt the next request          | VERIFIED   | `queue_deferred_response` discards when `io_pending_request_id.is_null()` (cleared on timeout) or when `response["id"] != io_pending_request_id` (mcp_server.cpp lines 131-138) |

**Score:** 3/3 truths verified

### Required Artifacts

| Artifact              | Expected                                                         | Status     | Details                                                                                                                    |
|-----------------------|------------------------------------------------------------------|------------|----------------------------------------------------------------------------------------------------------------------------|
| `src/mcp_server.h`    | `io_pending_request_id` field for request tracking               | VERIFIED   | Line 81: `nlohmann::json io_pending_request_id;  // Tracks which request the IO thread is waiting for`                   |
| `src/mcp_server.cpp`  | 30s IO thread timeout via `wait_for`, stale response discard     | VERIFIED   | `wait_for(30s)` at line 291; `io_pending_request_id` used at lines 131, 135, 298, 301, 311, 342; discard logic at 131-138 |
| `src/game_bridge.h`   | `pending_deadline` field and `has_pending_timeout`/`expire_pending` methods | VERIFIED | Line 92: `pending_deadline` field; lines 64-65: method declarations confirmed                                             |
| `src/game_bridge.cpp` | `expire_pending` implementation delivering timeout error via `deferred_callback` | VERIFIED | Lines 411-458: full `expire_pending` with per-type switch, clears state before calling callback; `has_pending_timeout` at lines 406-409 |

### Key Link Verification

| From                                         | To                                     | Via                                                   | Status  | Details                                                                                                      |
|----------------------------------------------|----------------------------------------|-------------------------------------------------------|---------|--------------------------------------------------------------------------------------------------------------|
| `mcp_server.cpp` (io_thread_func)            | `response_cv.wait_for(30s)`            | `std::condition_variable_any::wait_for` replacing `wait` | WIRED  | Line 291: `response_cv.wait_for(lock, std::chrono::seconds(30), ...)` confirmed; old unbounded `response_cv.wait(lock` not found in file |
| `mcp_server.cpp` (poll)                      | `game_bridge->expire_pending`          | `has_pending_timeout` check in poll loop              | WIRED   | Lines 372-374: `if (game_bridge && game_bridge->has_pending_timeout()) { game_bridge->expire_pending(); }`  |
| `mcp_server.cpp` (queue_deferred_response)   | `io_pending_request_id`                | ID comparison to discard stale responses              | WIRED   | Lines 131-138: null check and ID mismatch check both implemented under `queue_mutex` lock                   |

### Data-Flow Trace (Level 4)

Not applicable -- this phase modifies threading/timeout behavior, not data rendering components.

### Behavioral Spot-Checks

Step 7b skipped: requires a running Godot instance with a connected MCP client to trigger the 30s/15s timeouts. Timeout behavior is not testable without a live server. The code paths are verified structurally.

The following spot-checks are delegated to human verification:

| Behavior                              | Why not automatable                                                   |
|---------------------------------------|-----------------------------------------------------------------------|
| IO thread 30s timeout fires correctly | Requires MCP client connected, main thread artificially blocked 30s  |
| Game bridge 15s timeout fires         | Requires running game that drops a deferred request without responding |
| Stale response discarded              | Requires timing a late response after a timeout has already been sent |

### Requirements Coverage

| Requirement | Source Plan | Description                                                                            | Status    | Evidence                                                                                               |
|-------------|-------------|----------------------------------------------------------------------------------------|-----------|--------------------------------------------------------------------------------------------------------|
| TIMEOUT-01  | 27-01-PLAN  | IO thread returns JSON-RPC timeout error within 30s when main thread doesn't respond  | SATISFIED | `wait_for(30s)` in io_thread_func; error code -32001 with "Tool execution timed out (30s)" confirmed  |
| TIMEOUT-02  | 27-01-PLAN  | Game bridge deferred requests return timeout error within 15s when game doesn't respond | SATISFIED | 15s deadline set in all 9 deferred functions; `poll()` → `expire_pending()` wired                    |
| TIMEOUT-03  | 27-01-PLAN  | Late responses after timeout are discarded, do not corrupt next request                | SATISFIED | `queue_deferred_response` guards with null check and ID mismatch check                                 |

No orphaned requirements -- REQUIREMENTS.md maps exactly TIMEOUT-01, TIMEOUT-02, TIMEOUT-03 to Phase 27, all three claimed by 27-01-PLAN.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None | -    | -       | -        | -      |

No TODO/FIXME/placeholder patterns found in the modified files for Phase 27 code paths. The `expire_pending` implementation is complete with all 9 PendingType cases handled including a `default` fallback. No empty handlers or stub returns detected.

One design note (not a blocker): `io_pending_request_id` is default-constructed as `nlohmann::json` (which is `null`). The null check at `queue_deferred_response` line 131 means no deferred response can be delivered unless a request is actively being waited on. This is the correct invariant: deferred responses are only meaningful when the IO thread is in a `wait_for`. The initial null state is intentional.

### Human Verification Required

#### 1. IO Thread 30s Timeout Fires

**Test:** Connect an MCP client, call any tool (e.g., `get_scene_tree`) while the Godot editor main thread is blocked for more than 30 seconds (e.g., by attaching a debugger breakpoint or running an infinite GDScript loop in `_process`).
**Expected:** AI client receives a JSON-RPC error response with code -32001 and message "Tool execution timed out (30s)" within 30 seconds. The MCP connection remains open and subsequent tool calls succeed.
**Why human:** Requires artificially stalling the main thread for 30 seconds in a live Godot editor session.

#### 2. Game Bridge 15s Timeout Fires

**Test:** Run the game with the companion GDScript. Call `capture_game_viewport` or `eval_in_game`. Before the game responds, either pause the game in the debugger or disconnect the companion's message handler. Wait 15 seconds.
**Expected:** AI client receives a timeout error with a message like "Game viewport capture timed out (15s)" or "eval_in_game timed out (15s)". The server is ready for the next request immediately after.
**Why human:** Requires controlling game response timing in a live session.

#### 3. Stale Response Does Not Corrupt Next Request

**Test:** Trigger a game bridge timeout as in test 2, then immediately issue a second tool call (e.g., `get_scene_tree`) before the game sends its (now late) deferred response. Arrange for the game to send the stale response during the second tool's execution.
**Expected:** The second tool call receives its own correct response; the stale game bridge response is silently discarded and does not appear as the result of the second call.
**Why human:** Requires precise timing control to inject a stale response during a subsequent request.

### Gaps Summary

No gaps found. All three observable truths are fully verified at all levels:
- The unbounded `response_cv.wait(lock, ...)` is confirmed gone (grep returns zero matches).
- `wait_for` with 30-second duration is in place with timeout error construction.
- All 9 deferred game bridge operations set a 15-second deadline at initiation time.
- `poll()` is wired to check and expire pending bridge requests every frame.
- `queue_deferred_response` discards stale responses under mutex lock using both null check and ID comparison.
- Both implementation commits (cd76dc7, 441fe85) exist in git history with substantive diffs.
- REQUIREMENTS.md shows all three TIMEOUT requirements marked complete and mapped to Phase 27.

---

_Verified: 2026-03-31T10:00:00Z_
_Verifier: Claude (gsd-verifier)_
