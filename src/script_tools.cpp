#include "script_tools.h"
#include <sstream>
#include <algorithm>

// --- Pure C++ helpers (no Godot dependency) ---

bool validate_res_path(const std::string& path, std::string& error_out) {
    if (path.empty()) {
        error_out = "Path is empty";
        return false;
    }
    if (path.size() < 6 || path.substr(0, 6) != "res://") {
        error_out = "Path must start with res:// (got: " + path + ")";
        return false;
    }
    error_out.clear();
    return true;
}

EditResult edit_lines(const std::vector<std::string>& lines, const std::string& operation,
                      int line, const std::string& content, int end_line) {
    EditResult result;
    int size = static_cast<int>(lines.size());

    // Validate line number (1-based)
    if (line <= 0) {
        result.success = false;
        result.error = "Line number must be >= 1 (got: " + std::to_string(line) + ")";
        return result;
    }

    if (operation == "insert") {
        // Insert before the given line position (1-based)
        // line 1 = prepend, line size+1 = append, beyond that = error
        if (line > size + 1) {
            result.success = false;
            result.error = "Insert line " + std::to_string(line) + " is beyond file end + 1 (file has " + std::to_string(size) + " lines)";
            return result;
        }
        result.lines = lines;
        result.lines.insert(result.lines.begin() + (line - 1), content);
        result.success = true;
        return result;
    }

    if (operation == "replace") {
        int end = (end_line == -1) ? line : end_line;
        if (line > size) {
            result.success = false;
            result.error = "Replace line " + std::to_string(line) + " is out of range (file has " + std::to_string(size) + " lines)";
            return result;
        }
        if (end > size) {
            result.success = false;
            result.error = "Replace end_line " + std::to_string(end) + " is out of range (file has " + std::to_string(size) + " lines)";
            return result;
        }
        if (end < line) {
            result.success = false;
            result.error = "end_line (" + std::to_string(end) + ") must be >= line (" + std::to_string(line) + ")";
            return result;
        }
        result.lines = lines;
        // Erase the range [line-1, end) and insert the replacement content
        result.lines.erase(result.lines.begin() + (line - 1), result.lines.begin() + end);
        result.lines.insert(result.lines.begin() + (line - 1), content);
        result.success = true;
        return result;
    }

    if (operation == "delete") {
        int end = (end_line == -1) ? line : end_line;
        if (line > size) {
            result.success = false;
            result.error = "Delete line " + std::to_string(line) + " is out of range (file has " + std::to_string(size) + " lines)";
            return result;
        }
        if (end > size) {
            result.success = false;
            result.error = "Delete end_line " + std::to_string(end) + " is out of range (file has " + std::to_string(size) + " lines)";
            return result;
        }
        if (end < line) {
            result.success = false;
            result.error = "end_line (" + std::to_string(end) + ") must be >= line (" + std::to_string(line) + ")";
            return result;
        }
        result.lines = lines;
        result.lines.erase(result.lines.begin() + (line - 1), result.lines.begin() + end);
        result.success = true;
        return result;
    }

    result.success = false;
    result.error = "Unknown operation: " + operation + " (expected: insert, replace, delete)";
    return result;
}

// --- Godot-dependent implementations ---

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Helper: split a Godot String into lines (preserving empty trailing line)
static std::vector<std::string> split_lines(const String& text) {
    std::vector<std::string> lines;
    std::string content = std::string(text.utf8().get_data());
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

// Helper: join lines back into a single string with newlines
static String join_lines(const std::vector<std::string>& lines) {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i + 1 < lines.size()) {
            result += "\n";
        }
    }
    // Ensure trailing newline for GDScript files
    if (!result.empty() && result.back() != '\n') {
        result += "\n";
    }
    return String(result.c_str());
}

nlohmann::json read_script(const std::string& path) {
    std::string path_error;
    if (!validate_res_path(path, path_error)) {
        return {{"error", path_error}};
    }

    String gd_path = String(path.c_str());
    if (!FileAccess::file_exists(gd_path)) {
        return {{"error", "File not found: " + path}};
    }

    Ref<FileAccess> file = FileAccess::open(gd_path, FileAccess::READ);
    if (!file.is_valid()) {
        return {{"error", "Failed to open file: " + path}};
    }

    String content = file->get_as_text();
    std::string content_str = std::string(content.utf8().get_data());

    // Count lines
    int line_count = 0;
    if (!content_str.empty()) {
        line_count = 1;
        for (char c : content_str) {
            if (c == '\n') line_count++;
        }
        // If file ends with newline, don't count the empty line after it
        if (content_str.back() == '\n') {
            line_count--;
        }
    }

    return {{"success", true}, {"path", path}, {"content", content_str}, {"line_count", line_count}};
}

nlohmann::json write_script(const std::string& path, const std::string& content) {
    std::string path_error;
    if (!validate_res_path(path, path_error)) {
        return {{"error", path_error}};
    }

    String gd_path = String(path.c_str());

    // Error if file already exists (per user decision -- use edit_script to modify)
    if (FileAccess::file_exists(gd_path)) {
        return {{"error", "File already exists: " + path + ". Use edit_script to modify existing files."}};
    }

    Ref<FileAccess> file = FileAccess::open(gd_path, FileAccess::WRITE);
    if (!file.is_valid()) {
        return {{"error", "Failed to create file: " + path}};
    }

    file->store_string(String(content.c_str()));
    file.unref();  // Close file before updating filesystem

    // NOTE: EditorFileSystem::update_file() is intentionally NOT called here.
    // Calling it (even deferred) causes crashes when UndoRedo operations
    // (create_node, attach_script) run shortly after. The file is written
    // to disk; the editor will pick it up on next filesystem scan.

    return {{"success", true}, {"path", path}};
}

nlohmann::json edit_script(const std::string& path, const std::string& operation,
                           int line, const std::string& content, int end_line) {
    std::string path_error;
    if (!validate_res_path(path, path_error)) {
        return {{"error", path_error}};
    }

    String gd_path = String(path.c_str());
    if (!FileAccess::file_exists(gd_path)) {
        return {{"error", "File not found: " + path}};
    }

    // Read current content
    Ref<FileAccess> read_file = FileAccess::open(gd_path, FileAccess::READ);
    if (!read_file.is_valid()) {
        return {{"error", "Failed to open file: " + path}};
    }
    String file_content = read_file->get_as_text();
    read_file.unref();

    // Split into lines and apply edit
    std::vector<std::string> lines = split_lines(file_content);
    EditResult result = edit_lines(lines, operation, line, content, end_line);

    if (!result.success) {
        return {{"error", result.error}};
    }

    // Write back
    String new_content = join_lines(result.lines);
    Ref<FileAccess> write_file = FileAccess::open(gd_path, FileAccess::WRITE);
    if (!write_file.is_valid()) {
        return {{"error", "Failed to write file: " + path}};
    }
    write_file->store_string(new_content);
    write_file.unref();

    // NOTE: EditorFileSystem::update_file() intentionally omitted (see write_script).

    return {{"success", true}, {"path", path}, {"line_count", static_cast<int>(result.lines.size())}};
}

nlohmann::json attach_script(const std::string& node_path, const std::string& script_path,
                              godot::EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Find target node
    Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    if (!node) {
        return {{"error", "Node not found: " + node_path}};
    }

    // Validate script path
    std::string path_error;
    if (!validate_res_path(script_path, path_error)) {
        return {{"error", path_error}};
    }

    String gd_script_path = String(script_path.c_str());

    // Verify file exists on disk
    if (!FileAccess::file_exists(gd_script_path)) {
        return {{"error", "Script file not found: " + script_path}};
    }

    // Build GDScript manually instead of ResourceLoader::load() to avoid
    // crashes when loading .gd files just created by write_script.
    // ResourceLoader triggers editor internals that crash on unregistered files.
    Ref<FileAccess> script_file = FileAccess::open(gd_script_path, FileAccess::READ);
    if (!script_file.is_valid()) {
        return {{"error", "Failed to read script file: " + script_path}};
    }
    String source = script_file->get_as_text();
    script_file.unref();

    Ref<GDScript> script;
    script.instantiate();
    script->set_source_code(source);
    script->set_path(gd_script_path);
    script->reload();

    if (!script.is_valid()) {
        return {{"error", "Failed to create script: " + script_path}};
    }

    // Save current script for undo
    Variant old_script = node->get_script();

    // Build UndoRedo action
    undo_redo->create_action(godot::String("MCP: Attach script"));
    undo_redo->add_do_method(node, "set_script", script);
    undo_redo->add_undo_method(node, "set_script", old_script);
    undo_redo->commit_action();

    return {{"success", true}, {"node_path", node_path}, {"script_path", script_path}};
}

nlohmann::json detach_script(const std::string& node_path,
                              godot::EditorUndoRedoManager* undo_redo) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {{"error", "No scene open"}};
    }

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    // Find target node
    Node* node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    if (!node) {
        return {{"error", "Node not found: " + node_path}};
    }

    // Check node has a script
    Variant old_script = node->get_script();
    if (old_script.get_type() == Variant::NIL) {
        return {{"error", "Node has no script attached: " + node_path}};
    }

    // Build UndoRedo action
    undo_redo->create_action(godot::String("MCP: Detach script"));
    undo_redo->add_do_method(node, "set_script", Variant());
    undo_redo->add_undo_method(node, "set_script", old_script);
    undo_redo->commit_action();

    return {{"success", true}, {"node_path", node_path}};
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
