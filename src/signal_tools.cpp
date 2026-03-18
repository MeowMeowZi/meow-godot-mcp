#include "signal_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

// Helper: resolve a node path from scene root. Returns nullptr if not found.
static godot::Node* resolve_node(const std::string& node_path) {
    auto* ei = godot::EditorInterface::get_singleton();
    if (!ei) return nullptr;
    godot::Node* scene_root = ei->get_edited_scene_root();
    if (!scene_root) return nullptr;
    return scene_root->get_node_or_null(godot::NodePath(godot::String(node_path.c_str())));
}

nlohmann::json get_node_signals(const std::string& node_path) {
    godot::Node* node = resolve_node(node_path);
    if (!node) {
        return {{"success", false}, {"error", "Node not found: " + node_path}};
    }

    nlohmann::json signals_json = nlohmann::json::array();

    // get_signal_list() returns Array<Dictionary>
    godot::TypedArray<godot::Dictionary> signals = node->get_signal_list();

    for (int i = 0; i < signals.size(); i++) {
        godot::Dictionary sig = signals[i];
        godot::String name = sig["name"];
        nlohmann::json sig_json;
        sig_json["name"] = std::string(godot::String(name).utf8().get_data());

        // Extract argument info
        godot::Array args = sig["args"];
        nlohmann::json args_json = nlohmann::json::array();
        for (int j = 0; j < args.size(); j++) {
            godot::Dictionary arg = args[j];
            nlohmann::json arg_json;
            arg_json["name"] = std::string(godot::String(arg["name"]).utf8().get_data());
            arg_json["type"] = static_cast<int>(arg["type"]);
            args_json.push_back(arg_json);
        }
        sig_json["args"] = args_json;

        // Get connections for this signal
        godot::TypedArray<godot::Dictionary> conns =
            node->get_signal_connection_list(sig["name"]);

        nlohmann::json connections_json = nlohmann::json::array();
        for (int j = 0; j < conns.size(); j++) {
            godot::Dictionary conn = conns[j];
            nlohmann::json conn_json;

            // Extract callable info (target object + method)
            godot::Callable callable = conn["callable"];
            godot::Object* target_obj = callable.get_object();
            if (target_obj) {
                godot::Node* target_node = godot::Object::cast_to<godot::Node>(target_obj);
                if (target_node && target_node->is_inside_tree()) {
                    auto* ei = godot::EditorInterface::get_singleton();
                    godot::Node* scene_root = ei->get_edited_scene_root();
                    godot::String rel_path = godot::String(scene_root->get_path_to(target_node));
                    conn_json["target_path"] = std::string(rel_path.utf8().get_data());
                }
            }
            godot::StringName method = callable.get_method();
            conn_json["method"] = std::string(godot::String(method).utf8().get_data());
            conn_json["flags"] = static_cast<int>(conn["flags"]);

            connections_json.push_back(conn_json);
        }
        sig_json["connections"] = connections_json;
        sig_json["connection_count"] = conns.size();

        signals_json.push_back(sig_json);
    }

    return {
        {"success", true},
        {"node_path", node_path},
        {"signals", signals_json},
        {"signal_count", signals_json.size()}
    };
}

nlohmann::json connect_signal(const std::string& source_path, const std::string& signal_name,
                               const std::string& target_path, const std::string& method_name) {
    godot::Node* source = resolve_node(source_path);
    if (!source) {
        return {{"success", false}, {"error", "Source node not found: " + source_path}};
    }

    godot::Node* target = resolve_node(target_path);
    if (!target) {
        return {{"success", false}, {"error", "Target node not found: " + target_path}};
    }

    godot::StringName sig_name(godot::String(signal_name.c_str()));
    godot::Callable callable(target, godot::StringName(godot::String(method_name.c_str())));

    // Check if signal exists on source node
    bool signal_exists = false;
    godot::TypedArray<godot::Dictionary> signals = source->get_signal_list();
    for (int i = 0; i < signals.size(); i++) {
        godot::Dictionary sig = signals[i];
        if (godot::String(sig["name"]) == godot::String(signal_name.c_str())) {
            signal_exists = true;
            break;
        }
    }
    if (!signal_exists) {
        return {{"success", false}, {"error", "Signal '" + signal_name + "' does not exist on node: " + source_path}};
    }

    // Check if already connected
    if (source->is_connected(sig_name, callable)) {
        return {{"success", false}, {"error", "Signal '" + signal_name + "' is already connected to " + target_path + "::" + method_name}};
    }

    godot::Error err = source->connect(sig_name, callable, 0);
    if (err != godot::OK) {
        return {{"success", false}, {"error", "Failed to connect signal (error code: " + std::to_string(static_cast<int>(err)) + ")"}};
    }

    return {
        {"success", true},
        {"source_path", source_path},
        {"signal_name", signal_name},
        {"target_path", target_path},
        {"method_name", method_name}
    };
}

nlohmann::json disconnect_signal(const std::string& source_path, const std::string& signal_name,
                                  const std::string& target_path, const std::string& method_name) {
    godot::Node* source = resolve_node(source_path);
    if (!source) {
        return {{"success", false}, {"error", "Source node not found: " + source_path}};
    }

    godot::Node* target = resolve_node(target_path);
    if (!target) {
        return {{"success", false}, {"error", "Target node not found: " + target_path}};
    }

    godot::StringName sig_name(godot::String(signal_name.c_str()));
    godot::Callable callable(target, godot::StringName(godot::String(method_name.c_str())));

    // Check if actually connected
    if (!source->is_connected(sig_name, callable)) {
        return {{"success", false}, {"error", "Signal '" + signal_name + "' is not connected to " + target_path + "::" + method_name}};
    }

    source->disconnect(sig_name, callable);

    return {
        {"success", true},
        {"source_path", source_path},
        {"signal_name", signal_name},
        {"target_path", target_path},
        {"method_name", method_name},
        {"disconnected", true}
    };
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
