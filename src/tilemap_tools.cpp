#include "tilemap_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/tile_map_layer.hpp>
#include <godot_cpp/classes/tile_set.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

// --- Helper: Node lookup + TileMapLayer validation ---

struct TileMapLayerLookupResult {
    bool success;
    nlohmann::json error;
    TileMapLayer* layer;
};

static TileMapLayerLookupResult lookup_tilemap_layer(const std::string& node_path) {
    Node* scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
    if (!scene_root) {
        return {false, {{"error", "No scene open"}}, nullptr};
    }

    Node* node = nullptr;
    if (node_path.empty() || node_path == ".") {
        node = scene_root;
    } else {
        node = scene_root->get_node_or_null(NodePath(node_path.c_str()));
    }
    if (!node) {
        return {false, {{"error", "Node not found: " + node_path}}, nullptr};
    }

    TileMapLayer* layer = Object::cast_to<TileMapLayer>(node);
    if (!layer) {
        return {false, {{"error", "Node is not a TileMapLayer: " + node_path}}, nullptr};
    }

    return {true, {}, layer};
}

nlohmann::json set_tilemap_cells(const std::string& node_path,
                                  const nlohmann::json& cells,
                                  EditorUndoRedoManager* undo_redo) {
    auto [success, error, layer] = lookup_tilemap_layer(node_path);
    if (!success) return error;

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    if (!cells.is_array() || cells.empty()) {
        return {{"error", "cells must be a non-empty array"}};
    }

    // Capture old state and apply new cells
    undo_redo->create_action(String("MCP: Set TileMap cells"));

    int cells_set = 0;
    for (auto& cell : cells) {
        if (!cell.contains("x") || !cell.contains("y") ||
            !cell.contains("source_id") || !cell.contains("atlas_x") || !cell.contains("atlas_y")) {
            continue;
        }

        int x = cell["x"].get<int>();
        int y = cell["y"].get<int>();
        int source_id = cell["source_id"].get<int>();
        int atlas_x = cell["atlas_x"].get<int>();
        int atlas_y = cell["atlas_y"].get<int>();
        int alt_tile = cell.value("alternative_tile", 0);

        Vector2i coords(x, y);
        Vector2i atlas_coords(atlas_x, atlas_y);

        // Capture old state for undo
        int old_source = layer->get_cell_source_id(coords);
        Vector2i old_atlas = layer->get_cell_atlas_coords(coords);
        int old_alt = layer->get_cell_alternative_tile(coords);

        // Apply via UndoRedo
        undo_redo->add_do_method(layer, "set_cell", coords, source_id, atlas_coords, alt_tile);
        if (old_source == -1) {
            undo_redo->add_undo_method(layer, "erase_cell", coords);
        } else {
            undo_redo->add_undo_method(layer, "set_cell", coords, old_source, old_atlas, old_alt);
        }

        ++cells_set;
    }

    undo_redo->commit_action();

    return {{"success", true}, {"cells_set", cells_set}};
}

nlohmann::json erase_tilemap_cells(const std::string& node_path,
                                    const nlohmann::json& coords,
                                    EditorUndoRedoManager* undo_redo) {
    auto [success, error, layer] = lookup_tilemap_layer(node_path);
    if (!success) return error;

    if (!undo_redo) {
        return {{"error", "UndoRedo not available"}};
    }

    if (!coords.is_array() || coords.empty()) {
        return {{"error", "coords must be a non-empty array"}};
    }

    undo_redo->create_action(String("MCP: Erase TileMap cells"));

    int cells_erased = 0;
    for (auto& coord : coords) {
        if (!coord.contains("x") || !coord.contains("y")) continue;

        int x = coord["x"].get<int>();
        int y = coord["y"].get<int>();
        Vector2i pos(x, y);

        // Capture old state for undo
        int old_source = layer->get_cell_source_id(pos);
        if (old_source == -1) continue; // Already empty

        Vector2i old_atlas = layer->get_cell_atlas_coords(pos);
        int old_alt = layer->get_cell_alternative_tile(pos);

        undo_redo->add_do_method(layer, "erase_cell", pos);
        undo_redo->add_undo_method(layer, "set_cell", pos, old_source, old_atlas, old_alt);

        ++cells_erased;
    }

    undo_redo->commit_action();

    return {{"success", true}, {"cells_erased", cells_erased}};
}

nlohmann::json get_tilemap_cell_info(const std::string& node_path,
                                      const nlohmann::json& coords) {
    auto [success, error, layer] = lookup_tilemap_layer(node_path);
    if (!success) return error;

    if (!coords.is_array() || coords.empty()) {
        return {{"error", "coords must be a non-empty array"}};
    }

    nlohmann::json cells = nlohmann::json::array();
    for (auto& coord : coords) {
        if (!coord.contains("x") || !coord.contains("y")) continue;

        int x = coord["x"].get<int>();
        int y = coord["y"].get<int>();
        Vector2i pos(x, y);

        int source_id = layer->get_cell_source_id(pos);
        bool empty = (source_id == -1);

        nlohmann::json cell_info = {
            {"x", x}, {"y", y},
            {"source_id", source_id},
            {"empty", empty}
        };

        if (!empty) {
            Vector2i atlas = layer->get_cell_atlas_coords(pos);
            cell_info["atlas_coords"] = {{"x", atlas.x}, {"y", atlas.y}};
            cell_info["alternative_tile"] = layer->get_cell_alternative_tile(pos);
        }

        cells.push_back(cell_info);
    }

    return {{"success", true}, {"cells", cells}};
}

nlohmann::json get_tilemap_info(const std::string& node_path) {
    auto [success, error, layer] = lookup_tilemap_layer(node_path);
    if (!success) return error;

    nlohmann::json result = {
        {"success", true},
        {"node_path", node_path}
    };

    // TileSet info
    Ref<TileSet> tile_set = layer->get_tile_set();
    if (tile_set.is_valid()) {
        result["has_tile_set"] = true;
        result["tile_set_sources_count"] = tile_set->get_source_count();
    } else {
        result["has_tile_set"] = false;
        result["tile_set_sources_count"] = 0;
    }

    // Used cells info
    TypedArray<Vector2i> used_cells = layer->get_used_cells();
    result["used_cells_count"] = used_cells.size();

    // Bounding rect
    Rect2i used_rect = layer->get_used_rect();
    result["used_rect"] = {
        {"x", used_rect.position.x},
        {"y", used_rect.position.y},
        {"width", used_rect.size.x},
        {"height", used_rect.size.y}
    };

    return result;
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
