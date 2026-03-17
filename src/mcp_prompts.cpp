#include "mcp_prompts.h"
#include <unordered_map>
#include <functional>

// ---------------------------------------------------------------------------
// Prompt template definitions
// ---------------------------------------------------------------------------

struct PromptDef {
    std::string name;
    std::string description;
    nlohmann::json arguments; // JSON array of argument definitions
    // Generator: takes arguments object, returns messages array
    std::function<nlohmann::json(const nlohmann::json&)> generate;
};

static const std::vector<PromptDef>& get_prompt_defs() {
    static const std::vector<PromptDef> defs = {
        // 1. create_player_controller
        {
            "create_player_controller",
            "Create a player controller with movement, input handling, and collision",
            nlohmann::json::array({
                {{"name", "movement_type"}, {"description", "Movement style: 2d_platformer, 2d_top_down, 3d_first_person, 3d_third_person"}, {"required", true}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string movement_type = "2d_platformer";
                if (args.contains("movement_type") && args["movement_type"].is_string()) {
                    movement_type = args["movement_type"].get<std::string>();
                }

                std::string text;
                if (movement_type == "2d_platformer") {
                    text = "Create a 2d_platformer player controller in Godot:\n\n"
                           "1. Create a CharacterBody2D node named 'Player'\n"
                           "2. Add a CollisionShape2D child with a RectangleShape2D\n"
                           "3. Add a Sprite2D child for the player visual\n"
                           "4. Create a GDScript with:\n"
                           "   - SPEED and JUMP_VELOCITY constants\n"
                           "   - _physics_process: apply gravity, handle jump (ui_accept), horizontal movement (ui_left/ui_right)\n"
                           "   - Use move_and_slide() for collision response\n"
                           "5. Set up Input Map actions: ui_left, ui_right, ui_accept\n"
                           "6. Configure collision layer 1 (player) and mask 1 (world)";
                } else if (movement_type == "2d_top_down") {
                    text = "Create a 2d_top_down player controller in Godot:\n\n"
                           "1. Create a CharacterBody2D node named 'Player'\n"
                           "2. Add a CollisionShape2D child with a CircleShape2D\n"
                           "3. Add a Sprite2D child for the player visual\n"
                           "4. Create a GDScript with:\n"
                           "   - SPEED constant\n"
                           "   - _physics_process: get input vector from ui_left/ui_right/ui_up/ui_down\n"
                           "   - Set velocity = direction * SPEED, call move_and_slide()\n"
                           "5. Set up Input Map actions: ui_left, ui_right, ui_up, ui_down\n"
                           "6. Configure collision layer 1 (player) and mask 1 (world)";
                } else if (movement_type == "3d_first_person") {
                    text = "Create a 3d_first_person player controller in Godot:\n\n"
                           "1. Create a CharacterBody3D node named 'Player'\n"
                           "2. Add a CollisionShape3D child with a CapsuleShape3D\n"
                           "3. Add a Camera3D child at head height (~1.5m)\n"
                           "4. Create a GDScript with:\n"
                           "   - SPEED, JUMP_VELOCITY, MOUSE_SENSITIVITY constants\n"
                           "   - _input: handle mouse look (rotate body Y, camera X)\n"
                           "   - _physics_process: apply gravity, WASD movement relative to facing direction\n"
                           "   - Use move_and_slide() for collision, Input.mouse_mode = CAPTURED\n"
                           "5. Set up Input Map: move_forward, move_back, move_left, move_right, jump\n"
                           "6. Configure collision layer 1 (player) and mask 1 (world)";
                } else if (movement_type == "3d_third_person") {
                    text = "Create a 3d_third_person player controller in Godot:\n\n"
                           "1. Create a CharacterBody3D node named 'Player'\n"
                           "2. Add a CollisionShape3D child with a CapsuleShape3D\n"
                           "3. Add a SpringArm3D with Camera3D child for orbit camera\n"
                           "4. Add a MeshInstance3D child for the player visual\n"
                           "5. Create a GDScript with:\n"
                           "   - SPEED, JUMP_VELOCITY, CAMERA_SENSITIVITY constants\n"
                           "   - _input: handle camera orbit via mouse\n"
                           "   - _physics_process: movement relative to camera facing direction\n"
                           "   - Use move_and_slide() with gravity\n"
                           "6. Set up Input Map: move_forward, move_back, move_left, move_right, jump\n"
                           "7. Configure collision layer 1 (player) and mask 1 (world)";
                } else {
                    text = "Create a " + movement_type + " player controller in Godot:\n\n"
                           "1. Choose the appropriate CharacterBody node for your dimension (2D/3D)\n"
                           "2. Add a CollisionShape child with appropriate shape\n"
                           "3. Add a visual representation (Sprite/Mesh)\n"
                           "4. Create a GDScript with movement logic for " + movement_type + "\n"
                           "5. Set up appropriate Input Map actions\n"
                           "6. Configure collision layers and masks";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 2. setup_scene_structure
        {
            "setup_scene_structure",
            "Set up a well-organized scene tree structure for a game",
            nlohmann::json::array({
                {{"name", "game_type"}, {"description", "Game genre: platformer, rpg, puzzle, shooter"}, {"required", true}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string game_type = "platformer";
                if (args.contains("game_type") && args["game_type"].is_string()) {
                    game_type = args["game_type"].get<std::string>();
                }

                std::string text;
                if (game_type == "platformer") {
                    text = "Set up a platformer scene structure in Godot:\n\n"
                           "Root (Node2D) 'Main'\n"
                           "  +-- World (Node2D)\n"
                           "  |   +-- TileMapLayer 'Ground'\n"
                           "  |   +-- Platforms (Node2D) - container for moving platforms\n"
                           "  |   +-- Hazards (Node2D) - spikes, pits\n"
                           "  |   +-- Collectibles (Node2D) - coins, power-ups\n"
                           "  +-- Entities (Node2D)\n"
                           "  |   +-- Player (CharacterBody2D)\n"
                           "  |   +-- Enemies (Node2D) - container for enemy instances\n"
                           "  +-- UI (CanvasLayer)\n"
                           "  |   +-- HUD (Control) - score, health, lives\n"
                           "  |   +-- PauseMenu (Control) - hidden by default\n"
                           "  +-- Camera2D - follows player\n\n"
                           "Naming: PascalCase for nodes, group related nodes under containers.\n"
                           "Use y_sort_enabled on Entities if needed for depth sorting.";
                } else if (game_type == "rpg") {
                    text = "Set up an RPG scene structure in Godot:\n\n"
                           "Root (Node2D) 'Main'\n"
                           "  +-- World (Node2D)\n"
                           "  |   +-- TileMapLayer 'Terrain'\n"
                           "  |   +-- TileMapLayer 'Objects' - trees, rocks, buildings\n"
                           "  |   +-- NPCs (Node2D) - container for NPC instances\n"
                           "  |   +-- Items (Node2D) - dropped items, chests\n"
                           "  +-- Entities (Node2D) y_sort_enabled=true\n"
                           "  |   +-- Player (CharacterBody2D)\n"
                           "  |   +-- Party (Node2D) - party member followers\n"
                           "  +-- UI (CanvasLayer)\n"
                           "  |   +-- HUD (Control) - health, mana, minimap\n"
                           "  |   +-- DialogBox (Control) - NPC dialogue\n"
                           "  |   +-- Inventory (Control) - item management\n"
                           "  |   +-- PauseMenu (Control)\n"
                           "  +-- Camera2D - follows player with smoothing\n\n"
                           "Naming: PascalCase for nodes, use y_sort for proper depth ordering.";
                } else if (game_type == "puzzle") {
                    text = "Set up a puzzle game scene structure in Godot:\n\n"
                           "Root (Node2D) 'Main'\n"
                           "  +-- Board (Node2D)\n"
                           "  |   +-- Grid (Node2D) - the puzzle grid/board\n"
                           "  |   +-- Pieces (Node2D) - container for puzzle pieces\n"
                           "  |   +-- Effects (Node2D) - particles, highlights\n"
                           "  +-- UI (CanvasLayer)\n"
                           "  |   +-- HUD (Control) - score, timer, moves\n"
                           "  |   +-- LevelComplete (Control) - victory screen\n"
                           "  |   +-- PauseMenu (Control)\n"
                           "  +-- GameManager (Node) - game state, scoring logic\n"
                           "  +-- AudioManager (Node) - SFX and music\n\n"
                           "Naming: PascalCase. Keep game logic in GameManager, not in UI nodes.";
                } else if (game_type == "shooter") {
                    text = "Set up a shooter scene structure in Godot:\n\n"
                           "Root (Node3D) 'Main'\n"
                           "  +-- World (Node3D)\n"
                           "  |   +-- Environment (Node3D) - WorldEnvironment, DirectionalLight3D\n"
                           "  |   +-- Level (Node3D) - static geometry, CSG, or imported meshes\n"
                           "  |   +-- SpawnPoints (Node3D) - Marker3D nodes for enemy spawns\n"
                           "  +-- Entities (Node3D)\n"
                           "  |   +-- Player (CharacterBody3D)\n"
                           "  |   +-- Enemies (Node3D) - container for enemy instances\n"
                           "  |   +-- Projectiles (Node3D) - bullets, rockets\n"
                           "  +-- UI (CanvasLayer)\n"
                           "  |   +-- HUD (Control) - crosshair, health, ammo\n"
                           "  |   +-- PauseMenu (Control)\n"
                           "  +-- Camera3D - attached to player or SpringArm3D\n\n"
                           "Naming: PascalCase. Use physics layers to separate player/enemy/world.";
                } else {
                    text = "Set up a " + game_type + " scene structure in Godot:\n\n"
                           "Root node with World, Entities, UI (CanvasLayer) children.\n"
                           "Group related nodes under container nodes.\n"
                           "Use PascalCase naming convention.\n"
                           "Separate visual, logic, and UI layers.";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 3. debug_physics
        {
            "debug_physics",
            "Debug physics issues by inspecting collision shapes, layers, and body properties",
            nlohmann::json::array({
                {{"name", "node_path"}, {"description", "Path to physics node to debug (optional, empty for general guide)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string node_path;
                if (args.contains("node_path") && args["node_path"].is_string()) {
                    node_path = args["node_path"].get<std::string>();
                }

                std::string text = "Physics debugging workflow in Godot";
                if (!node_path.empty()) {
                    text += " for node '" + node_path + "'";
                }
                text += ":\n\n"
                        "1. **Check Collision Shapes:**\n"
                        "   - Verify the node has a CollisionShape2D/3D child\n"
                        "   - Ensure the shape resource is assigned (not null)\n"
                        "   - Check shape dimensions match the visual representation\n\n"
                        "2. **Verify Collision Layers & Masks:**\n"
                        "   - collision_layer: what layer this body IS ON (what others detect it as)\n"
                        "   - collision_mask: what layers this body DETECTS (what it collides with)\n"
                        "   - Common mistake: both bodies need compatible layer/mask pairs\n\n"
                        "3. **Check Body Type:**\n"
                        "   - StaticBody: immovable (walls, floors) -- no movement code needed\n"
                        "   - CharacterBody: game-controlled movement -- use move_and_slide()\n"
                        "   - RigidBody: physics-simulated -- don't set position directly\n"
                        "   - Area: detection only, no physics response\n\n"
                        "4. **Inspect Physics Properties:**\n"
                        "   - RigidBody: check mass, gravity_scale, linear_damp, freeze\n"
                        "   - CharacterBody: check floor_max_angle, up_direction, slide_on_ceiling\n"
                        "   - Verify is_on_floor()/is_on_wall() return expected values\n\n"
                        "5. **Enable Debug Visualization:**\n"
                        "   - Debug > Visible Collision Shapes (in editor)\n"
                        "   - Project Settings > Debug > Shapes > enable visible shapes at runtime\n\n"
                        "6. **Common Issues:**\n"
                        "   - One-way collision not working: check one_way_collision on CollisionShape\n"
                        "   - Falling through floor: ensure matching collision layers\n"
                        "   - Jittery movement: use _physics_process not _process for physics code\n"
                        "   - Tunneling: increase physics ticks or use continuous collision detection";

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 4. create_ui_interface
        {
            "create_ui_interface",
            "Create a UI interface with common elements like health bars, menus, or HUD",
            nlohmann::json::array({
                {{"name", "ui_type"}, {"description", "UI type: hud, main_menu, pause_menu, inventory"}, {"required", true}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string ui_type = "hud";
                if (args.contains("ui_type") && args["ui_type"].is_string()) {
                    ui_type = args["ui_type"].get<std::string>();
                }

                std::string text;
                if (ui_type == "hud") {
                    text = "Create a HUD (heads-up display) in Godot:\n\n"
                           "Scene structure:\n"
                           "  CanvasLayer 'HUD' (layer=10, for always-on-top)\n"
                           "    +-- MarginContainer (anchors: full rect)\n"
                           "        +-- VBoxContainer 'TopBar' (anchors: top)\n"
                           "        |   +-- HBoxContainer\n"
                           "        |       +-- TextureRect 'HealthIcon'\n"
                           "        |       +-- ProgressBar 'HealthBar' (min=0, max=100)\n"
                           "        |       +-- Label 'ScoreLabel'\n"
                           "        +-- VBoxContainer 'BottomBar' (anchors: bottom)\n"
                           "            +-- HBoxContainer 'Hotbar'\n\n"
                           "Setup steps:\n"
                           "1. Create CanvasLayer, set layer to 10\n"
                           "2. Add MarginContainer with full-rect anchors and 16px margins\n"
                           "3. Use ProgressBar for health (stylebox override for custom look)\n"
                           "4. Connect HealthBar.value to player health signal\n"
                           "5. Use theme overrides for consistent font/color across labels";
                } else if (ui_type == "main_menu") {
                    text = "Create a main menu in Godot:\n\n"
                           "Scene structure:\n"
                           "  Control 'MainMenu' (anchors: full rect)\n"
                           "    +-- TextureRect 'Background' (anchors: full rect)\n"
                           "    +-- VBoxContainer 'CenterPanel' (anchors: center)\n"
                           "        +-- Label 'Title' (large font)\n"
                           "        +-- VSeparator (spacing)\n"
                           "        +-- Button 'PlayButton' text='Play'\n"
                           "        +-- Button 'OptionsButton' text='Options'\n"
                           "        +-- Button 'QuitButton' text='Quit'\n\n"
                           "Setup steps:\n"
                           "1. Create root Control with full-rect anchors\n"
                           "2. Center the VBoxContainer using anchor presets\n"
                           "3. Set minimum_size on buttons for consistent sizing\n"
                           "4. Connect button pressed signals to handler methods\n"
                           "5. PlayButton: get_tree().change_scene_to_file('res://scenes/game.tscn')\n"
                           "6. QuitButton: get_tree().quit()\n"
                           "7. Add theme with custom fonts and button styles";
                } else if (ui_type == "pause_menu") {
                    text = "Create a pause menu in Godot:\n\n"
                           "Scene structure:\n"
                           "  CanvasLayer 'PauseLayer' (layer=100)\n"
                           "    +-- Control 'PauseMenu' (anchors: full rect)\n"
                           "        +-- ColorRect 'Overlay' (anchors: full rect, color: #00000080)\n"
                           "        +-- PanelContainer 'Panel' (anchors: center)\n"
                           "            +-- VBoxContainer\n"
                           "                +-- Label 'PausedLabel' text='PAUSED'\n"
                           "                +-- Button 'ResumeButton' text='Resume'\n"
                           "                +-- Button 'OptionsButton' text='Options'\n"
                           "                +-- Button 'MainMenuButton' text='Main Menu'\n\n"
                           "Setup steps:\n"
                           "1. Set process_mode = PROCESS_MODE_ALWAYS on PauseLayer\n"
                           "2. Handle pause toggle in _input: if Input.is_action_just_pressed('ui_cancel')\n"
                           "3. get_tree().paused = true/false to toggle pause\n"
                           "4. PauseMenu.visible controls show/hide\n"
                           "5. ResumeButton: unpause and hide menu\n"
                           "6. MainMenuButton: unpause, then change scene";
                } else if (ui_type == "inventory") {
                    text = "Create an inventory UI in Godot:\n\n"
                           "Scene structure:\n"
                           "  CanvasLayer 'InventoryLayer' (layer=50)\n"
                           "    +-- Control 'InventoryUI' (anchors: full rect)\n"
                           "        +-- PanelContainer 'Panel' (anchors: center)\n"
                           "            +-- VBoxContainer\n"
                           "                +-- Label 'Title' text='Inventory'\n"
                           "                +-- GridContainer 'SlotGrid' (columns=5)\n"
                           "                |   +-- TextureRect 'Slot1..N' (custom_minimum_size: 64x64)\n"
                           "                +-- HBoxContainer 'ItemInfo'\n"
                           "                    +-- Label 'ItemName'\n"
                           "                    +-- Label 'ItemDesc'\n\n"
                           "Setup steps:\n"
                           "1. Create GridContainer with columns=5 (or desired grid width)\n"
                           "2. Each slot: TextureRect (64x64) with Panel stylebox background\n"
                           "3. Use drag-and-drop: implement _get_drag_data, _can_drop_data, _drop_data\n"
                           "4. Toggle visibility with Input.is_action_just_pressed('inventory_toggle')\n"
                           "5. Store item data in an Array/Dictionary resource\n"
                           "6. Update slot textures when inventory changes";
                } else {
                    text = "Create a " + ui_type + " UI interface in Godot:\n\n"
                           "1. Use CanvasLayer for overlay UIs (set appropriate layer number)\n"
                           "2. Use Control nodes with anchor presets for responsive layout\n"
                           "3. Use VBoxContainer/HBoxContainer for automatic layout\n"
                           "4. Connect button signals to handler functions\n"
                           "5. Use Theme resources for consistent styling";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        }
    };
    return defs;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

nlohmann::json get_all_prompts_json() {
    auto result = nlohmann::json::array();
    for (const auto& def : get_prompt_defs()) {
        result.push_back({
            {"name", def.name},
            {"description", def.description},
            {"arguments", def.arguments}
        });
    }
    return result;
}

nlohmann::json get_prompt_messages(const std::string& name, const nlohmann::json& arguments) {
    for (const auto& def : get_prompt_defs()) {
        if (def.name == name) {
            return def.generate(arguments);
        }
    }
    return nullptr;
}

bool prompt_exists(const std::string& name) {
    for (const auto& def : get_prompt_defs()) {
        if (def.name == name) return true;
    }
    return false;
}
