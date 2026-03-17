#ifndef GODOT_MCP_MEOW_PROJECT_TOOLS_H
#define GODOT_MCP_MEOW_PROJECT_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef GODOT_MCP_MEOW_GODOT_ENABLED
nlohmann::json list_project_files();
nlohmann::json get_project_settings();
nlohmann::json get_resource_info(const std::string& path);
#endif

#endif // GODOT_MCP_MEOW_PROJECT_TOOLS_H
