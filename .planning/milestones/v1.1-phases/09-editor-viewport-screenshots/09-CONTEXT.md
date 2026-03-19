# Phase 9: Editor Viewport Screenshots - Context

**Gathered:** 2026-03-18
**Status:** Ready for planning

<domain>
## Phase Boundary

AI can see what the editor viewport looks like via captured images. Covers capturing 2D and 3D editor viewport screenshots as PNG, returning them as MCP ImageContent for AI client rendering. Does NOT cover game viewport (running game), video capture, viewport streaming, or non-editor viewports.

Requirements: VWPT-01, VWPT-02, VWPT-03

</domain>

<decisions>
## Implementation Decisions

### Tool Architecture — 1 Tool with viewport_type Parameter
- **capture_viewport** — Single tool with `viewport_type` parameter ("2d" / "3d"). Logic is identical for both, only target viewport differs.
- Immediate capture of current frame — may be one frame stale but simple and reliable, avoids async complexity
- Optional `width` / `height` parameters for resolution control; defaults to current viewport resolution, scales if specified
- New `viewport_tools.h` / `viewport_tools.cpp` module

### MCP ImageContent Protocol
- Return format: MCP ImageContent `{"type": "image", "data": "base64...", "mimeType": "image/png"}` — standard MCP protocol, AI clients render natively
- Extend `mcp_protocol.cpp` with new `create_image_tool_result()` builder — reuses existing tool result framework
- No chunking or compression — TCP handles arbitrary JSON size, bridge relays to stdio, MCP clients handle large messages
- Image encoding: PNG (lossless, universal, MCP spec recommended mimeType)

### Godot Screenshot Implementation
- Viewport access: EditorInterface → SubViewport chain via `get_editor_viewport_2d()` / `get_editor_viewport_3d()` (research needed to verify exact API path)
- Image pipeline: `Viewport::get_texture()` → `ViewportTexture::get_image()` → `Image::save_png_to_buffer()` → base64 encode
- 2D/3D viewport distinction via EditorInterface API — intelligent detection of active editor viewport
- Empty/unavailable viewport returns `{"error": "Viewport not available"}` — e.g., 3D viewport in a pure 2D scene

### Claude's Discretion
- Exact EditorInterface API path to reach 2D/3D SubViewport (requires research of godot-cpp headers)
- Base64 encoding implementation (Godot's Marshalls::base64_encode or custom)
- Whether to include viewport metadata in response (resolution, scene name, camera position)
- Image scaling algorithm when width/height specified
- Error handling for edge cases (minimized viewport, viewport behind dock, etc.)

</decisions>

<canonical_refs>
## Canonical References

### Requirements & Roadmap
- `.planning/REQUIREMENTS.md` — Phase 9 maps to VWPT-01..03
- `.planning/ROADMAP.md` — Phase 9 success criteria

### Prior Phase Context
- `.planning/phases/06-scene-file-management/06-CONTEXT.md` — Latest tool design patterns
- `.planning/phases/08-animation-system/08-CONTEXT.md` — Most recent tool module pattern

### Existing Implementation
- `src/mcp_server.cpp` — MCPServer: handle_request dispatch, 34 current tools
- `src/mcp_protocol.h` / `src/mcp_protocol.cpp` — JSON-RPC builders — need new ImageContent builder
- `src/mcp_tool_registry.cpp` — 34 ToolDef entries
- `src/scene_file_tools.cpp` — EditorInterface usage patterns

</canonical_refs>

<code_context>
## Existing Code Insights

### Key Godot APIs (need research verification)
- `EditorInterface::get_editor_viewport_2d()` — may return SubViewport or Control (verify in headers)
- `EditorInterface::get_editor_viewport_3d()` — similar
- `SubViewport::get_texture()` → `ViewportTexture`
- `ViewportTexture::get_image()` → `Image`
- `Image::save_png_to_buffer()` → `PackedByteArray`
- `Marshalls::base64_encode()` or `Crypto` utilities for base64

### Integration Points
- `handle_request()`: add 1 new tool handler (capture_viewport)
- `mcp_tool_registry.cpp`: register 1 new ToolDef
- `mcp_protocol.cpp`: add `create_image_tool_result()` builder for ImageContent
- New `viewport_tools.h/.cpp` module

</code_context>

<deferred>
## Deferred Ideas

- Game viewport capture (running game) — Phase 10 scope
- Video/GIF capture — complex, not in v1.1
- Viewport streaming / real-time updates — needs MCP notification support
- Non-editor viewports — specialized use case
- Multiple viewport capture in one call — keep simple

</deferred>

---

*Phase: 09-editor-viewport-screenshots*
*Context gathered: 2026-03-18*
