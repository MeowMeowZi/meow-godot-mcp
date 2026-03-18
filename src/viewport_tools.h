#ifndef MEOW_GODOT_MCP_VIEWPORT_TOOLS_H
#define MEOW_GODOT_MCP_VIEWPORT_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// VWPT-01 + VWPT-02 + VWPT-03: Capture editor viewport screenshot
// Returns JSON with "data" (base64 PNG), "mimeType", and "metadata" on success,
// or "error" string on failure.
nlohmann::json capture_viewport(const std::string& viewport_type,
                                 int width, int height);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
#endif // MEOW_GODOT_MCP_VIEWPORT_TOOLS_H
