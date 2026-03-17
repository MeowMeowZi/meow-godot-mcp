#ifndef GODOT_MCP_MEOW_MCP_PROMPTS_H
#define GODOT_MCP_MEOW_MCP_PROMPTS_H

// Pure C++17 + nlohmann/json -- NO Godot headers
// This allows unit testing without godot-cpp dependency

#include <nlohmann/json.hpp>
#include <string>

// Returns JSON array of all prompt objects (name, description, arguments)
nlohmann::json get_all_prompts_json();

// Returns messages array for a prompt with argument substitution, or null if not found
nlohmann::json get_prompt_messages(const std::string& name, const nlohmann::json& arguments);

// Check if a prompt name is valid
bool prompt_exists(const std::string& name);

#endif // GODOT_MCP_MEOW_MCP_PROMPTS_H
