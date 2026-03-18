#ifndef MEOW_GODOT_MCP_PROJECT_TOOLS_H
#define MEOW_GODOT_MCP_PROJECT_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED
nlohmann::json list_project_files();
nlohmann::json get_project_settings(const std::string& category = "");
nlohmann::json get_resource_info(const std::string& path);
#endif

#endif // MEOW_GODOT_MCP_PROJECT_TOOLS_H
