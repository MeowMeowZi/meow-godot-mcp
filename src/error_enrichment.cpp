#include "error_enrichment.h"

#include <algorithm>
#include <unordered_map>

// --- Levenshtein distance (space-optimized single-row) ---

int levenshtein_distance(const std::string& a, const std::string& b) {
    if (a.size() < b.size()) return levenshtein_distance(b, a);
    std::vector<int> prev(b.size() + 1), curr(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= a.size(); ++i) {
        curr[0] = static_cast<int>(i);
        for (size_t j = 1; j <= b.size(); ++j) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            curr[j] = std::min({curr[j-1] + 1, prev[j] + 1, prev[j-1] + cost});
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}

// --- Error categorization by string prefix matching ---

ErrorCategory categorize_error(const std::string& error_msg, const std::string& /*tool_name*/) {
    // NODE_NOT_FOUND patterns
    if (error_msg.find("Node not found: ") == 0 ||
        error_msg.find("Parent not found: ") == 0 ||
        error_msg.find("Source node not found: ") == 0 ||
        error_msg.find("Target node not found: ") == 0 ||
        error_msg.find("Node not found at path: ") == 0) {
        return ErrorCategory::NODE_NOT_FOUND;
    }

    // NO_SCENE_OPEN
    if (error_msg.find("No scene open") != std::string::npos) {
        return ErrorCategory::NO_SCENE_OPEN;
    }

    // GAME_NOT_RUNNING patterns
    if (error_msg == "Game bridge not initialized" ||
        error_msg == "No game running or bridge not connected" ||
        error_msg == "Game is not currently running") {
        return ErrorCategory::GAME_NOT_RUNNING;
    }

    // UNKNOWN_CLASS patterns
    if (error_msg.find("Unknown class: ") == 0) {
        return ErrorCategory::UNKNOWN_CLASS;
    }
    // " is not a Node type" suffix pattern
    if (error_msg.size() > 19 && error_msg.find(" is not a Node type") != std::string::npos) {
        return ErrorCategory::UNKNOWN_CLASS;
    }

    // TYPE_MISMATCH patterns (various "Unknown X: " prefixes)
    if (error_msg.find("Unknown preset: ") == 0 ||
        error_msg.find("Unknown alignment: ") == 0 ||
        error_msg.find("Unknown size flag: ") == 0 ||
        error_msg.find("Unknown track type: ") == 0 ||
        error_msg.find("Unknown loop_mode: ") == 0 ||
        error_msg.find("Unknown action: ") == 0 ||
        error_msg.find("Unknown input type: ") == 0 ||
        error_msg.find("Unknown mouse_action: ") == 0) {
        return ErrorCategory::TYPE_MISMATCH;
    }

    // SCRIPT_ERROR patterns
    if (error_msg.find("File not found: ") == 0 ||
        error_msg.find("Script file not found: ") == 0 ||
        error_msg.find("Path must start with res://") == 0) {
        return ErrorCategory::SCRIPT_ERROR;
    }

    // RESOURCE_ERROR patterns
    if (error_msg.find("Resource not found: ") == 0 ||
        error_msg.find("Failed to load resource: ") == 0) {
        return ErrorCategory::RESOURCE_ERROR;
    }

    // DEFERRED_PENDING
    if (error_msg == "Another deferred request is already pending") {
        return ErrorCategory::DEFERRED_PENDING;
    }

    return ErrorCategory::GENERIC;
}

// --- Type format hints table (ERR-06) ---

static const std::unordered_map<std::string, std::string> TYPE_FORMAT_HINTS = {
    {"Vector2",     "Vector2(x, y) e.g. Vector2(100, 200)"},
    {"Vector3",     "Vector3(x, y, z) e.g. Vector3(0, 1, 0)"},
    {"Color",       "Color(r, g, b, a) with 0-1 floats, e.g. Color(1, 0, 0, 1) or #ff0000"},
    {"Rect2",       "Rect2(x, y, width, height) e.g. Rect2(0, 0, 100, 50)"},
    {"Transform2D", "Transform2D(rotation, position) e.g. Transform2D(0, Vector2(100, 200))"},
    {"NodePath",    "Relative path from scene root e.g. Player/Sprite2D"},
    {"StringName",  "Simple string e.g. idle"},
    {"float",       "Decimal number e.g. 3.14"},
    {"int",         "Integer number e.g. 42"},
    {"bool",        "true or false"},
};

// --- Helper: extract target name from node path ---

static std::string extract_target_name(const std::string& error_msg) {
    // Try each prefix pattern
    const char* prefixes[] = {
        "Node not found: ",
        "Parent not found: ",
        "Source node not found: ",
        "Target node not found: ",
        "Node not found at path: "
    };
    std::string path;
    for (const auto* prefix : prefixes) {
        std::string p(prefix);
        if (error_msg.find(p) == 0) {
            path = error_msg.substr(p.size());
            break;
        }
    }
    if (path.empty()) return "";
    auto slash_pos = path.rfind('/');
    return (slash_pos != std::string::npos) ? path.substr(slash_pos + 1) : path;
}

// --- Helper: extract class name from error ---

static std::string extract_class_name(const std::string& error_msg) {
    // "Unknown class: Sprit2D"
    const std::string prefix = "Unknown class: ";
    if (error_msg.find(prefix) == 0) {
        return error_msg.substr(prefix.size());
    }
    // "Sprit2D is not a Node type"
    const std::string suffix = " is not a Node type";
    auto pos = error_msg.find(suffix);
    if (pos != std::string::npos) {
        return error_msg.substr(0, pos);
    }
    return "";
}

// --- Category-specific enrichment functions ---

static std::string enrich_no_scene_open(const std::string& error_msg) {
    return error_msg + " No scene is currently open in the editor. Use open_scene to open an existing scene or create_scene to create a new one.";
}

static std::string enrich_game_not_running(const std::string& error_msg) {
    return error_msg + " The game is not running. Use run_game to start the game first (mode: 'main', 'current', or 'custom').";
}

static std::string enrich_type_mismatch(const std::string& error_msg) {
    return error_msg + " Check the parameter value matches the expected type. Use get_scene_tree to inspect current node properties.";
}

static std::string enrich_script_error(const std::string& error_msg) {
    return error_msg + " Use list_project_files to see available files in the project. Script paths must start with res:// (e.g. res://scripts/player.gd).";
}

static std::string enrich_resource_error(const std::string& error_msg) {
    return error_msg + " Use list_project_files to see available resources in the project.";
}

static std::string enrich_deferred_pending(const std::string& error_msg) {
    return error_msg + " Wait for the previous request to complete before sending another. Use get_game_bridge_status to check the current state.";
}

static std::string enrich_generic(const std::string& error_msg) {
    return error_msg + " Use get_scene_tree to inspect the current scene state.";
}

// --- Public enrichment functions ---

std::string enrich_node_not_found(const std::string& error_msg, const std::string& /*tool_name*/,
                                   const std::vector<std::string>& sibling_names) {
    std::string target_name = extract_target_name(error_msg);
    std::string result = error_msg;

    if (!target_name.empty() && !sibling_names.empty()) {
        // Fuzzy match against siblings (Levenshtein distance <= 2)
        std::vector<std::pair<int, std::string>> matches;
        for (const auto& sibling : sibling_names) {
            int dist = levenshtein_distance(target_name, sibling);
            if (dist <= 2 && dist > 0) {
                matches.push_back({dist, sibling});
            }
        }
        std::sort(matches.begin(), matches.end());

        if (!matches.empty()) {
            result += " Did you mean: ";
            for (size_t i = 0; i < std::min(matches.size(), size_t(3)); i++) {
                if (i > 0) result += ", ";
                result += "'" + matches[i].second + "'";
            }
            result += "?";
        }

        // List available children (up to 10)
        result += " Available children: ";
        for (size_t i = 0; i < std::min(sibling_names.size(), size_t(10)); i++) {
            if (i > 0) result += ", ";
            result += sibling_names[i];
        }
        if (sibling_names.size() > 10) {
            result += "... (" + std::to_string(sibling_names.size()) + " total)";
        }
        result += ".";
    }

    result += " Use get_scene_tree to see the full node hierarchy.";
    return result;
}

std::string enrich_unknown_class(const std::string& error_msg, const std::string& /*tool_name*/,
                                  const std::vector<std::string>& known_classes) {
    std::string class_name = extract_class_name(error_msg);
    std::string result = error_msg;

    if (!class_name.empty() && !known_classes.empty()) {
        // Fuzzy match against known classes (Levenshtein distance <= 2)
        std::vector<std::pair<int, std::string>> matches;
        for (const auto& cls : known_classes) {
            int dist = levenshtein_distance(class_name, cls);
            if (dist <= 2 && dist > 0) {
                matches.push_back({dist, cls});
            }
        }
        std::sort(matches.begin(), matches.end());

        if (!matches.empty()) {
            result += " Did you mean: ";
            for (size_t i = 0; i < std::min(matches.size(), size_t(3)); i++) {
                if (i > 0) result += ", ";
                result += "'" + matches[i].second + "'";
            }
            result += "?";
        }
    }

    result += " Use get_scene_tree to see existing node types in your scene.";
    return result;
}

std::string enrich_error(const std::string& error_msg, const std::string& tool_name) {
    auto category = categorize_error(error_msg, tool_name);

    switch (category) {
        case ErrorCategory::NODE_NOT_FOUND:
            // Pure C++ version (no siblings available)
            return error_msg + " Use get_scene_tree to see the full node hierarchy.";
        case ErrorCategory::NO_SCENE_OPEN:
            return enrich_no_scene_open(error_msg);
        case ErrorCategory::GAME_NOT_RUNNING:
            return enrich_game_not_running(error_msg);
        case ErrorCategory::UNKNOWN_CLASS:
            // Pure C++ version (no class list available)
            return error_msg + " Use get_scene_tree to see existing node types in your scene.";
        case ErrorCategory::TYPE_MISMATCH:
            return enrich_type_mismatch(error_msg);
        case ErrorCategory::SCRIPT_ERROR:
            return enrich_script_error(error_msg);
        case ErrorCategory::RESOURCE_ERROR:
            return enrich_resource_error(error_msg);
        case ErrorCategory::DEFERRED_PENDING:
            return enrich_deferred_pending(error_msg);
        case ErrorCategory::GENERIC:
        default:
            return enrich_generic(error_msg);
    }
}

// --- Godot-dependent section ---

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

std::string enrich_error_with_context(const std::string& error_msg, const std::string& tool_name) {
    auto category = categorize_error(error_msg, tool_name);

    if (category == ErrorCategory::NODE_NOT_FOUND) {
        // Try to fetch sibling names from the scene tree
        std::vector<std::string> sibling_names;
        godot::Node* scene_root = godot::EditorInterface::get_singleton()->get_edited_scene_root();
        if (scene_root) {
            // Extract the parent path from the error message
            std::string target_name = extract_target_name(error_msg);
            // Get the full path from the error and determine parent
            const char* prefixes[] = {
                "Node not found: ",
                "Parent not found: ",
                "Source node not found: ",
                "Target node not found: ",
                "Node not found at path: "
            };
            std::string full_path;
            for (const auto* prefix : prefixes) {
                std::string p(prefix);
                if (error_msg.find(p) == 0) {
                    full_path = error_msg.substr(p.size());
                    break;
                }
            }
            // Find parent path
            auto slash_pos = full_path.rfind('/');
            std::string parent_path = (slash_pos != std::string::npos) ? full_path.substr(0, slash_pos) : "";

            godot::Node* parent = parent_path.empty() ? scene_root :
                scene_root->get_node_or_null(godot::NodePath(parent_path.c_str()));

            if (parent) {
                for (int i = 0; i < parent->get_child_count(); i++) {
                    godot::String name = parent->get_child(i)->get_name();
                    sibling_names.push_back(std::string(name.utf8().get_data()));
                }
            }
        }
        return enrich_node_not_found(error_msg, tool_name, sibling_names);
    }

    if (category == ErrorCategory::UNKNOWN_CLASS) {
        // Fetch Node subclass names from ClassDB (cached)
        static std::vector<std::string> node_classes;
        if (node_classes.empty()) {
            godot::PackedStringArray classes = godot::ClassDB::get_inheriters_from_class("Node");
            for (int i = 0; i < classes.size(); i++) {
                node_classes.push_back(std::string(classes[i].utf8().get_data()));
            }
        }
        return enrich_unknown_class(error_msg, tool_name, node_classes);
    }

    // For all other categories, pure C++ enrichment is sufficient
    return enrich_error(error_msg, tool_name);
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
