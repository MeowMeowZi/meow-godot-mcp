# Phase 7: Deferred Items

## Pre-existing Issues

1. **test_protocol.cpp line 108**: `ASSERT_EQ(tools.size(), 23)` is stale -- should be 29 after Phase 7 Plan 01 added 6 UI tools. The canonical test in test_tool_registry.cpp correctly asserts 29. This test_protocol.cpp assertion was missed during Plan 01 execution.
