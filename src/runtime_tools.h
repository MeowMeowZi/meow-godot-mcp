#ifndef MEOW_GODOT_MCP_RUNTIME_TOOLS_H
#define MEOW_GODOT_MCP_RUNTIME_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// Run the game in specified mode. Returns JSON with success, running status, mode.
// mode: "main" | "current" | "custom"
// scene_path: only used when mode == "custom"
nlohmann::json run_game(const std::string& mode, const std::string& scene_path = "");

// Stop the currently running game. Returns JSON with success status.
nlohmann::json stop_game();

// Get accumulated game output from log file since last read position.
// clear_after_read: if true, advance read position (default true)
nlohmann::json get_game_output(bool clear_after_read = true);

// Reset log read position to current end of file (call before run_game)
void reset_log_position();

#endif

#endif // MEOW_GODOT_MCP_RUNTIME_TOOLS_H
