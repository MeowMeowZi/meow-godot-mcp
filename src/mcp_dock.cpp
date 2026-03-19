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
    status_label->set_text(String::utf8("状态：已停止"));
    root->add_child(status_label);

    port_label = memnew(Label);
    port_label->set_text(String::utf8("端口：--"));
    root->add_child(port_label);

    version_label = memnew(Label);
    version_label->set_text(String::utf8("Godot: 检测中..."));
    root->add_child(version_label);

    tools_label = memnew(Label);
    tools_label->set_text(String::utf8("工具数：0"));
    root->add_child(tools_label);

    // Separator between info and controls
    auto* sep = memnew(HSeparator);
    root->add_child(sep);

    // Button row
    auto* btn_box = memnew(HBoxContainer);
    root->add_child(btn_box);

    toggle_button = memnew(Button);
    toggle_button->set_text(String::utf8("启动"));
    toggle_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    btn_box->add_child(toggle_button);

    restart_button = memnew(Button);
    restart_button->set_text(String::utf8("重启"));
    restart_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    restart_button->set_disabled(true);
    btn_box->add_child(restart_button);

    // Separator before configure button
    auto* sep2 = memnew(HSeparator);
    root->add_child(sep2);

    // Configure Claude Code MCP button
    configure_mcp_button = memnew(Button);
    configure_mcp_button->set_text(String::utf8("配置 Claude Code MCP"));
    configure_mcp_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    root->add_child(configure_mcp_button);

    // Configure feedback panel (hidden by default, shown after button click)
    configure_feedback = memnew(PanelContainer);
    configure_feedback->set_visible(false);

    Ref<StyleBoxFlat> fb_style;
    fb_style.instantiate();
    fb_style->set_bg_color(Color(0.1, 0.4, 0.2, 0.3));
    fb_style->set_border_color(Color(0.2, 0.8, 0.4, 0.8));
    fb_style->set_border_width_all(1);
    fb_style->set_corner_radius_all(4);
    fb_style->set_content_margin_all(8);
    configure_feedback->add_theme_stylebox_override("panel", fb_style);

    configure_feedback_label = memnew(Label);
    configure_feedback_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    configure_feedback->add_child(configure_feedback_label);
    root->add_child(configure_feedback);

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
    warn_label->set_text(String::utf8(
        "游戏桥接：自动加载未配置\n"
        "项目设置 > 自动加载 > 添加：\n"
        "  路径: addons/meow_godot_mcp/companion/meow_mcp_bridge.gd\n"
        "  名称: MeowMCPBridge"
    ));
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

Button* MCPDock::get_configure_mcp_button() const {
    return configure_mcp_button;
}

void MCPDock::update_status(bool running, bool client_connected, int port,
                            const std::string& version, int tool_count) {
    // Dirty check: only update if state changed
    if (running == last_running && client_connected == last_connected && tool_count == last_tool_count) {
        return;
    }

    // Update status text based on three-state logic
    if (!running) {
        status_label->set_text(String::utf8("状态：已停止"));
    } else if (!client_connected) {
        status_label->set_text(String::utf8("状态：等待客户端连接..."));
    } else {
        status_label->set_text(String::utf8("状态：已连接"));
    }

    // Port display
    if (running) {
        port_label->set_text(String::utf8("端口：") + String::num_int64(port));
    } else {
        port_label->set_text(String::utf8("端口：--"));
    }

    // Version and tool count
    version_label->set_text(String("Godot: ") + String(version.c_str()));
    tools_label->set_text(String::utf8("工具数：") + String::num_int64(tool_count));

    // Update cached state
    last_running = running;
    last_connected = client_connected;
    last_tool_count = tool_count;
}

void MCPDock::update_buttons(bool running) {
    if (running) {
        toggle_button->set_text(String::utf8("停止"));
        restart_button->set_disabled(false);
    } else {
        toggle_button->set_text(String::utf8("启动"));
        restart_button->set_disabled(true);
    }
}

void MCPDock::set_autoload_warning(bool missing) {
    if (autoload_warning) {
        autoload_warning->set_visible(missing);
    }
}

void MCPDock::show_configure_feedback(const String& command) {
    if (configure_feedback && configure_feedback_label) {
        configure_feedback_label->set_text(
            String::utf8("已复制到剪贴板！请在 Claude Code 终端粘贴执行：\n\n")
            + command
            + String::utf8("\n\n执行后重启 Claude Code 即可连接。")
        );
        configure_feedback->set_visible(true);
    }
}
