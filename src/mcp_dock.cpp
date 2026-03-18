#include "mcp_dock.h"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>

using namespace godot;

MCPDock::MCPDock() {
    // Root container -- its name becomes the dock tab title
    root = memnew(VBoxContainer);
    root->set_name("MCP Meow");

    // Status section
    status_label = memnew(Label);
    status_label->set_text("Status: Stopped");
    root->add_child(status_label);

    port_label = memnew(Label);
    port_label->set_text("Port: --");
    root->add_child(port_label);

    version_label = memnew(Label);
    version_label->set_text("Godot: detecting...");
    root->add_child(version_label);

    tools_label = memnew(Label);
    tools_label->set_text("Tools: 0");
    root->add_child(tools_label);

    // Separator between info and controls
    auto* sep = memnew(HSeparator);
    root->add_child(sep);

    // Button row
    auto* btn_box = memnew(HBoxContainer);
    root->add_child(btn_box);

    toggle_button = memnew(Button);
    toggle_button->set_text("Start");
    toggle_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    btn_box->add_child(toggle_button);

    restart_button = memnew(Button);
    restart_button->set_text("Restart");
    restart_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    restart_button->set_disabled(true);  // Disabled when server is stopped
    btn_box->add_child(restart_button);

    // Autoload warning banner (hidden by default)
    autoload_warning = memnew(PanelContainer);
    autoload_warning->set_visible(false);

    Ref<StyleBoxFlat> warn_style;
    warn_style.instantiate();
    warn_style->set_bg_color(Color(0.6, 0.4, 0.1, 0.3));
    warn_style->set_border_color(Color(0.9, 0.7, 0.2, 0.8));
    warn_style->set_border_width_all(1);
    warn_style->set_corner_radius_all(4);
    warn_style->set_content_margin_all(8);
    autoload_warning->add_theme_stylebox_override("panel", warn_style);

    auto* warn_label = memnew(Label);
    warn_label->set_text(
        "Game Bridge: Autoload not configured.\n"
        "Project Settings > Autoload > Add:\n"
        "  Path: addons/meow_godot_mcp/companion/meow_mcp_bridge.gd\n"
        "  Name: MeowMCPBridge"
    );
    warn_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    autoload_warning->add_child(warn_label);
    root->add_child(autoload_warning);

    // Initialize cached state
    last_running = false;
    last_connected = false;
    last_tool_count = 0;
}

MCPDock::~MCPDock() {
    // Godot owns the nodes via scene tree -- memdelete happens in MCPPlugin::_exit_tree
}

VBoxContainer* MCPDock::get_root() const {
    return root;
}

Button* MCPDock::get_toggle_button() const {
    return toggle_button;
}

Button* MCPDock::get_restart_button() const {
    return restart_button;
}

void MCPDock::update_status(bool running, bool client_connected, int port,
                            const std::string& version, int tool_count) {
    // Dirty check: only update if state changed
    if (running == last_running && client_connected == last_connected && tool_count == last_tool_count) {
        return;
    }

    // Update status text based on three-state logic
    if (!running) {
        status_label->set_text("Status: Stopped");
    } else if (!client_connected) {
        status_label->set_text("Status: Waiting for client...");
    } else {
        status_label->set_text("Status: Connected");
    }

    // Port display
    if (running) {
        status_label->set_text(status_label->get_text());  // Already set above
        port_label->set_text(String("Port: ") + String::num_int64(port));
    } else {
        port_label->set_text("Port: --");
    }

    // Version and tool count
    version_label->set_text(String("Godot: ") + String(version.c_str()));
    tools_label->set_text(String("Tools: ") + String::num_int64(tool_count));

    // Update cached state
    last_running = running;
    last_connected = client_connected;
    last_tool_count = tool_count;
}

void MCPDock::update_buttons(bool running) {
    if (running) {
        toggle_button->set_text("Stop");
        restart_button->set_disabled(false);
    } else {
        toggle_button->set_text("Start");
        restart_button->set_disabled(true);
    }
}

void MCPDock::set_autoload_warning(bool missing) {
    if (autoload_warning) {
        autoload_warning->set_visible(missing);
    }
}
