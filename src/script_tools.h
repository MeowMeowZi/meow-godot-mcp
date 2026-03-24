#ifndef MEOW_GODOT_MCP_SCRIPT_TOOLS_H
#define MEOW_GODOT_MCP_SCRIPT_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Pure C++ helpers (testable without Godot)
struct EditResult {
    bool success;
    std::vector<std::string> lines;
    std::string error;
};

EditResult edit_lines(const std::vector<std::string>& lines, const std::string& operation,
                      int line, const std::string& content = "", int end_line = -1);

bool validate_res_path(const std::string& path, std::string& error_out);

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>

// Godot-dependent tool functions (called from mcp_server.cpp)
nlohmann::json read_script(const std::string& path);
nlohmann::json write_script(const std::string& path, const std::string& content);
nlohmann::json edit_script(const std::string& path, const std::string& operation,
                           int line, const std::string& content, int end_line);
nlohmann::json attach_script(const std::string& node_path, const std::string& script_path,
                              godot::EditorUndoRedoManager* undo_redo);
nlohmann::json detach_script(const std::string& node_path,
                              godot::EditorUndoRedoManager* undo_redo);
nlohmann::json validate_scripts();
#endif

#endif // MEOW_GODOT_MCP_SCRIPT_TOOLS_H
