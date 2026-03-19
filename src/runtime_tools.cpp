#include "runtime_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// Track log file read position for incremental reads
static int64_t s_last_log_position = 0;

void reset_log_position() {
    godot::String log_path = "user://logs/godot.log";
    if (godot::FileAccess::file_exists(log_path)) {
        auto file = godot::FileAccess::open(log_path, godot::FileAccess::READ);
        if (file.is_valid()) {
            file->seek_end(0);
            s_last_log_position = file->get_position();
        }
    } else {
        s_last_log_position = 0;
    }
}

nlohmann::json run_game(const std::string& mode, const std::string& scene_path) {
    auto* ei = godot::EditorInterface::get_singleton();
    if (!ei) {
        return {{"success", false}, {"error", "EditorInterface not available"}};
    }

    // Check if already running
    if (ei->is_playing_scene()) {
        godot::String playing = ei->get_playing_scene();
        std::string playing_str;
        if (playing.length() > 0) {
            playing_str = std::string(playing.utf8().get_data());
        }
        return {{"success", true}, {"already_running", true},
                {"scene", playing_str}};
    }

    // Validate mode
    if (mode != "main" && mode != "current" && mode != "custom") {
        return {{"success", false}, {"error", "Invalid mode: '" + mode + "'. Must be 'main', 'current', or 'custom'"}};
    }

    // Validate scene_path for custom mode
    if (mode == "custom" && scene_path.empty()) {
        return {{"success", false}, {"error", "scene_path is required when mode is 'custom'"}};
    }

    // Reset log position to end of file before launching (legacy fallback)
    reset_log_position();

    // Launch game
    if (mode == "main") {
        ei->play_main_scene();
    } else if (mode == "current") {
        ei->play_current_scene();
    } else if (mode == "custom") {
        ei->play_custom_scene(godot::String(scene_path.c_str()));
    }

    nlohmann::json result = {
        {"success", true},
        {"running", true},
        {"mode", mode}
    };

    if (mode == "custom") {
        result["scene_path"] = scene_path;
    }

    return result;
}

nlohmann::json stop_game() {
    auto* ei = godot::EditorInterface::get_singleton();
    if (!ei) {
        return {{"success", false}, {"error", "EditorInterface not available"}};
    }

    if (!ei->is_playing_scene()) {
        return {{"success", false}, {"error", "Game is not currently running"}};
    }

    ei->stop_playing_scene();

    return {{"success", true}, {"stopped", true}};
}

nlohmann::json get_game_output(bool clear_after_read) {
    godot::String log_path = "user://logs/godot.log";

    if (!godot::FileAccess::file_exists(log_path)) {
        bool game_running = false;
        auto* ei = godot::EditorInterface::get_singleton();
        if (ei) {
            game_running = ei->is_playing_scene();
        }
        return {{"success", true}, {"lines", nlohmann::json::array()},
                {"count", 0}, {"game_running", game_running},
                {"message", "No log file found. Enable file logging in Project Settings."}};
    }

    auto file = godot::FileAccess::open(log_path, godot::FileAccess::READ);
    if (!file.is_valid()) {
        // File may be locked by running game process (common on Windows)
        bool game_running = false;
        auto* ei = godot::EditorInterface::get_singleton();
        if (ei) {
            game_running = ei->is_playing_scene();
        }
        return {{"success", true}, {"lines", nlohmann::json::array()},
                {"count", 0}, {"game_running", game_running},
                {"message", "Log file exists but cannot be opened (may be locked by running game)"}};
    }

    // Seek to last read position for incremental reads
    if (s_last_log_position > 0) {
        int64_t file_len = file->get_length();
        if (s_last_log_position > file_len) {
            // File was truncated/rotated, reset to beginning
            s_last_log_position = 0;
        }
        file->seek(s_last_log_position);
    }

    nlohmann::json lines = nlohmann::json::array();
    while (!file->eof_reached()) {
        godot::String line = file->get_line();
        if (line.length() > 0) {
            lines.push_back(std::string(line.utf8().get_data()));
        }
    }

    int64_t new_position = file->get_position();

    if (clear_after_read) {
        s_last_log_position = new_position;
    }

    // Check if game is currently running
    bool game_running = false;
    auto* ei = godot::EditorInterface::get_singleton();
    if (ei) {
        game_running = ei->is_playing_scene();
    }

    return {
        {"success", true},
        {"lines", lines},
        {"count", lines.size()},
        {"game_running", game_running}
    };
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
