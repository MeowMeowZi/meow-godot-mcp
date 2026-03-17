#include "project_tools.h"
#include "script_tools.h"  // for validate_res_path

#ifdef GODOT_MCP_MEOW_GODOT_ENABLED

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

// Recursive helper to collect files under a directory
static void collect_files(const String& dir_path, nlohmann::json& files) {
    Ref<DirAccess> dir = DirAccess::open(dir_path);
    if (!dir.is_valid()) {
        return;
    }

    dir->list_dir_begin();
    String name = dir->get_next();

    while (!name.is_empty()) {
        // Skip hidden entries (starting with ".")
        if (name.begins_with(".")) {
            name = dir->get_next();
            continue;
        }

        String full_path = dir_path;
        if (!full_path.ends_with("/")) {
            full_path += "/";
        }
        full_path += name;

        if (dir->current_is_dir()) {
            collect_files(full_path, files);
        } else {
            // Extract extension
            String ext = name.get_extension();
            files.push_back({
                {"path", std::string(full_path.utf8().get_data())},
                {"type", std::string(ext.utf8().get_data())}
            });
        }

        name = dir->get_next();
    }

    dir->list_dir_end();
}

nlohmann::json list_project_files() {
    nlohmann::json files = nlohmann::json::array();
    collect_files("res://", files);
    return {
        {"success", true},
        {"files", files},
        {"count", files.size()}
    };
}

nlohmann::json get_project_settings() {
    ProjectSettings* settings = ProjectSettings::get_singleton();
    if (!settings) {
        return {{"success", false}, {"error", "ProjectSettings singleton not available"}};
    }

    nlohmann::json result_settings = nlohmann::json::object();

    TypedArray<Dictionary> properties = settings->get_property_list();
    for (int i = 0; i < properties.size(); i++) {
        Dictionary prop = properties[i];
        String name = prop["name"];
        int usage = prop["usage"];

        // Skip internal and private properties
        std::string name_str(name.utf8().get_data());
        if (name_str.empty() || name_str[0] == '_') {
            continue;
        }
        if (usage & PROPERTY_USAGE_INTERNAL) {
            continue;
        }

        Variant value = settings->get_setting(name);
        String value_str = String(value);
        result_settings[name_str] = std::string(value_str.utf8().get_data());
    }

    return {
        {"success", true},
        {"settings", result_settings}
    };
}

nlohmann::json get_resource_info(const std::string& path) {
    std::string error;
    if (!validate_res_path(path, error)) {
        return {{"success", false}, {"error", error}};
    }

    String godot_path = String(path.c_str());

    ResourceLoader* loader = ResourceLoader::get_singleton();
    if (!loader) {
        return {{"success", false}, {"error", "ResourceLoader singleton not available"}};
    }

    if (!loader->exists(godot_path)) {
        return {{"success", false}, {"error", "Resource not found: " + path}};
    }

    Ref<Resource> res = loader->load(godot_path);
    if (!res.is_valid()) {
        return {{"success", false}, {"error", "Failed to load resource: " + path}};
    }

    std::string class_name(String(res->get_class()).utf8().get_data());
    std::string res_path(String(res->get_path()).utf8().get_data());

    nlohmann::json properties = nlohmann::json::object();

    TypedArray<Dictionary> prop_list = res->get_property_list();
    for (int i = 0; i < prop_list.size(); i++) {
        Dictionary prop = prop_list[i];
        String prop_name = prop["name"];
        int usage = prop["usage"];

        // Only keep properties with STORAGE or EDITOR usage, skip INTERNAL
        if (usage & PROPERTY_USAGE_INTERNAL) {
            continue;
        }
        if (!(usage & PROPERTY_USAGE_STORAGE) && !(usage & PROPERTY_USAGE_EDITOR)) {
            continue;
        }

        std::string prop_name_str(prop_name.utf8().get_data());
        if (prop_name_str.empty()) {
            continue;
        }

        Variant value = res->get(prop_name);
        String value_str = String(value);
        properties[prop_name_str] = std::string(value_str.utf8().get_data());
    }

    return {
        {"success", true},
        {"path", res_path},
        {"type", class_name},
        {"properties", properties}
    };
}

#endif // GODOT_MCP_MEOW_GODOT_ENABLED
