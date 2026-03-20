#include "runtime_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <fstream>
#include <sstream>

// Track log file read position for incremental reads
static int64_t s_last_log_position = 0;

// Get the game's log file path using OS::get_user_data_dir()
// This resolves correctly even when called from the editor process
static godot::String get_game_log_path() {
    auto* os = godot::OS::get_singleton();
    if (os) {
        return os->get_user_data_dir().path_join("logs/godot.log");
    }
    return "user://logs/godot.log";
}

void reset_log_position() {
    godot::String log_path = get_game_log_path();
    std::string path_str(log_path.utf8().get_data());
    std::ifstream file(path_str, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        s_last_log_position = static_cast<int64_t>(file.tellg());
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

    // Auto-enable file logging so get_game_output works without manual setup (GOUT-01/03)
    auto* ps = godot::ProjectSettings::get_singleton();
    if (ps) {
        bool logging_enabled = ps->get_setting("debug/file_logging/enable_file_logging");
        if (!logging_enabled) {
            ps->set_setting("debug/file_logging/enable_file_logging", true);
            ps->save();
        }
    }

    // Reset log position to end of file before launching
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
    godot::String log_path = get_game_log_path();

    std::string log_path_str(log_path.utf8().get_data());

    // Use std::ifstream for shared read access (Windows locks files opened by game process)
    std::ifstream file(log_path_str, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        bool game_running = false;
        auto* ei = godot::EditorInterface::get_singleton();
        if (ei) {
            game_running = ei->is_playing_scene();
        }
        return {{"success", true}, {"lines", nlohmann::json::array()},
                {"count", 0}, {"game_running", game_running},
                {"message", "No log file found at: " + log_path_str}};
    }

    // Seek to last read position for incremental reads
    if (s_last_log_position > 0) {
        file.seekg(0, std::ios::end);
        auto file_len = file.tellg();
        if (s_last_log_position > file_len) {
            s_last_log_position = 0;
        }
        file.seekg(s_last_log_position);
    }

    nlohmann::json lines = nlohmann::json::array();
    std::string line;
    while (std::getline(file, line)) {
        // Remove trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    int64_t new_position = static_cast<int64_t>(file.tellg());
    if (new_position < 0) {
        // EOF reached, get actual position
        file.clear();
        file.seekg(0, std::ios::end);
        new_position = static_cast<int64_t>(file.tellg());
    }

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
