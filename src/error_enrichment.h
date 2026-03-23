#ifndef MEOW_GODOT_MCP_ERROR_ENRICHMENT_H
#define MEOW_GODOT_MCP_ERROR_ENRICHMENT_H

// Pure C++17 header -- NO Godot dependency for unit testability
// Godot-dependent overloads guarded by MEOW_GODOT_MCP_GODOT_ENABLED

#include <string>
#include <vector>

// Error category classification for enrichment routing
enum class ErrorCategory {
    NODE_NOT_FOUND,
    NO_SCENE_OPEN,
    GAME_NOT_RUNNING,
    UNKNOWN_CLASS,
    TYPE_MISMATCH,
    SCRIPT_ERROR,
    RESOURCE_ERROR,
    DEFERRED_PENDING,
    GENERIC
};

// Levenshtein edit distance (space-optimized, O(min(m,n)) space)
int levenshtein_distance(const std::string& a, const std::string& b);

// Classify error message into a category for enrichment routing
ErrorCategory categorize_error(const std::string& error_msg, const std::string& tool_name);

// Main entry point: enrich error with diagnostic text (pure C++, no Godot calls)
// Appends context-specific guidance based on error category
std::string enrich_error(const std::string& error_msg, const std::string& tool_name);

// Node-not-found enrichment with pre-fetched sibling list (for testability)
std::string enrich_node_not_found(const std::string& error_msg, const std::string& tool_name,
                                   const std::vector<std::string>& sibling_names);

// Unknown-class enrichment with pre-fetched class list (for testability)
std::string enrich_unknown_class(const std::string& error_msg, const std::string& tool_name,
                                  const std::vector<std::string>& known_classes);

// Missing-parameter enrichment: appends tool-specific parameter format examples
// to INVALID_PARAMS error messages (ERR-04)
std::string enrich_missing_params(const std::string& error_msg, const std::string& tool_name);

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
// Godot-dependent overload: fetches sibling names / class list from live editor
// Falls back to pure C++ enrich_error for categories that don't need Godot context
std::string enrich_error_with_context(const std::string& error_msg, const std::string& tool_name);
#endif

#endif // MEOW_GODOT_MCP_ERROR_ENRICHMENT_H
