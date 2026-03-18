#ifndef GODOT_MCP_MEOW_SIGNAL_TOOLS_H
#define GODOT_MCP_MEOW_SIGNAL_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

#ifdef GODOT_MCP_MEOW_GODOT_ENABLED

// Get all signals defined on a node, with parameter info and connection details
nlohmann::json get_node_signals(const std::string& node_path);

// Connect a signal from source_path node to method_name on target_path node
nlohmann::json connect_signal(const std::string& source_path, const std::string& signal_name,
                               const std::string& target_path, const std::string& method_name);

// Disconnect a signal connection between two nodes
nlohmann::json disconnect_signal(const std::string& source_path, const std::string& signal_name,
                                  const std::string& target_path, const std::string& method_name);

#endif

#endif // GODOT_MCP_MEOW_SIGNAL_TOOLS_H
