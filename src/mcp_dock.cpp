#include "mcp_dock.h"
#include "mcp_tool_registry.h"

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <map>

using namespace godot;

MCPDock::MCPDock() {
    // Root container -- its name becomes the dock tab title
    root = memnew(VBoxContainer);
    root->set_name("MCP Meow");

    // Status section
    status_label = memnew(Label);
    status_label->set_text(String::utf8("状态：已停止"));
    root->add_child(status_label);

    // Port row: label + editable spinbox
    auto* port_row = memnew(HBoxContainer);
    root->add_child(port_row);

    port_label = memnew(Label);
    port_label->set_text(String::utf8("端口："));
    port_row->add_child(port_label);

    port_spinbox = memnew(SpinBox);
    port_spinbox->set_min(1024);
    port_spinbox->set_max(65535);
    port_spinbox->set_step(1);
    port_spinbox->set_value(6800);
    port_spinbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    port_spinbox->set_tooltip_text(String::utf8("MCP 服务监听端口（修改后自动重启）"));
    port_row->add_child(port_spinbox);

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

    // Tools category section (collapsible area with checkboxes)
    auto* tools_sep = memnew(HSeparator);
    root->add_child(tools_sep);

    auto* tools_header = memnew(Label);
    tools_header->set_text(String::utf8("工具开关"));
    tools_header->add_theme_font_size_override("font_size", 13);
    root->add_child(tools_header);

    // Scroll container for tool checkboxes (limited height)
    auto* scroll = memnew(ScrollContainer);
    scroll->set_custom_minimum_size(Vector2(0, 180));
    scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    root->add_child(scroll);

    tools_section = memnew(VBoxContainer);
    tools_section->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    scroll->add_child(tools_section);

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

SpinBox* MCPDock::get_port_spinbox() const {
    return port_spinbox;
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

    // Port spinbox: show actual port when running
    if (running && port_spinbox) {
        port_spinbox->set_value_no_signal(port);
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
        feedback_timer = 1.0;  // Activate timer (hide on client connect)
    }
}

void MCPDock::tick_feedback(double delta, bool client_connected) {
    if (feedback_timer > 0.0 && client_connected) {
        feedback_timer = 0.0;
        if (configure_feedback) {
            configure_feedback->set_visible(false);
        }
    }
}

void MCPDock::set_tool_toggle_callback(ToolToggleCallback cb) {
    tool_toggle_cb = cb;
}

const std::vector<CheckBox*>& MCPDock::get_tool_checkboxes() const {
    return tool_checkboxes;
}

const std::vector<Button*>& MCPDock::get_category_headers() const {
    return category_headers;
}

void MCPDock::build_tool_checkboxes() {
    if (!tools_section) return;

    // Clear existing
    for (auto* cb : tool_checkboxes) {
        // Children are owned by Godot scene tree
    }
    tool_checkboxes.clear();
    category_headers.clear();
    while (tools_section->get_child_count() > 0) {
        auto* child = tools_section->get_child(0);
        tools_section->remove_child(child);
        memdelete(child);
    }

    // Group tools by category
    std::map<ToolCategory, std::vector<const ToolDef*>> by_category;
    for (const auto& tool : get_all_tools()) {
        by_category[tool.category].push_back(&tool);
    }

    // Category order
    ToolCategory order[] = {
        ToolCategory::SCENE, ToolCategory::SCRIPT, ToolCategory::PROJECT,
        ToolCategory::RUNTIME, ToolCategory::INPUT, ToolCategory::QUERY,
        ToolCategory::TILEMAP, ToolCategory::COMPOSITE, ToolCategory::DX
    };

    for (auto cat : order) {
        auto it = by_category.find(cat);
        if (it == by_category.end()) continue;

        int count = static_cast<int>(it->second.size());

        // Collapsible header button
        auto* header_btn = memnew(Button);
        String cat_name = String::utf8(get_category_name(cat));
        header_btn->set_text(String::utf8("\xe2\x96\xbc ") + cat_name + String(" (") + String::num_int64(count) + String(")"));
        header_btn->set_flat(true);
        header_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        header_btn->add_theme_font_size_override("font_size", 12);
        header_btn->add_theme_color_override("font_color", Color(0.7, 0.7, 0.85));
        header_btn->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
        tools_section->add_child(header_btn);

        // Container for this category's checkboxes
        auto* cat_box = memnew(VBoxContainer);
        cat_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        // Add left margin for indentation
        cat_box->add_theme_constant_override("margin_left", 16);
        tools_section->add_child(cat_box);

        // Connect header button to toggle visibility
        // Store cat_box reference in button meta for toggling
        header_btn->set_meta("cat_box", cat_box);
        header_btn->set_meta("cat_name", cat_name);

        // Tool checkboxes inside category container
        for (const auto* tool : it->second) {
            auto* cb = memnew(CheckBox);
            cb->set_text(String(tool->name.c_str()));
            cb->set_pressed_no_signal(!is_tool_disabled(tool->name));
            cb->add_theme_font_size_override("font_size", 11);
            cb->set_meta("tool_name", String(tool->name.c_str()));
            cat_box->add_child(cb);
            tool_checkboxes.push_back(cb);
        }

        // Store header for signal connection
        header_btn->set_meta("collapsed", false);
        category_headers.push_back(header_btn);
    }
}
