#ifndef MEOW_GODOT_MCP_TILEMAP_TOOLS_H
#define MEOW_GODOT_MCP_TILEMAP_TOOLS_H

#include <nlohmann/json.hpp>
#include <string>

namespace godot {
class EditorUndoRedoManager;
}

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

// Set tiles at specified grid coordinates on a TileMapLayer
nlohmann::json set_tilemap_cells(const std::string& node_path,
                                  const nlohmann::json& cells,
                                  godot::EditorUndoRedoManager* undo_redo);

// Erase tiles at specified grid coordinates on a TileMapLayer
nlohmann::json erase_tilemap_cells(const std::string& node_path,
                                    const nlohmann::json& coords,
                                    godot::EditorUndoRedoManager* undo_redo);

// Query tile information at specified coordinates
nlohmann::json get_tilemap_cell_info(const std::string& node_path,
                                      const nlohmann::json& coords);

// Query TileMapLayer metadata (tile_set info, used cells, bounds)
nlohmann::json get_tilemap_info(const std::string& node_path);

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
#endif // MEOW_GODOT_MCP_TILEMAP_TOOLS_H
