#ifndef MEOW_GODOT_MCP_MCP_DOCK_H
#define MEOW_GODOT_MCP_MCP_DOCK_H

#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_separator.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>

#include <string>

// Plain C++ class (NOT a Godot Object, NOT registered to ClassDB)
// Manages a VBoxContainer hierarchy for the MCP Meow dock panel.
// Owned by MCPPlugin via raw pointer (same pattern as MCPServer).
class MCPDock {
public:
    MCPDock();
    ~MCPDock();

    // Returns the root Control to pass to add_control_to_dock()
    godot::VBoxContainer* get_root() const;

    // Returns buttons for signal connection (connected to MCPPlugin, not MCPDock)
    godot::Button* get_toggle_button() const;
    godot::Button* get_restart_button() const;
    godot::Button* get_configure_mcp_button() const;
    godot::SpinBox* get_port_spinbox() const;

    // Update status display (called from MCPPlugin::_process via timer)
    // Only updates UI labels when state actually changes (dirty check)
    void update_status(bool running, bool client_connected, int port,
                       const std::string& version, int tool_count);

    // Update button text (Start vs Stop) and restart button disabled state
    void update_buttons(bool running);

    // Show/hide autoload warning banner
    void set_autoload_warning(bool missing);

    // Show configure feedback message in dock panel (auto-hides after timeout)
    void show_configure_feedback(const godot::String& command);

    // Called from MCPPlugin::_process to tick feedback timer
    void tick_feedback(double delta, bool client_connected);

private:
    godot::VBoxContainer* root = nullptr;
    godot::Label* status_label = nullptr;
    godot::Label* port_label = nullptr;
    godot::Label* version_label = nullptr;
    godot::Label* tools_label = nullptr;
    godot::Button* toggle_button = nullptr;
    godot::Button* restart_button = nullptr;
    godot::Button* configure_mcp_button = nullptr;
    godot::SpinBox* port_spinbox = nullptr;

    godot::PanelContainer* autoload_warning = nullptr;
    godot::PanelContainer* configure_feedback = nullptr;
    godot::Label* configure_feedback_label = nullptr;

    // Feedback auto-hide timer
    double feedback_timer = 0.0;

    // Cached state for dirty checking
    bool last_running = false;
    bool last_connected = false;
    int last_tool_count = 0;
};

#endif // MEOW_GODOT_MCP_MCP_DOCK_H
