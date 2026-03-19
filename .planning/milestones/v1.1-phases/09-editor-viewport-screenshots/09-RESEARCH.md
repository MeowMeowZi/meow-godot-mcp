# Phase 9: Editor Viewport Screenshots - Research

**Researched:** 2026-03-18
**Domain:** Godot Editor Viewport Capture, Image Encoding, MCP ImageContent Protocol
**Confidence:** HIGH

## Summary

Phase 9 enables AI clients to see the editor viewport by capturing screenshots of the 2D and 3D editor viewports and returning them as base64-encoded PNG images via MCP ImageContent. The implementation requires exactly one new tool (`capture_viewport`), a new protocol builder for ImageContent responses, and a new `viewport_tools` module following the established project pattern.

The research confirms a fully viable, straightforward pipeline: `EditorInterface::get_editor_viewport_2d/3d()` returns `SubViewport*`, then `SubViewport::get_texture()` returns `Ref<ViewportTexture>`, then `ViewportTexture::get_image()` (inherited from `Texture2D`) returns `Ref<Image>`, then `Image::save_png_to_buffer()` returns `PackedByteArray`, and finally `Marshalls::raw_to_base64()` converts bytes to base64 `String`. All APIs are verified in the godot-cpp v10 headers. The MCP ImageContent format is confirmed as `{"type": "image", "data": "<base64>", "mimeType": "image/png"}` in the content array. The key protocol change is a new `create_image_tool_result()` builder that returns ImageContent items instead of TextContent items, since the existing `create_tool_result()` hardcodes `type: "text"`.

**Primary recommendation:** Implement single `capture_viewport` tool with `viewport_type` parameter, new `create_image_tool_result()` protocol builder, and new `viewport_tools.h/.cpp` module -- totaling approximately 150-200 lines of new code.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Tool Architecture**: Single `capture_viewport` tool with `viewport_type` parameter ("2d" / "3d"). New `viewport_tools.h` / `viewport_tools.cpp` module.
- **Immediate capture**: Capture current frame (may be one frame stale but simple and reliable, avoids async complexity).
- **Optional width/height parameters**: For resolution control; defaults to current viewport resolution, scales if specified.
- **MCP ImageContent Protocol**: Return `{"type": "image", "data": "base64...", "mimeType": "image/png"}`.
- **New `create_image_tool_result()` builder** in `mcp_protocol.cpp` -- reuses existing tool result framework.
- **No chunking or compression** beyond PNG -- TCP handles arbitrary JSON size.
- **Image encoding**: PNG (lossless, universal, MCP spec recommended).
- **Viewport access**: `EditorInterface` -> SubViewport chain via `get_editor_viewport_2d()` / `get_editor_viewport_3d()`.
- **Image pipeline**: `Viewport::get_texture()` -> `ViewportTexture::get_image()` -> `Image::save_png_to_buffer()` -> base64.
- **Empty/unavailable viewport**: Returns `{"error": "Viewport not available"}`.

### Claude's Discretion
- Exact EditorInterface API path to reach 2D/3D SubViewport (RESOLVED: see Architecture Patterns below)
- Base64 encoding implementation (RESOLVED: use `Marshalls::raw_to_base64()`)
- Whether to include viewport metadata in response (resolution, scene name, camera position)
- Image scaling algorithm when width/height specified
- Error handling for edge cases (minimized viewport, viewport behind dock, etc.)

### Deferred Ideas (OUT OF SCOPE)
- Game viewport capture (running game) -- Phase 10 scope
- Video/GIF capture -- complex, not in v1.1
- Viewport streaming / real-time updates -- needs MCP notification support
- Non-editor viewports -- specialized use case
- Multiple viewport capture in one call -- keep simple
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| VWPT-01 | AI can capture editor 2D viewport screenshot | `EditorInterface::get_editor_viewport_2d()` -> `SubViewport*` confirmed in header. Full pipeline verified. |
| VWPT-02 | AI can capture editor 3D viewport screenshot | `EditorInterface::get_editor_viewport_3d(int32_t p_idx = 0)` -> `SubViewport*` confirmed in header. Idx=0 for primary viewport. |
| VWPT-03 | Screenshots returned as base64 PNG via MCP ImageContent | `Image::save_png_to_buffer()` + `Marshalls::raw_to_base64()` confirmed. MCP ImageContent format verified from spec 2025-03-26. |
</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| godot-cpp | v10+ (Godot 4.3+) | GDExtension C++ bindings | Project foundation |
| nlohmann/json | 3.12.0 | JSON construction for MCP protocol | Already used in all modules |

### Godot APIs Used (verified in godot-cpp headers)
| API | Header | Method Signature | Purpose |
|-----|--------|------------------|---------|
| `EditorInterface::get_editor_viewport_2d()` | `editor_interface.hpp:99` | `SubViewport* get_editor_viewport_2d() const` | Get 2D viewport SubViewport |
| `EditorInterface::get_editor_viewport_3d()` | `editor_interface.hpp:100` | `SubViewport* get_editor_viewport_3d(int32_t p_idx = 0) const` | Get 3D viewport SubViewport (idx 0 = primary) |
| `Viewport::get_texture()` | `viewport.hpp:234` | `Ref<ViewportTexture> get_texture() const` | Get viewport's rendered texture |
| `Texture2D::get_image()` | `texture2d.hpp:62` | `Ref<Image> get_image() const` | Download GPU texture to CPU Image |
| `Image::save_png_to_buffer()` | `image.hpp:179` | `PackedByteArray save_png_to_buffer() const` | Encode Image as PNG bytes |
| `Image::resize()` | `image.hpp:164` | `void resize(int32_t w, int32_t h, Interpolation interp)` | Scale image for width/height params |
| `Image::get_width()` / `get_height()` | `image.hpp:153-154` | `int32_t get_width/get_height() const` | Read image dimensions for metadata |
| `Image::is_empty()` | `image.hpp:175` | `bool is_empty() const` | Check for empty/invalid image |
| `SubViewport::get_size()` | `sub_viewport.hpp:63` | `Vector2i get_size() const` | Read viewport resolution |
| `Marshalls::raw_to_base64()` | `marshalls.hpp:56` | `String raw_to_base64(const PackedByteArray& array)` | Convert PNG bytes to base64 string |
| `Marshalls::get_singleton()` | `marshalls.hpp:52` | `static Marshalls* get_singleton()` | Access Marshalls singleton |

**No new dependencies required.** All APIs are available in the existing godot-cpp v10 bindings.

## Architecture Patterns

### Recommended Project Structure (new files only)
```
src/
├── viewport_tools.h        # Function declarations (VWPT-01, VWPT-02, VWPT-03)
├── viewport_tools.cpp      # Viewport capture implementation
├── mcp_protocol.h          # + create_image_tool_result() declaration
├── mcp_protocol.cpp        # + create_image_tool_result() implementation
├── mcp_tool_registry.cpp   # + 1 new ToolDef (capture_viewport)
└── mcp_server.cpp          # + 1 new tool dispatch block
```

### Pattern 1: Complete Capture Pipeline (VERIFIED)
**What:** The end-to-end pipeline from viewport to base64 string.
**When to use:** Every call to `capture_viewport`.
**Example:**
```cpp
// Source: godot-cpp headers verified 2026-03-18
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/marshalls.hpp>

// Step 1: Get SubViewport
SubViewport* viewport = EditorInterface::get_singleton()->get_editor_viewport_2d();
// or: EditorInterface::get_singleton()->get_editor_viewport_3d(0);

if (!viewport) {
    return {{"error", "Viewport not available"}};
}

// Step 2: Get texture -> Image
Ref<ViewportTexture> tex = viewport->get_texture();
if (!tex.is_valid()) {
    return {{"error", "Viewport texture not available"}};
}

Ref<Image> image = tex->get_image();
if (!image.is_valid() || image->is_empty()) {
    return {{"error", "Viewport image is empty"}};
}

// Step 3: Optional resize
if (width > 0 && height > 0) {
    image->resize(width, height, Image::INTERPOLATE_LANCZOS);
}

// Step 4: Encode PNG -> base64
PackedByteArray png_data = image->save_png_to_buffer();
if (png_data.size() == 0) {
    return {{"error", "PNG encoding failed"}};
}

String base64_str = Marshalls::get_singleton()->raw_to_base64(png_data);
std::string base64_cpp(base64_str.utf8().get_data());
```

### Pattern 2: MCP ImageContent Tool Result Builder
**What:** New protocol builder that returns image data instead of text.
**When to use:** For the `capture_viewport` tool result only.
**Example:**
```cpp
// New function in mcp_protocol.h/cpp
nlohmann::json create_image_tool_result(
    const nlohmann::json& id,
    const std::string& base64_data,
    const std::string& mime_type,
    const nlohmann::json& metadata = nlohmann::json())
{
    nlohmann::json content_array = nlohmann::json::array();

    // Primary: ImageContent
    content_array.push_back({
        {"type", "image"},
        {"data", base64_data},
        {"mimeType", mime_type}
    });

    // Optional: TextContent with metadata
    if (!metadata.is_null() && !metadata.empty()) {
        content_array.push_back({
            {"type", "text"},
            {"text", metadata.dump()}
        });
    }

    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", {
            {"content", content_array},
            {"isError", false}
        }}
    };
}
```

### Pattern 3: Tool Module Pattern (follows animation_tools/ui_tools)
**What:** Standard module structure with `#ifdef MEOW_GODOT_MCP_GODOT_ENABLED` guard.
**Example header:**
```cpp
#ifndef MEOW_GODOT_MCP_VIEWPORT_TOOLS_H
#define MEOW_GODOT_MCP_VIEWPORT_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// VWPT-01 + VWPT-02 + VWPT-03: Capture editor viewport screenshot
nlohmann::json capture_viewport(const std::string& viewport_type,
                                 int width, int height);

#endif
#endif
```

### Pattern 4: Tool Dispatch in mcp_server.cpp
**What:** Standard dispatch block following Phase 8 pattern.
**Example:**
```cpp
// In handle_request(), after animation tools block:
if (tool_name == "capture_viewport") {
    std::string viewport_type = "2d";
    int width = 0, height = 0;
    if (params.contains("arguments") && params["arguments"].is_object()) {
        auto& args = params["arguments"];
        if (args.contains("viewport_type") && args["viewport_type"].is_string())
            viewport_type = args["viewport_type"].get<std::string>();
        if (args.contains("width") && args["width"].is_number_integer())
            width = args["width"].get<int>();
        if (args.contains("height") && args["height"].is_number_integer())
            height = args["height"].get<int>();
    }
    // NOTE: capture_viewport returns special format -- use create_image_tool_result
    auto result = capture_viewport(viewport_type, width, height);
    if (result.contains("error")) {
        return mcp::create_tool_result(id, result);  // TextContent error
    }
    // Extract base64 and metadata, use image builder
    return mcp::create_image_tool_result(id,
        result["data"].get<std::string>(),
        result["mimeType"].get<std::string>(),
        result.value("metadata", nlohmann::json()));
}
```

### Anti-Patterns to Avoid
- **Saving to file then reading back:** Do NOT use `Image::save_png()` to write a temp file then read it. Use `save_png_to_buffer()` which returns in-memory bytes directly.
- **Custom base64 implementation:** Do NOT hand-roll base64 encoding. Godot's `Marshalls::raw_to_base64()` is tested, available, and correct.
- **Async viewport capture:** Do NOT try to defer to next frame for "fresh" content. The one-frame-stale capture on main thread is simpler and avoids callback/promise complexity. The viewport is always being rendered by the editor.
- **Separate tools for 2D/3D:** Per locked decision, use single tool with `viewport_type` parameter.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Base64 encoding | Custom base64 encoder | `Marshalls::raw_to_base64()` | Godot built-in, proven, handles padding correctly |
| PNG encoding | Custom PNG writer | `Image::save_png_to_buffer()` | Godot built-in, uses libpng internally |
| Image scaling | Manual pixel manipulation | `Image::resize()` with `INTERPOLATE_LANCZOS` | Godot's resize handles format conversion, interpolation modes |
| Viewport access | Manual node tree traversal | `EditorInterface::get_editor_viewport_2d/3d()` | Direct API, returns correct SubViewport |

**Key insight:** The entire pipeline from viewport capture to base64 output uses exclusively Godot built-in APIs. No third-party libraries or custom implementations are needed.

## Common Pitfalls

### Pitfall 1: get_image() Returns Null or Empty
**What goes wrong:** `Texture2D::get_image()` can return an empty/null `Ref<Image>` if the viewport hasn't rendered yet or the rendering backend hasn't flushed.
**Why it happens:** On the very first frame after editor startup, or if the viewport tab is not visible (e.g., user is on Script tab), the viewport texture may not have content.
**How to avoid:** Check `image.is_valid()` AND `!image->is_empty()` before proceeding. Return a descriptive error if either fails.
**Warning signs:** Empty PNG buffer (size 0), crash on null dereference.

### Pitfall 2: 3D Viewport Index
**What goes wrong:** `get_editor_viewport_3d(int32_t p_idx = 0)` takes an index parameter because Godot supports up to 4 3D viewports (quad-split). Passing no index or 0 gives the primary viewport.
**Why it happens:** Users might have quad-viewport layout enabled.
**How to avoid:** Always use index 0 for the primary 3D viewport. Document this behavior -- captures the main/first 3D viewport.
**Warning signs:** Wrong viewport captured in quad-split mode.

### Pitfall 3: Threading -- Must Run on Main Thread
**What goes wrong:** `get_texture()` and `get_image()` involve rendering server calls that MUST happen on the main thread.
**Why it happens:** The viewport capture tool is dispatched via `handle_request()` which runs on the main thread (via `poll()`), so this is already safe. But if someone tried to optimize by running on IO thread, it would crash.
**How to avoid:** Keep capture_viewport execution in `handle_request()` which is already main-thread-only. Do NOT try to capture on IO thread.
**Warning signs:** Random crashes, corrupted images.

### Pitfall 4: Large Base64 Strings in JSON
**What goes wrong:** A 1920x1080 viewport screenshot as PNG is roughly 1-5 MB, which becomes ~1.3-6.7 MB of base64. The resulting JSON-RPC response could be 7+ MB.
**Why it happens:** Viewport screenshots are inherently large compared to text tool results.
**How to avoid:** Per locked decision, no chunking needed -- TCP and bridge handle arbitrary message sizes. But be aware that `nlohmann::json::dump()` for large strings is fine. The bridge's `put_data()` sends the full buffer atomically.
**Warning signs:** Slow response times (acceptable), client timeout (adjust if needed).

### Pitfall 5: Image Format Conversion
**What goes wrong:** `get_image()` may return images in formats other than RGBA8 (e.g., RGBAH for HDR viewports). `save_png_to_buffer()` handles most formats but may produce unexpected results with exotic formats.
**Why it happens:** Editor viewports may use HDR rendering.
**How to avoid:** If needed, call `image->convert(Image::FORMAT_RGBA8)` before `save_png_to_buffer()` to ensure compatibility. However, `save_png_to_buffer()` internally handles format conversion, so this is a defensive measure only if issues arise.
**Warning signs:** Corrupted or all-black PNG output.

### Pitfall 6: create_tool_result vs create_image_tool_result
**What goes wrong:** Using the existing `create_tool_result()` for viewport screenshots would wrap the base64 data inside a TextContent JSON, double-encoding it.
**Why it happens:** `create_tool_result()` puts `content_data.dump()` inside `{"type": "text", "text": ...}`. For image data, the content array needs `{"type": "image", "data": ..., "mimeType": ...}` directly.
**How to avoid:** Create a new `create_image_tool_result()` builder that constructs ImageContent items. The `capture_viewport` dispatch in `mcp_server.cpp` must use this new builder, not the existing one.
**Warning signs:** AI client shows base64 text instead of rendering the image.

## Code Examples

Verified patterns from godot-cpp headers:

### Complete capture_viewport Function
```cpp
// Source: godot-cpp headers (editor_interface.hpp, viewport.hpp, texture2d.hpp, image.hpp, marshalls.hpp)
nlohmann::json capture_viewport(const std::string& viewport_type,
                                 int width, int height) {
    // Validate viewport_type
    SubViewport* viewport = nullptr;
    if (viewport_type == "2d") {
        viewport = EditorInterface::get_singleton()->get_editor_viewport_2d();
    } else if (viewport_type == "3d") {
        viewport = EditorInterface::get_singleton()->get_editor_viewport_3d(0);
    } else {
        return {{"error", "Invalid viewport_type: " + viewport_type +
                          ". Valid: 2d, 3d"}};
    }

    if (!viewport) {
        return {{"error", "Viewport not available for type: " + viewport_type}};
    }

    // Get texture
    Ref<ViewportTexture> tex = viewport->get_texture();
    if (!tex.is_valid()) {
        return {{"error", "Viewport texture not available"}};
    }

    // Get image (CPU download from GPU)
    Ref<Image> image = tex->get_image();
    if (!image.is_valid() || image->is_empty()) {
        return {{"error", "Viewport image is empty (viewport may not be visible)"}};
    }

    // Record original dimensions for metadata
    int orig_width = image->get_width();
    int orig_height = image->get_height();

    // Optional resize
    if (width > 0 && height > 0) {
        image->resize(width, height, Image::INTERPOLATE_LANCZOS);
    } else if (width > 0 || height > 0) {
        // If only one dimension specified, scale proportionally
        if (width > 0) {
            double scale = (double)width / orig_width;
            height = (int)(orig_height * scale);
        } else {
            double scale = (double)height / orig_height;
            width = (int)(orig_width * scale);
        }
        image->resize(width, height, Image::INTERPOLATE_LANCZOS);
    }

    // Encode as PNG
    PackedByteArray png_data = image->save_png_to_buffer();
    if (png_data.size() == 0) {
        return {{"error", "PNG encoding failed"}};
    }

    // Base64 encode
    String base64_godot = Marshalls::get_singleton()->raw_to_base64(png_data);
    std::string base64_str(base64_godot.utf8().get_data());

    // Build result with metadata
    nlohmann::json metadata = {
        {"viewport_type", viewport_type},
        {"width", image->get_width()},
        {"height", image->get_height()},
        {"original_width", orig_width},
        {"original_height", orig_height}
    };

    return {
        {"data", base64_str},
        {"mimeType", "image/png"},
        {"metadata", metadata}
    };
}
```

### MCP ImageContent Response (complete JSON-RPC)
```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "content": [
      {
        "type": "image",
        "data": "iVBORw0KGgoAAAANSUhEUg...",
        "mimeType": "image/png"
      },
      {
        "type": "text",
        "text": "{\"viewport_type\":\"2d\",\"width\":1920,\"height\":1080}"
      }
    ],
    "isError": false
  }
}
```

### ToolDef Registration
```cpp
// In mcp_tool_registry.cpp, add to tools vector:
{
    "capture_viewport",
    "Capture a screenshot of the editor 2D or 3D viewport. Returns the image as base64-encoded PNG via MCP ImageContent. The AI client renders the image natively.",
    {
        {"type", "object"},
        {"properties", {
            {"viewport_type", {
                {"type", "string"},
                {"enum", {"2d", "3d"}},
                {"description", "Which viewport to capture: 2d or 3d (default: 2d)"}
            }},
            {"width", {
                {"type", "integer"},
                {"description", "Optional output width in pixels. Scales the image. Omit for original viewport resolution."}
            }},
            {"height", {
                {"type", "integer"},
                {"description", "Optional output height in pixels. Scales the image. Omit for original viewport resolution."}
            }}
        }},
        {"required", nlohmann::json::array()}
    },
    {4, 3, 0}
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| No viewport capture API in GDExtension | `EditorInterface::get_editor_viewport_2d/3d()` available since Godot 4.2+ | Godot 4.2 (2023) | Enables direct SubViewport access from plugins |
| MCP text-only responses | MCP ImageContent type in spec 2025-03-26 | 2025-03-26 | AI clients can render images natively |
| Separate text+image protocols | Single content array with mixed types | MCP spec 2025-03-26 | Can return image + metadata in one response |

**Deprecated/outdated:**
- Nothing in this domain is deprecated. All APIs are current for Godot 4.3+ and MCP spec 2025-03-26.

## Open Questions

1. **Viewport metadata in response**
   - What we know: We can include width, height, viewport_type as TextContent alongside the ImageContent in the content array.
   - What's unclear: Whether AI clients parse/display the text alongside the image, or if metadata adds noise.
   - Recommendation: Include basic metadata (viewport_type, width, height) as a second TextContent item. This provides context for the AI model about what it's looking at, and doesn't affect image rendering. Minimal cost.

2. **Image scaling interpolation**
   - What we know: Godot offers NEAREST, BILINEAR, CUBIC, TRILINEAR, LANCZOS.
   - What's unclear: Which gives best results for screenshots of editor content (mix of sharp edges, text, and gradients).
   - Recommendation: Use `INTERPOLATE_LANCZOS` for best quality downscaling (sharpest for UI/text content). This is the highest-quality option available.

3. **Proportional scaling when only width OR height specified**
   - What we know: The decision locks optional width/height parameters.
   - What's unclear: Whether specifying only one dimension should error or auto-calculate the other.
   - Recommendation: Auto-calculate the missing dimension proportionally. This is the intuitive behavior.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Python UAT scripts (socket-based TCP client) |
| Config file | none -- standalone scripts |
| Quick run command | `python tests/uat_phase9.py` |
| Full suite command | `python tests/uat_phase9.py` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| VWPT-01 | Capture 2D viewport screenshot | integration | `python tests/uat_phase9.py` | No -- Wave 0 |
| VWPT-02 | Capture 3D viewport screenshot | integration | `python tests/uat_phase9.py` | No -- Wave 0 |
| VWPT-03 | Response is MCP ImageContent with base64 PNG | integration | `python tests/uat_phase9.py` | No -- Wave 0 |
| PROTO | create_image_tool_result builder correctness | unit | `ctest --test-dir tests/build -R protocol` | Partial -- extend test_protocol.cpp |

### Expected UAT Tests (10 tests)
1. `tools/list` shows 35 tools including `capture_viewport` (tool count up from 34)
2. `capture_viewport` with `viewport_type: "2d"` returns ImageContent (VWPT-01)
3. `capture_viewport` with `viewport_type: "3d"` returns ImageContent (VWPT-02)
4. Response has `type: "image"` content item with non-empty `data` field (VWPT-03)
5. Response has `mimeType: "image/png"` (VWPT-03)
6. Base64 data decodes to valid PNG (check PNG signature bytes `\x89PNG`)
7. Response includes metadata TextContent with width/height
8. `capture_viewport` with `width`/`height` returns scaled image
9. `capture_viewport` with invalid `viewport_type` returns error
10. `capture_viewport` default (no args) captures 2D viewport

### Sampling Rate
- **Per task commit:** `python tests/uat_phase9.py` (quick, ~10 seconds)
- **Per wave merge:** Full suite -- `python tests/uat_phase9.py`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/uat_phase9.py` -- covers VWPT-01, VWPT-02, VWPT-03
- [ ] `tests/test_protocol.cpp` -- extend with `create_image_tool_result` unit test

## Sources

### Primary (HIGH confidence)
- `godot-cpp/gen/include/godot_cpp/classes/editor_interface.hpp` -- Lines 99-100: `get_editor_viewport_2d()` and `get_editor_viewport_3d()` signatures verified
- `godot-cpp/gen/include/godot_cpp/classes/viewport.hpp` -- Line 234: `get_texture()` returns `Ref<ViewportTexture>`
- `godot-cpp/gen/include/godot_cpp/classes/texture2d.hpp` -- Line 62: `get_image()` returns `Ref<Image>`
- `godot-cpp/gen/include/godot_cpp/classes/image.hpp` -- Line 179: `save_png_to_buffer()` returns `PackedByteArray`
- `godot-cpp/gen/include/godot_cpp/classes/marshalls.hpp` -- Line 56: `raw_to_base64()` returns `String`
- `godot-cpp/gen/include/godot_cpp/classes/sub_viewport.hpp` -- SubViewport inherits Viewport, has `get_size()`
- MCP Specification 2025-03-26, Server Tools: ImageContent format `{"type": "image", "data": "base64...", "mimeType": "image/png"}`
  - URL: https://modelcontextprotocol.io/specification/2025-03-26/server/tools

### Secondary (MEDIUM confidence)
- Existing project source: `mcp_protocol.cpp`, `mcp_server.cpp`, `animation_tools.cpp` -- established patterns for tool modules, dispatch, and protocol builders

### Tertiary (LOW confidence)
- None -- all critical findings verified from primary sources

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- All APIs verified in godot-cpp headers, no uncertainty
- Architecture: HIGH -- Follows established project patterns exactly, all APIs confirmed
- Pitfalls: HIGH -- Based on direct API analysis and Godot rendering behavior knowledge
- MCP ImageContent format: HIGH -- Verified from official spec page

**Research date:** 2026-03-18
**Valid until:** 2026-04-18 (stable domain -- Godot 4.3 APIs are frozen, MCP spec is versioned)
