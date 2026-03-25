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
        },

        // 5. tool_composition_guide (PROMPT-01)
        {
            "tool_composition_guide",
            "Quick reference card mapping common game development tasks to MCP tool sequences with parameter examples",
            nlohmann::json::array({
                {{"name", "task_category"}, {"description", "Task category: scene_setup, ui_building, animation, scripting, debugging, testing (default: scene_setup)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string category = "scene_setup";
                if (args.contains("task_category") && args["task_category"].is_string()) {
                    category = args["task_category"].get<std::string>();
                }

                std::string text;
                if (category == "scene_setup") {
                    text = "Tool Composition Guide: Scene Setup\n\n"
                           "=== Create a scene with nodes ===\n"
                           "1. create_scene { \"root_type\": \"Node2D\", \"path\": \"res://scenes/level.tscn\" }\n"
                           "2. create_node { \"type\": \"CharacterBody2D\", \"name\": \"Player\", \"parent\": \"/root/Level\" }\n"
                           "3. set_node_property { \"node_path\": \"/root/Level/Player\", \"property\": \"position\", \"value\": \"Vector2(100, 200)\" }\n"
                           "4. save_scene { \"path\": \"res://scenes/level.tscn\" }\n\n"
                           "=== Instantiate a prefab scene ===\n"
                           "1. instantiate_scene { \"scene_path\": \"res://scenes/enemy.tscn\", \"parent_path\": \"/root/Level/Enemies\", \"name\": \"Goblin\" }\n\n"
                           "=== Bulk property update ===\n"
                           "1. find_nodes { \"name_pattern\": \"Enemy*\", \"type\": \"CharacterBody2D\" }\n"
                           "2. batch_set_property { \"node_paths\": [\"Enemy1\", \"Enemy2\"], \"property\": \"speed\", \"value\": \"150\" }\n\n"
                           "=== Duplicate existing node ===\n"
                           "1. duplicate_node { \"source_path\": \"/root/Level/Player\", \"new_name\": \"Player2\" }";
                } else if (category == "ui_building") {
                    text = "Tool Composition Guide: UI Building\n\n"
                           "=== Create a UI panel (quick) ===\n"
                           "1. create_ui_panel { \"spec\": { \"root_type\": \"VBoxContainer\", \"children\": [{\"type\": \"Label\", \"text\": \"Title\"}, {\"type\": \"Button\", \"text\": \"Play\"}] }, \"parent_path\": \"/root/Scene\" }\n\n"
                           "=== Create a UI from scratch ===\n"
                           "1. create_node { \"type\": \"Control\", \"name\": \"HUD\", \"parent\": \"/root/Scene\" }\n"
                           "2. set_layout_preset { \"node_path\": \"/root/Scene/HUD\", \"preset\": \"full_rect\" }\n"
                           "3. create_stylebox { \"node_path\": \"/root/Scene/HUD\", \"override_name\": \"panel\", \"bg_color\": \"#1a1a2e\", \"corner_radius\": 8 }\n"
                           "4. set_theme_override { \"node_path\": \"/root/Scene/HUD/Title\", \"overrides\": { \"font_size\": 32 } }\n"
                           "5. set_container_layout { \"node_path\": \"/root/Scene/HUD\", \"separation\": 8, \"alignment\": 1 }\n\n"
                           "=== Inspect existing UI ===\n"
                           "1. get_ui_properties { \"node_path\": \"/root/Scene/HUD\" }\n"
                           "2. get_theme_overrides { \"node_path\": \"/root/Scene/HUD/Title\" }";
                } else if (category == "animation") {
                    text = "Tool Composition Guide: Animation\n\n"
                           "=== Create a fade-in animation ===\n"
                           "1. create_animation { \"animation_name\": \"fade_in\", \"parent_path\": \"/root/Scene/UI\" }\n"
                           "2. add_animation_track { \"player_path\": \"/root/Scene/UI/AnimationPlayer\", \"animation_name\": \"fade_in\", "
                              "\"track_type\": \"value\", \"track_path\": \".:modulate\" }\n"
                           "3. set_keyframe { \"player_path\": \"/root/Scene/UI/AnimationPlayer\", \"animation_name\": \"fade_in\", "
                              "\"track_index\": 0, \"time\": 0.0, \"action\": \"set\", \"value\": \"Color(1,1,1,0)\" }\n"
                           "4. set_keyframe { ... \"time\": 0.5, \"value\": \"Color(1,1,1,1)\" }\n"
                           "5. set_animation_properties { \"player_path\": \"/root/Scene/UI/AnimationPlayer\", \"animation_name\": \"fade_in\", "
                              "\"length\": 0.5, \"loop_mode\": 0 }\n\n"
                           "=== Inspect animation ===\n"
                           "1. get_animation_info { \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }";
                } else if (category == "scripting") {
                    text = "Tool Composition Guide: Scripting\n\n"
                           "=== Create and attach a new script ===\n"
                           "1. write_script { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n...\" }\n"
                           "2. attach_script { \"node_path\": \"/root/Scene/Player\", \"script_path\": \"res://scripts/player.gd\" }\n\n"
                           "=== Edit an existing script ===\n"
                           "1. read_script { \"path\": \"res://scripts/player.gd\" }\n"
                           "2. edit_script { \"path\": \"res://scripts/player.gd\", \"edits\": [{\"line\": 5, \"action\": \"replace\", \"text\": \"var speed = 300\"}] }\n\n"
                           "=== Connect a signal in script ===\n"
                           "1. connect_signal { \"source_path\": \"/root/Scene/Button\", \"signal_name\": \"pressed\", "
                              "\"target_path\": \"/root/Scene/Main\", \"method_name\": \"_on_button_pressed\" }\n\n"
                           "=== Detach a script ===\n"
                           "1. detach_script { \"node_path\": \"/root/Scene/Player\" }";
                } else if (category == "debugging") {
                    text = "Tool Composition Guide: Debugging\n\n"
                           "=== Inspect scene structure ===\n"
                           "1. get_scene_tree { \"max_depth\": 5, \"include_properties\": true }\n"
                           "2. find_nodes { \"name_pattern\": \"*Player*\" }\n\n"
                           "=== Check signal connections ===\n"
                           "1. get_node_signals { \"node_path\": \"/root/Scene/Button\" }\n\n"
                           "=== Debug a running game ===\n"
                           "1. get_game_output { \"keyword\": \"error\" }\n"
                           "2. eval_in_game { \"expression\": \"get_node('Player').position\" }\n"
                           "3. capture_game_viewport { \"width\": 800 }\n\n"
                           "=== Check project structure ===\n"
                           "1. list_project_files { \"directory\": \"res://scenes\" }\n"
                           "2. get_project_settings { \"keys\": [\"application/run/main_scene\"] }\n"
                           "3. get_resource_info { \"path\": \"res://scenes/main.tscn\" }";
                } else if (category == "testing") {
                    text = "Tool Composition Guide: Testing\n\n"
                           "=== Manual test flow ===\n"
                           "1. run_game { \"mode\": \"current\" }\n"
                           "2. get_game_bridge_status { }\n"
                           "3. click_node { \"node_path\": \"StartButton\" }\n"
                           "4. get_game_node_property { \"node_path\": \"Player\", \"property\": \"health\" }\n"
                           "5. stop_game { }\n\n"
                           "=== Automated test sequence ===\n"
                           "1. run_game { \"mode\": \"custom\", \"scene_path\": \"res://scenes/test.tscn\" }\n"
                           "2. run_test_sequence { \"steps\": [\n"
                           "     { \"action\": \"click_node\", \"args\": { \"node_path\": \"Button\" }, \"description\": \"Click button\" },\n"
                           "     { \"action\": \"get_game_node_property\", \"args\": { \"node_path\": \"Label\", \"property\": \"text\" },\n"
                           "       \"assert\": { \"property\": \"value\", \"contains\": \"clicked\" }, \"description\": \"Verify label\" }\n"
                           "   ] }\n"
                           "3. stop_game { }\n\n"
                           "=== Visual verification ===\n"
                           "1. run_game { \"mode\": \"main\" }\n"
                           "2. capture_game_viewport { \"width\": 1280 }\n"
                           "3. get_game_scene_tree { \"max_depth\": 3 }\n"
                           "4. stop_game { }";
                } else {
                    text = "Tool Composition Guide: Category Summary\n\n"
                           "Available categories (pass as task_category argument):\n\n"
                           "  scene_setup   - Create scenes, add nodes, set properties, instantiate prefabs\n"
                           "  ui_building   - Build UI layouts with containers, styles, and themes\n"
                           "  animation     - Create animations with tracks, keyframes, and properties\n"
                           "  scripting     - Write/edit scripts, attach to nodes, connect signals\n"
                           "  debugging     - Inspect scene tree, check signals, read game output\n"
                           "  testing       - Run game, click UI, verify state, automated test sequences\n\n"
                           "Each category provides step-by-step tool sequences with parameter examples.";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 9. debug_game_crash (PROMPT-02)
        {
            "debug_game_crash",
            "Systematic workflow for diagnosing game crashes and runtime errors using MCP diagnostic tools",
            nlohmann::json::array({
                {{"name", "error_type"}, {"description", "Error type: crash, null_reference, signal_error, script_error, scene_load_error (default: crash)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string error_type = "crash";
                if (args.contains("error_type") && args["error_type"].is_string()) {
                    error_type = args["error_type"].get<std::string>();
                }

                std::string text;
                if (error_type == "crash") {
                    text = "Debug Game Crash: Systematic Diagnosis\n\n"
                           "Step 1: Read error logs\n"
                           "  Tool: get_game_output\n"
                           "  Parameters: { \"keyword\": \"error\" }\n"
                           "  Look for: stack traces, error messages, last printed output before crash\n\n"
                           "Step 2: Read the crashing script\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<crashing_script>.gd\" }\n"
                           "  Look for: null references, invalid method calls, type mismatches\n\n"
                           "Step 3: Check scene structure\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 5, \"include_properties\": true }\n"
                           "  Look for: missing nodes, wrong node types, detached scripts\n\n"
                           "Step 4: Verify expected nodes exist\n"
                           "  Tool: find_nodes\n"
                           "  Parameters: { \"name_pattern\": \"<expected_node_name>\" }\n"
                           "  Look for: nodes referenced in script that may be missing or renamed\n\n"
                           "Step 5: Check signal connections\n"
                           "  Tool: get_node_signals\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/<node>\" }\n"
                           "  Look for: disconnected signals, signals connected to non-existent methods\n\n"
                           "Step 6: Fix the issue\n"
                           "  Tool: edit_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<script>.gd\", \"edits\": [{\"line\": N, \"action\": \"replace\", \"text\": \"fixed code\"}] }\n\n"
                           "Step 7: Re-test\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"current\" }\n"
                           "  Verify the crash is resolved by checking get_game_output for errors";
                } else if (error_type == "null_reference") {
                    text = "Debug Null Reference Error\n\n"
                           "Step 1: Check scene tree for the node\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 5 }\n"
                           "  Verify the node exists in the scene tree at the expected path\n\n"
                           "Step 2: Search for the node by name\n"
                           "  Tool: find_nodes\n"
                           "  Parameters: { \"name_pattern\": \"<node_name>\" }\n"
                           "  Check if the node exists but at a different path\n\n"
                           "Step 3: Test reference at runtime\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node_or_null('path/to/node') != null\" }\n"
                           "  Verify the node is accessible at runtime (not just editor time)\n\n"
                           "Step 4: Read the script using the reference\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<script>.gd\" }\n"
                           "  Check: @onready var refs, get_node() calls, hardcoded paths\n\n"
                           "Step 5: Fix the path\n"
                           "  Tool: edit_script\n"
                           "  Use correct node path from find_nodes result\n\n"
                           "Step 6: Re-test\n"
                           "  Tool: run_game\n"
                           "  Tool: get_game_output\n"
                           "  Verify no more null reference errors";
                } else if (error_type == "signal_error") {
                    text = "Debug Signal Error\n\n"
                           "Step 1: List signals on the source node\n"
                           "  Tool: get_node_signals\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/<source_node>\" }\n"
                           "  Check: signal exists, current connections, target methods\n\n"
                           "Step 2: Read the target script\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<target>.gd\" }\n"
                           "  Verify: target method exists and has correct signature\n\n"
                           "Step 3: Disconnect broken signal\n"
                           "  Tool: disconnect_signal\n"
                           "  Parameters: { \"source_path\": \"<source>\", \"signal_name\": \"<signal>\", "
                              "\"target_path\": \"<target>\", \"method_name\": \"<old_method>\" }\n\n"
                           "Step 4: Reconnect with correct method\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"<source>\", \"signal_name\": \"<signal>\", "
                              "\"target_path\": \"<target>\", \"method_name\": \"<correct_method>\" }\n\n"
                           "Step 5: Re-test\n"
                           "  Tool: run_game\n"
                           "  Tool: get_game_output\n"
                           "  Verify signal fires correctly without errors";
                } else if (error_type == "script_error") {
                    text = "Debug Script Error\n\n"
                           "Step 1: Read the error output\n"
                           "  Tool: get_game_output\n"
                           "  Parameters: { \"keyword\": \"error\" }\n"
                           "  Note the script path, line number, and error message\n\n"
                           "Step 2: Read the script\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<script>.gd\" }\n"
                           "  Examine the error line and surrounding context\n\n"
                           "Step 3: Fix specific lines\n"
                           "  Tool: edit_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<script>.gd\", \"edits\": [\n"
                           "    { \"line\": <error_line>, \"action\": \"replace\", \"text\": \"corrected code\" }\n"
                           "  ] }\n\n"
                           "Step 4: Re-attach script if needed\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/<node>\", \"script_path\": \"res://scripts/<script>.gd\" }\n\n"
                           "Step 5: Re-test\n"
                           "  Tool: run_game\n"
                           "  Tool: get_game_output\n"
                           "  Verify no more script errors appear";
                } else if (error_type == "scene_load_error") {
                    text = "Debug Scene Load Error\n\n"
                           "Step 1: List project files to verify scene exists\n"
                           "  Tool: list_project_files\n"
                           "  Parameters: { \"directory\": \"res://scenes\" }\n"
                           "  Check: file exists at expected path, correct extension (.tscn/.scn)\n\n"
                           "Step 2: Try opening the scene\n"
                           "  Tool: open_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/<scene>.tscn\" }\n"
                           "  If this fails, scene file may be corrupted or have missing dependencies\n\n"
                           "Step 3: Inspect scene tree once loaded\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3 }\n"
                           "  Check for: missing resources, broken references, invalid node types\n\n"
                           "Step 4: Check listed open scenes\n"
                           "  Tool: list_open_scenes\n"
                           "  Verify which scenes are currently open in the editor\n\n"
                           "Step 5: If scene is broken, recreate it\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/<scene>.tscn\" }\n"
                           "  Then rebuild the scene tree with create_node calls";
                } else {
                    text = "Debug Game Crash: General Guide\n\n"
                           "Available error types (pass as error_type argument):\n"
                           "  crash            - General crash diagnosis workflow\n"
                           "  null_reference   - Node/variable is null at runtime\n"
                           "  signal_error     - Signal connection or emission failures\n"
                           "  script_error     - GDScript syntax or runtime errors\n"
                           "  scene_load_error - Scene file fails to load or is corrupted\n\n"
                           "General debugging tools:\n"
                           "  get_game_output - Read error logs and print output\n"
                           "  read_script     - Examine script source code\n"
                           "  get_scene_tree  - Inspect scene tree structure\n"
                           "  find_nodes      - Search for nodes by name pattern\n"
                           "  eval_in_game    - Evaluate expressions at runtime";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 10. debug_physics_issue (PROMPT-06)
        {
            "debug_physics_issue",
            "Specialized workflow for diagnosing and fixing physics problems: collision failures, movement bugs, layer mismatches",
            nlohmann::json::array({
                {{"name", "issue_type"}, {"description", "Physics issue: no_collision, wrong_movement, falling_through, jitter, one_way_collision, tunneling (default: no_collision)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string issue_type = "no_collision";
                if (args.contains("issue_type") && args["issue_type"].is_string()) {
                    issue_type = args["issue_type"].get<std::string>();
                }

                std::string text;
                if (issue_type == "no_collision") {
                    text = "Debug Physics: No Collision Detected\n\n"
                           "Step 1: Find collision shapes in the scene\n"
                           "  Tool: find_nodes\n"
                           "  Parameters: { \"name_pattern\": \"CollisionShape*\" }\n"
                           "  Check: collision shapes exist for both colliding bodies\n\n"
                           "Step 2: Verify scene tree structure\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 4, \"include_properties\": true }\n"
                           "  Check: CollisionShape is a direct child of a physics body (CharacterBody2D, StaticBody2D, etc.)\n\n"
                           "Step 3: Read collision layer/mask at runtime\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Player').collision_layer\" }\n"
                           "  And: { \"expression\": \"get_node('Player').collision_mask\" }\n"
                           "  Check: layer of body A matches mask of body B and vice versa\n"
                           "  Layer 1 = bit 1 (value 1), Layer 2 = bit 2 (value 2), etc.\n\n"
                           "Step 4: Fix collision layers with batch update\n"
                           "  Tool: batch_set_property\n"
                           "  Parameters: { \"node_paths\": [\"/root/Scene/Player\"], \"property\": \"collision_mask\", \"value\": \"3\" }\n"
                           "  Set mask to detect layers 1 and 2 (bitmask: 1+2=3)\n\n"
                           "Step 5: Verify collision shape has a shape resource\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"Player/CollisionShape2D\", \"property\": \"shape\" }\n"
                           "  If null, the collision shape has no shape assigned\n\n"
                           "Step 6: Re-test\n"
                           "  Tool: run_game\n"
                           "  Verify collisions work correctly";
                } else if (issue_type == "wrong_movement") {
                    text = "Debug Physics: Wrong Movement Behavior\n\n"
                           "Step 1: Check CharacterBody velocity at runtime\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Player').velocity\" }\n"
                           "  Check: velocity vector matches expected direction/magnitude\n\n"
                           "Step 2: Check floor detection\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Player').is_on_floor()\" }\n"
                           "  If false when it should be true: floor collision layers may not match\n\n"
                           "Step 3: Read the movement script\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/player.gd\" }\n"
                           "  Check: _physics_process (not _process), move_and_slide() call, gravity application\n\n"
                           "Step 4: Fix the movement script\n"
                           "  Tool: edit_script\n"
                           "  Common fixes: add gravity, fix direction calculation, use delta correctly\n\n"
                           "Step 5: Re-test movement\n"
                           "  Tool: run_game\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Player').velocity\" }\n"
                           "  Verify velocity and movement behave correctly";
                } else if (issue_type == "falling_through") {
                    text = "Debug Physics: Falling Through Floor\n\n"
                           "Step 1: Verify floor has a collision shape\n"
                           "  Tool: find_nodes\n"
                           "  Parameters: { \"name_pattern\": \"CollisionShape*\" }\n"
                           "  Check: floor/ground node has a CollisionShape2D/3D child\n\n"
                           "Step 2: Check floor body type\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3, \"include_properties\": true }\n"
                           "  Floor should be StaticBody2D/3D (not Area2D which has no collision response)\n\n"
                           "Step 3: Verify layer/mask compatibility\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Floor').collision_layer\" }\n"
                           "  And: { \"expression\": \"get_node('Player').collision_mask\" }\n"
                           "  Player's mask must include Floor's layer\n\n"
                           "Step 4: Fix layers if mismatched\n"
                           "  Tool: batch_set_property\n"
                           "  Parameters: { \"node_paths\": [\"/root/Scene/Floor\"], \"property\": \"collision_layer\", \"value\": \"1\" }\n\n"
                           "Step 5: Re-test\n"
                           "  Tool: run_game\n"
                           "  Verify player stands on the floor correctly";
                } else if (issue_type == "jitter") {
                    text = "Debug Physics: Jittery Movement\n\n"
                           "Step 1: Check process function usage\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/player.gd\" }\n"
                           "  CRITICAL: Physics code must use _physics_process, NOT _process\n"
                           "  _process runs at variable framerate, _physics_process at fixed intervals\n\n"
                           "Step 2: Check delta usage\n"
                           "  In _physics_process, verify movement uses delta:\n"
                           "  velocity.x = direction * speed  (NOT direction * speed * delta for CharacterBody)\n"
                           "  Note: move_and_slide() already accounts for delta internally\n\n"
                           "Step 3: Fix the script\n"
                           "  Tool: edit_script\n"
                           "  Move physics code from _process to _physics_process\n"
                           "  Remove redundant delta multiplication with move_and_slide()\n\n"
                           "Step 4: Check camera smoothing\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Camera2D').position_smoothing_enabled\" }\n"
                           "  Enable smoothing to reduce visual jitter from physics interpolation\n\n"
                           "Step 5: Re-test\n"
                           "  Tool: run_game\n"
                           "  Tool: capture_game_viewport\n"
                           "  Verify smooth movement without jitter";
                } else if (issue_type == "one_way_collision") {
                    text = "Debug Physics: One-Way Collision Not Working\n\n"
                           "Step 1: Check one_way_collision property\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"Platform/CollisionShape2D\", \"property\": \"one_way_collision\" }\n"
                           "  Must be true for one-way platforms\n\n"
                           "Step 2: Verify the CollisionShape setup\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3 }\n"
                           "  Check: CollisionShape2D is child of StaticBody2D or AnimatableBody2D\n\n"
                           "Step 3: Set one_way_collision if not enabled\n"
                           "  Tool: batch_set_property\n"
                           "  Parameters: { \"node_paths\": [\"Platform/CollisionShape2D\"], "
                              "\"property\": \"one_way_collision\", \"value\": \"true\" }\n\n"
                           "Step 4: Check one_way_collision_margin\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"Platform/CollisionShape2D\", \"property\": \"one_way_collision_margin\" }\n"
                           "  Increase if player passes through at high speed (default: 1.0)\n\n"
                           "Step 5: Re-test\n"
                           "  Tool: run_game\n"
                           "  Verify player can jump through from below and land on top";
                } else if (issue_type == "tunneling") {
                    text = "Debug Physics: Tunneling (Objects Passing Through Each Other)\n\n"
                           "Tunneling occurs when fast-moving objects skip past thin collision shapes between frames.\n\n"
                           "Step 1: Check object speed\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Projectile').velocity.length()\" }\n"
                           "  If speed > 500 pixels/frame, tunneling is likely\n\n"
                           "Step 2: Read the movement script\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/projectile.gd\" }\n"
                           "  Check: is it using _physics_process and move_and_slide?\n\n"
                           "Step 3: Solutions:\n"
                           "  a) Enable Continuous Collision Detection (CCD):\n"
                           "     Tool: edit_script\n"
                           "     For RigidBody: set continuous_cd = true in _ready()\n\n"
                           "  b) Increase physics tick rate:\n"
                           "     Tool: get_project_settings\n"
                           "     Parameters: { \"keys\": [\"physics/common/physics_ticks_per_second\"] }\n"
                           "     Default is 60, try 120 for fast objects\n\n"
                           "  c) Use raycasts for hit detection:\n"
                           "     Cast a ray from previous position to current position each frame\n"
                           "     Detect hits even if the body moves past the shape\n\n"
                           "Step 4: Re-test\n"
                           "  Tool: run_game\n"
                           "  Verify fast objects no longer pass through walls";
                } else {
                    text = "Debug Physics Issue: General Guide\n\n"
                           "Available issue types (pass as issue_type argument):\n"
                           "  no_collision       - Two bodies don't collide (layer/mask/shape issues)\n"
                           "  wrong_movement     - Character moves incorrectly (velocity/gravity)\n"
                           "  falling_through     - Objects fall through floors (missing shapes/layers)\n"
                           "  jitter             - Movement is jittery (_process vs _physics_process)\n"
                           "  one_way_collision  - One-way platforms not working\n"
                           "  tunneling          - Fast objects pass through thin walls\n\n"
                           "Key diagnostic tools:\n"
                           "  find_nodes          - Search for collision shapes by name\n"
                           "  get_scene_tree      - Inspect node hierarchy\n"
                           "  eval_in_game        - Read physics properties at runtime\n"
                           "  read_script         - Examine movement code\n"
                           "  batch_set_property  - Fix collision layers/masks in bulk\n"
                           "  get_game_node_property - Check specific node properties";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 11. fix_common_errors (PROMPT-08)
        {
            "fix_common_errors",
            "Recovery guide for common MCP tool errors: node not found, scene not open, script syntax errors, property type mismatches",
            nlohmann::json::array({
                {{"name", "error_pattern"}, {"description", "Error pattern: node_not_found, no_scene_open, script_syntax, type_mismatch, game_not_running, permission_error (default: node_not_found)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string error_pattern = "node_not_found";
                if (args.contains("error_pattern") && args["error_pattern"].is_string()) {
                    error_pattern = args["error_pattern"].get<std::string>();
                }

                std::string text;
                if (error_pattern == "node_not_found") {
                    text = "Fix Common Error: Node Not Found\n\n"
                           "Error message pattern: \"Node not found: <path>\" or \"Invalid node path\"\n\n"
                           "Step 1: See the actual scene tree\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 5 }\n"
                           "  Compare actual paths against the path you used\n\n"
                           "Step 2: Search for the node by name\n"
                           "  Tool: find_nodes\n"
                           "  Parameters: { \"name_pattern\": \"<partial_name>\" }\n"
                           "  The node may exist at a different path or with a different case\n\n"
                           "Step 3: Common path mistakes:\n"
                           "  - Missing /root/ prefix: use \"/root/Scene/Node\" not \"Scene/Node\"\n"
                           "  - Wrong case: node names are case-sensitive in Godot\n"
                           "  - Node not yet added: create it first with create_node\n"
                           "  - Node in different scene: check list_open_scenes\n\n"
                           "Step 4: Retry with the correct path\n"
                           "  Use the path shown by get_scene_tree or find_nodes";
                } else if (error_pattern == "no_scene_open") {
                    text = "Fix Common Error: No Scene Open\n\n"
                           "Error message pattern: \"No scene is currently open\" or \"Scene root is null\"\n\n"
                           "Step 1: Check what scenes are open\n"
                           "  Tool: list_open_scenes\n"
                           "  If empty, no scene is loaded in the editor\n\n"
                           "Step 2a: Open an existing scene\n"
                           "  Tool: open_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/main.tscn\" }\n\n"
                           "Step 2b: Or create a new scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/new_scene.tscn\" }\n\n"
                           "Step 3: Retry the original operation\n"
                           "  Most MCP tools require an open scene with a scene root\n\n"
                           "Common causes:\n"
                           "  - Godot just started and no scene was opened\n"
                           "  - All scene tabs were closed\n"
                           "  - Previous create_scene failed silently";
                } else if (error_pattern == "script_syntax") {
                    text = "Fix Common Error: Script Syntax Error\n\n"
                           "Error message pattern: \"Parse Error\" or \"Unexpected token\" or \"Expected ...\"\n\n"
                           "Step 1: Read the current script content\n"
                           "  Tool: read_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<script>.gd\" }\n"
                           "  Find the syntax error at the reported line number\n\n"
                           "Step 2: Fix the specific error lines\n"
                           "  Tool: edit_script\n"
                           "  Parameters: { \"path\": \"res://scripts/<script>.gd\", \"edits\": [\n"
                           "    { \"line\": <error_line>, \"action\": \"replace\", \"text\": \"fixed_code\" }\n"
                           "  ] }\n\n"
                           "Step 3: Re-attach script if it was detached\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/<node>\", \"script_path\": \"res://scripts/<script>.gd\" }\n\n"
                           "Common GDScript syntax errors:\n"
                           "  - Missing colon after if/for/func: func _ready():  (not func _ready())\n"
                           "  - Wrong indentation: use tabs, not spaces (Godot default)\n"
                           "  - Missing quotes around strings: var name = \"Player\" not var name = Player\n"
                           "  - Using = instead of == in conditions: if x == 5 not if x = 5";
                } else if (error_pattern == "type_mismatch") {
                    text = "Fix Common Error: Property Type Mismatch\n\n"
                           "Error message pattern: \"Invalid value type\" or \"Cannot convert\"\n\n"
                           "Step 1: Check the expected property type\n"
                           "  Tool: get_resource_info\n"
                           "  Parameters: { \"path\": \"res://scenes/<scene>.tscn\" }\n"
                           "  Or inspect the node to see property types\n\n"
                           "Step 2: Common Godot property value formats:\n"
                           "  Vector2:    \"Vector2(100, 200)\"     (not \"100,200\")\n"
                           "  Vector3:    \"Vector3(1, 2, 3)\"      (not \"1,2,3\")\n"
                           "  Color:      \"Color(1, 0, 0, 1)\"     (RGBA 0-1) or \"#ff0000\"\n"
                           "  Rect2:      \"Rect2(0, 0, 100, 50)\"\n"
                           "  NodePath:   \"NodePath('Player')\"    (for NodePath properties)\n"
                           "  bool:       \"true\" or \"false\"       (lowercase)\n"
                           "  int:        \"42\"                     (whole number as string)\n"
                           "  float:      \"3.14\"                   (decimal as string)\n"
                           "  Resource:   \"res://path/to/resource\" (for resource properties)\n\n"
                           "Step 3: Set the property with correct format\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/<node>\", \"property\": \"position\", \"value\": \"Vector2(100, 200)\" }\n\n"
                           "Step 4: For resource properties, use the res:// prefix\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/<node>\", \"property\": \"texture\", \"value\": \"res://assets/sprite.png\" }";
                } else if (error_pattern == "game_not_running") {
                    text = "Fix Common Error: Game Not Running\n\n"
                           "Error message pattern: \"Game is not running\" or \"No game bridge connection\"\n\n"
                           "Step 1: Start the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"current\" }\n"
                           "  Or: { \"mode\": \"main\" } for main scene, { \"mode\": \"custom\", \"scene_path\": \"res://...\" }\n\n"
                           "Step 2: Wait for bridge connection\n"
                           "  Tool: get_game_bridge_status\n"
                           "  The game bridge must be connected before using runtime tools\n"
                           "  run_game auto-waits, but you can also poll get_game_bridge_status\n\n"
                           "Step 3: Retry the runtime tool\n"
                           "  Runtime tools (require game to be running):\n"
                           "  - eval_in_game, get_game_node_property, get_game_scene_tree\n"
                           "  - click_node, inject_input, capture_game_viewport\n"
                           "  - get_game_output, run_test_sequence, get_game_bridge_status\n\n"
                           "Common causes:\n"
                           "  - Game was never started (call run_game first)\n"
                           "  - Game crashed on startup (check get_game_output for errors)\n"
                           "  - Game was stopped (call run_game to restart)";
                } else if (error_pattern == "permission_error") {
                    text = "Fix Common Error: Permission / File Path Error\n\n"
                           "Error message pattern: \"File not found\" or \"Cannot save\" or \"Invalid path\"\n\n"
                           "Step 1: List project files to verify paths\n"
                           "  Tool: list_project_files\n"
                           "  Parameters: { \"directory\": \"res://\" }\n"
                           "  Check that the file exists and path is correct\n\n"
                           "Step 2: Verify the res:// prefix\n"
                           "  All Godot project files must use \"res://\" prefix:\n"
                           "  Correct:   \"res://scripts/player.gd\"\n"
                           "  Incorrect: \"scripts/player.gd\" or \"C:/project/scripts/player.gd\"\n\n"
                           "Step 3: Check file extension\n"
                           "  Scripts:  .gd (GDScript)\n"
                           "  Scenes:   .tscn (text scene) or .scn (binary scene)\n"
                           "  Resources: .tres (text resource) or .res (binary resource)\n\n"
                           "Step 4: Create the file if it doesn't exist\n"
                           "  Tool: write_script (for .gd files)\n"
                           "  Parameters: { \"path\": \"res://scripts/new_script.gd\", \"content\": \"extends Node\\n\" }\n"
                           "  Tool: create_scene (for .tscn files)\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/new_scene.tscn\" }\n\n"
                           "Common causes:\n"
                           "  - Typo in file path\n"
                           "  - Missing directory (Godot auto-creates directories on save)\n"
                           "  - File was deleted or moved outside of Godot";
                } else {
                    text = "Fix Common Errors: Recovery Guide\n\n"
                           "Available error patterns (pass as error_pattern argument):\n"
                           "  node_not_found    - Node path doesn't match scene tree\n"
                           "  no_scene_open     - No scene loaded in editor\n"
                           "  script_syntax     - GDScript parse errors\n"
                           "  type_mismatch     - Wrong property value format\n"
                           "  game_not_running  - Runtime tools need game running\n"
                           "  permission_error  - File path or access issues\n\n"
                           "General recovery steps:\n"
                           "1. get_scene_tree - See current scene state\n"
                           "2. find_nodes - Search for nodes by name\n"
                           "3. list_project_files - Check file existence\n"
                           "4. get_game_output - Read error details from logs";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 12. build_platformer_game (PROMPT-03)
        {
            "build_platformer_game",
            "Complete end-to-end workflow for building a 2D platformer game from empty project to playable prototype using MCP tools",
            nlohmann::json::array({
                {{"name", "complexity"}, {"description", "Complexity level: minimal (player+platforms), standard (enemies+collectibles+HUD), full (animations+sounds+menus) (default: standard)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string complexity = "standard";
                if (args.contains("complexity") && args["complexity"].is_string()) {
                    complexity = args["complexity"].get<std::string>();
                }

                std::string text;
                // Phase 1-4: Core (all complexity levels)
                text = "Build a 2D Platformer Game (" + complexity + " complexity)\n\n"
                       "Phase 1: Scene Setup\n"
                       "  Step 1: Create the main scene\n"
                       "    Tool: create_scene\n"
                       "    Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/game.tscn\" }\n"
                       "  Step 2: Save scene\n"
                       "    Tool: save_scene\n"
                       "    Parameters: { \"path\": \"res://scenes/game.tscn\" }\n\n"

                       "Phase 2: Player Character\n"
                       "  Step 3: Create the player\n"
                       "    Tool: create_character\n"
                       "    Parameters: { \"type\": \"2d\", \"name\": \"Player\", \"parent_path\": \"/root/Game\", \"shape\": \"rectangle\", \"shape_size\": \"Vector2(32, 64)\" }\n"
                       "    Result: CharacterBody2D with CollisionShape2D and Sprite2D\n"
                       "  Step 4: Write player movement script\n"
                       "    Tool: write_script\n"
                       "    Parameters: { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 300.0\\nconst JUMP_VELOCITY = -400.0\\n\\nvar gravity = ProjectSettings.get_setting('physics/2d/default_gravity')\\n\\nfunc _physics_process(delta):\\n\\tif not is_on_floor():\\n\\t\\tvelocity.y += gravity * delta\\n\\tif Input.is_action_just_pressed('ui_accept') and is_on_floor():\\n\\t\\tvelocity.y = JUMP_VELOCITY\\n\\tvar direction = Input.get_axis('ui_left', 'ui_right')\\n\\tvelocity.x = direction * SPEED if direction else move_toward(velocity.x, 0, SPEED)\\n\\tmove_and_slide()\" }\n"
                       "  Step 5: Attach script to player\n"
                       "    Tool: attach_script\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Player\", \"script_path\": \"res://scripts/player.gd\" }\n\n"

                       "Phase 3: Level Geometry\n"
                       "  Step 6: Create ground\n"
                       "    Tool: create_node\n"
                       "    Parameters: { \"type\": \"StaticBody2D\", \"name\": \"Ground\", \"parent\": \"/root/Game\" }\n"
                       "  Step 7: Add ground collision\n"
                       "    Tool: create_collision_shape\n"
                       "    Parameters: { \"parent_path\": \"/root/Game/Ground\", \"shape_type\": \"rectangle\", \"shape_size\": \"Vector2(1200, 40)\" }\n"
                       "  Step 8: Position ground\n"
                       "    Tool: set_node_property\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Ground\", \"property\": \"position\", \"value\": \"Vector2(600, 500)\" }\n"
                       "  Step 9: Add ground visual\n"
                       "    Tool: create_node\n"
                       "    Parameters: { \"type\": \"ColorRect\", \"name\": \"GroundVisual\", \"parent\": \"/root/Game/Ground\" }\n"
                       "  Step 10: Add platforms using additional StaticBody2D nodes\n"
                       "    Tool: create_node\n"
                       "    Parameters: { \"type\": \"StaticBody2D\", \"name\": \"Platform1\", \"parent\": \"/root/Game\" }\n"
                       "    Tool: create_collision_shape\n"
                       "    Parameters: { \"parent_path\": \"/root/Game/Platform1\", \"shape_type\": \"rectangle\", \"shape_size\": \"Vector2(200, 20)\" }\n"
                       "    Tool: set_node_property\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Platform1\", \"property\": \"position\", \"value\": \"Vector2(300, 350)\" }\n"
                       "    Repeat with duplicate_node for more platforms:\n"
                       "    Tool: duplicate_node\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Platform1\", \"new_name\": \"Platform2\" }\n\n"

                       "Phase 4: Camera\n"
                       "  Step 11: Add camera to player\n"
                       "    Tool: create_node\n"
                       "    Parameters: { \"type\": \"Camera2D\", \"name\": \"Camera\", \"parent\": \"/root/Game/Player\" }\n"
                       "  Step 12: Configure camera\n"
                       "    Tool: set_node_property\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Player/Camera\", \"property\": \"position_smoothing_enabled\", \"value\": \"true\" }\n"
                       "    Tool: set_node_property\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Player/Camera\", \"property\": \"limit_bottom\", \"value\": \"600\" }\n";

                if (complexity == "standard" || complexity == "full") {
                    text += "\nPhase 5: Enemies & Collectibles\n"
                            "  Step 13: Create an enemy\n"
                            "    Tool: create_character\n"
                            "    Parameters: { \"type\": \"2d\", \"name\": \"Enemy\", \"parent_path\": \"/root/Game\", \"shape\": \"rectangle\", \"shape_size\": \"Vector2(32, 32)\" }\n"
                            "  Step 14: Write enemy patrol script\n"
                            "    Tool: write_script\n"
                            "    Parameters: { \"path\": \"res://scripts/enemy.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 100.0\\nvar direction = 1\\n\\nfunc _physics_process(delta):\\n\\tvelocity.x = direction * SPEED\\n\\tmove_and_slide()\\n\\tif is_on_wall():\\n\\t\\tdirection *= -1\" }\n"
                            "  Step 15: Attach enemy script\n"
                            "    Tool: attach_script\n"
                            "    Parameters: { \"node_path\": \"/root/Game/Enemy\", \"script_path\": \"res://scripts/enemy.gd\" }\n"
                            "  Step 16: Create collectible\n"
                            "    Tool: create_node\n"
                            "    Parameters: { \"type\": \"Area2D\", \"name\": \"Coin\", \"parent\": \"/root/Game\" }\n"
                            "    Tool: create_collision_shape\n"
                            "    Parameters: { \"parent_path\": \"/root/Game/Coin\", \"shape_type\": \"circle\", \"radius\": 16 }\n"
                            "  Step 17: Connect collectible signal\n"
                            "    Tool: connect_signal\n"
                            "    Parameters: { \"source_path\": \"/root/Game/Coin\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/Coin\", \"method_name\": \"_on_body_entered\" }\n\n"

                            "Phase 6: HUD\n"
                            "  Step 18: Create HUD panel\n"
                            "    Tool: create_ui_panel\n"
                            "    Parameters: { \"root_type\": \"HBoxContainer\", \"name\": \"HUD\", \"parent_path\": \"/root/Game\", \"children\": [{\"type\": \"Label\", \"name\": \"ScoreLabel\"}, {\"type\": \"Label\", \"name\": \"HealthLabel\"}] }\n"
                            "  Step 19: Set HUD layout\n"
                            "    Tool: set_layout_preset\n"
                            "    Parameters: { \"node_path\": \"/root/Game/HUD\", \"preset\": \"top_wide\" }\n"
                            "  Step 20: Style HUD text\n"
                            "    Tool: set_theme_override\n"
                            "    Parameters: { \"node_path\": \"/root/Game/HUD/ScoreLabel\", \"overrides\": { \"font_size\": 24 } }\n";
                }

                if (complexity == "full") {
                    text += "\nPhase 7: Animations\n"
                            "  Step 21: Create walk animation\n"
                            "    Tool: create_animation\n"
                            "    Parameters: { \"animation_name\": \"walk\", \"parent_path\": \"/root/Game/Player\" }\n"
                            "  Step 22: Add walk animation track\n"
                            "    Tool: add_animation_track\n"
                            "    Parameters: { \"player_path\": \"/root/Game/Player/AnimationPlayer\", \"animation_name\": \"walk\", \"track_type\": \"value\", \"track_path\": \"Sprite2D:frame\" }\n"
                            "  Step 23: Set walk keyframes\n"
                            "    Tool: set_keyframe\n"
                            "    Parameters: { \"player_path\": \"/root/Game/Player/AnimationPlayer\", \"animation_name\": \"walk\", \"track_index\": 0, \"time\": 0.0, \"action\": \"set\", \"value\": \"0\" }\n"
                            "    Tool: set_keyframe\n"
                            "    Parameters: { ... \"time\": 0.25, \"value\": \"1\" }\n"
                            "  Step 24: Create idle and jump animations (repeat pattern)\n"
                            "    Tool: create_animation\n"
                            "    Parameters: { \"animation_name\": \"idle\", \"parent_path\": \"/root/Game/Player\" }\n"
                            "    Tool: create_animation\n"
                            "    Parameters: { \"animation_name\": \"jump\", \"parent_path\": \"/root/Game/Player\" }\n";
                }

                text += "\nPhase 8: Test & Iterate\n"
                        "  Step: Run the game\n"
                        "    Tool: run_game\n"
                        "    Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                        "  Step: Capture screenshot\n"
                        "    Tool: capture_game_viewport\n"
                        "    Parameters: { \"width\": 1280 }\n"
                        "  Step: Test player movement\n"
                        "    Tool: inject_input\n"
                        "    Parameters: { \"type\": \"key\", \"keycode\": \"right\", \"pressed\": true }\n"
                        "  Step: Stop game\n"
                        "    Tool: stop_game\n"
                        "    Parameters: { }";

                if (complexity != "minimal" && complexity != "standard" && complexity != "full") {
                    text = "Build a 2D Platformer Game: Complexity Options\n\n"
                           "Available complexity levels (pass as complexity argument):\n"
                           "  minimal  - Player + platforms only (Phases 1-4, 8)\n"
                           "  standard - Adds enemies, collectibles, HUD (Phases 1-6, 8) [default]\n"
                           "  full     - Adds animations for walk/idle/jump cycles (all phases)\n\n"
                           "Key tools used: create_scene, create_character, create_collision_shape, "
                           "write_script, attach_script, create_node, set_node_property, "
                           "duplicate_node, connect_signal, create_ui_panel, set_layout_preset, "
                           "create_animation, run_game, capture_game_viewport, inject_input, stop_game";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 13. setup_tilemap_level (PROMPT-04)
        {
            "setup_tilemap_level",
            "Step-by-step workflow for creating a TileMap-based level: tileset setup, painting terrain, adding collision, and level boundaries",
            nlohmann::json::array({
                {{"name", "level_type"}, {"description", "Level type: platformer_level, top_down_level, dungeon (default: platformer_level)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string level_type = "platformer_level";
                if (args.contains("level_type") && args["level_type"].is_string()) {
                    level_type = args["level_type"].get<std::string>();
                }

                std::string text;
                if (level_type == "platformer_level") {
                    text = "Setup TileMap Level: Platformer\n\n"
                           "Step 1: Check current scene structure\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3 }\n"
                           "  Result: See existing nodes to decide where to add TileMapLayer\n\n"
                           "Step 2: Create TileMapLayer node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TileMapLayer\", \"name\": \"TerrainLayer\", \"parent\": \"/root/Level\" }\n"
                           "  Note: You need a TileSet resource configured with at least one source. Use get_tilemap_info to check.\n\n"
                           "Step 3: Verify TileMap setup\n"
                           "  Tool: get_tilemap_info\n"
                           "  Parameters: { \"node_path\": \"/root/Level/TerrainLayer\" }\n"
                           "  Result: Shows tile_set info, source_id values, atlas_coords available\n"
                           "  Note: source_id and atlas_coords depend on your TileSet configuration\n\n"
                           "Step 4: Paint ground row (floor)\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/TerrainLayer\", \"cells\": [\n"
                           "    { \"x\": 0, \"y\": 10 }, { \"x\": 1, \"y\": 10 }, { \"x\": 2, \"y\": 10 },\n"
                           "    { \"x\": 3, \"y\": 10 }, { \"x\": 4, \"y\": 10 }, { \"x\": 5, \"y\": 10 },\n"
                           "    ... { \"x\": 19, \"y\": 10 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                           "  Result: Ground tiles painted across row y=10 (x=0..19)\n\n"
                           "Step 5: Paint elevated platforms\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/TerrainLayer\", \"cells\": [\n"
                           "    { \"x\": 5, \"y\": 7 }, { \"x\": 6, \"y\": 7 }, { \"x\": 7, \"y\": 7 },\n"
                           "    { \"x\": 12, \"y\": 5 }, { \"x\": 13, \"y\": 5 }, { \"x\": 14, \"y\": 5 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                           "  Result: Two floating platforms at different heights\n\n"
                           "Step 6: Paint walls/boundaries\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/TerrainLayer\", \"cells\": [\n"
                           "    { \"x\": 0, \"y\": 0 }, { \"x\": 0, \"y\": 1 }, ... { \"x\": 0, \"y\": 10 },\n"
                           "    { \"x\": 19, \"y\": 0 }, { \"x\": 19, \"y\": 1 }, ... { \"x\": 19, \"y\": 10 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                           "  Result: Left and right boundary walls\n\n"
                           "Step 7: Create gaps/pits by erasing tiles\n"
                           "  Tool: erase_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/TerrainLayer\", \"cells\": [\n"
                           "    { \"x\": 8, \"y\": 10 }, { \"x\": 9, \"y\": 10 }\n"
                           "  ] }\n"
                           "  Result: Two-tile gap in the ground (pit hazard)\n\n"
                           "Step 8: Verify painted cells\n"
                           "  Tool: get_tilemap_cell_info\n"
                           "  Parameters: { \"node_path\": \"/root/Level/TerrainLayer\", \"x\": 5, \"y\": 7 }\n"
                           "  Result: Confirms tile is placed at the expected position\n\n"
                           "Step 9: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/level.tscn\" }";
                } else if (level_type == "top_down_level") {
                    text = "Setup TileMap Level: Top-Down\n\n"
                           "Step 1: Check current scene structure\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3 }\n\n"
                           "Step 2: Create TileMapLayer node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TileMapLayer\", \"name\": \"FloorLayer\", \"parent\": \"/root/Level\" }\n\n"
                           "Step 3: Verify TileMap setup\n"
                           "  Tool: get_tilemap_info\n"
                           "  Parameters: { \"node_path\": \"/root/Level/FloorLayer\" }\n"
                           "  Note: Check source_id and atlas_coords from your TileSet\n\n"
                           "Step 4: Paint full floor area\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/FloorLayer\", \"cells\": [\n"
                           "    { \"x\": 0, \"y\": 0 }, { \"x\": 1, \"y\": 0 }, ... { \"x\": 19, \"y\": 0 },\n"
                           "    { \"x\": 0, \"y\": 1 }, ... { \"x\": 19, \"y\": 14 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                           "  Result: Full floor coverage (20x15 grid)\n\n"
                           "Step 5: Paint wall perimeter\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/FloorLayer\", \"cells\": [\n"
                           "    // Top row: y=0, x=0..19\n"
                           "    // Bottom row: y=14, x=0..19\n"
                           "    // Left column: x=0, y=0..14\n"
                           "    // Right column: x=19, y=0..14\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(1, 0)\" }\n"
                           "  Result: Wall border around the level (use different atlas coords for wall tiles)\n\n"
                           "Step 6: Paint interior obstacles\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/FloorLayer\", \"cells\": [\n"
                           "    { \"x\": 5, \"y\": 5 }, { \"x\": 5, \"y\": 6 }, { \"x\": 6, \"y\": 5 }, { \"x\": 6, \"y\": 6 },\n"
                           "    { \"x\": 12, \"y\": 8 }, { \"x\": 13, \"y\": 8 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(1, 0)\" }\n"
                           "  Result: Interior wall clusters blocking direct paths\n\n"
                           "Step 7: Create paths between areas by erasing obstacle tiles\n"
                           "  Tool: erase_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/FloorLayer\", \"cells\": [\n"
                           "    { \"x\": 5, \"y\": 7 }\n"
                           "  ] }\n"
                           "  Result: Opening in obstacle wall for player passage\n\n"
                           "Step 8: Verify cell placement\n"
                           "  Tool: get_tilemap_cell_info\n"
                           "  Parameters: { \"node_path\": \"/root/Level/FloorLayer\", \"x\": 5, \"y\": 5 }\n\n"
                           "Step 9: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/level.tscn\" }";
                } else if (level_type == "dungeon") {
                    text = "Setup TileMap Level: Dungeon\n\n"
                           "Step 1: Check current scene structure\n"
                           "  Tool: get_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3 }\n\n"
                           "Step 2: Create TileMapLayer node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TileMapLayer\", \"name\": \"DungeonLayer\", \"parent\": \"/root/Level\" }\n\n"
                           "Step 3: Verify TileMap setup\n"
                           "  Tool: get_tilemap_info\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\" }\n\n"
                           "Step 4: Paint Room 1 (rectangular fill)\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\", \"cells\": [\n"
                           "    // Room at (2,2)-(7,6): fill floor tiles for interior\n"
                           "    { \"x\": 3, \"y\": 3 }, { \"x\": 4, \"y\": 3 }, { \"x\": 5, \"y\": 3 }, { \"x\": 6, \"y\": 3 },\n"
                           "    { \"x\": 3, \"y\": 4 }, ... { \"x\": 6, \"y\": 5 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                           "  Result: First dungeon room (floor tiles)\n\n"
                           "Step 5: Paint Room 1 walls (perimeter of room)\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\", \"cells\": [\n"
                           "    // Walls: x=2 and x=7 for y=2..6, y=2 and y=6 for x=2..7\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(1, 0)\" }\n\n"
                           "Step 6: Paint Room 2 (second room at offset position)\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\", \"cells\": [\n"
                           "    // Room at (12,2)-(17,6): floor tiles\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n\n"
                           "Step 7: Paint corridor connecting rooms (1-tile-wide path)\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\", \"cells\": [\n"
                           "    { \"x\": 7, \"y\": 4 }, { \"x\": 8, \"y\": 4 }, { \"x\": 9, \"y\": 4 },\n"
                           "    { \"x\": 10, \"y\": 4 }, { \"x\": 11, \"y\": 4 }, { \"x\": 12, \"y\": 4 }\n"
                           "  ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                           "  Result: 1-tile-wide corridor between Room 1 and Room 2\n\n"
                           "Step 8: Create door as single-tile gap in room wall\n"
                           "  Tool: erase_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\", \"cells\": [\n"
                           "    { \"x\": 7, \"y\": 4 }, { \"x\": 12, \"y\": 4 }\n"
                           "  ] }\n"
                           "  Result: Single-tile doorways at corridor-room junctions\n\n"
                           "Step 9: Verify dungeon layout\n"
                           "  Tool: get_tilemap_cell_info\n"
                           "  Parameters: { \"node_path\": \"/root/Level/DungeonLayer\", \"x\": 8, \"y\": 4 }\n"
                           "  Confirm corridor tile is placed\n\n"
                           "Step 10: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/dungeon.tscn\" }";
                } else {
                    text = "Setup TileMap Level: Type Options\n\n"
                           "Available level types (pass as level_type argument):\n"
                           "  platformer_level - Ground row + floating platforms + walls + gaps/pits\n"
                           "  top_down_level   - Full floor + wall perimeter + interior obstacles + paths\n"
                           "  dungeon          - Rooms as rectangular fills + corridors + door gaps\n\n"
                           "Key tools used: create_node, get_tilemap_info, set_tilemap_cells, "
                           "erase_tilemap_cells, get_tilemap_cell_info, get_scene_tree, save_scene\n\n"
                           "TileSet Notes:\n"
                           "  source_id: index of the TileSetSource in the TileSet (usually 0)\n"
                           "  atlas_coords: Vector2i coordinates in the tileset atlas\n"
                           "  Use get_tilemap_info to discover available source_id and atlas_coords";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 14. build_top_down_game (PROMPT-05)
        {
            "build_top_down_game",
            "Complete end-to-end workflow for building a 2D top-down game from empty project to playable prototype using MCP tools",
            nlohmann::json::array({
                {{"name", "genre"}, {"description", "Sub-genre: adventure, shooter, rpg (default: adventure)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string genre = "adventure";
                if (args.contains("genre") && args["genre"].is_string()) {
                    genre = args["genre"].get<std::string>();
                }

                std::string text;
                // Phases 1-4: Common to all genres
                text = "Build a 2D Top-Down Game (" + genre + " genre)\n\n"
                       "Phase 1: Scene Setup\n"
                       "  Step 1: Create main scene\n"
                       "    Tool: create_scene\n"
                       "    Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/game.tscn\" }\n"
                       "  Step 2: Save scene\n"
                       "    Tool: save_scene\n"
                       "    Parameters: { \"path\": \"res://scenes/game.tscn\" }\n\n"

                       "Phase 2: Player Character\n"
                       "  Step 3: Create the player\n"
                       "    Tool: create_character\n"
                       "    Parameters: { \"type\": \"2d\", \"name\": \"Player\", \"parent_path\": \"/root/Game\", \"shape\": \"circle\", \"radius\": 16 }\n"
                       "    Result: CharacterBody2D with circular collision shape\n"
                       "  Step 4: Write top-down movement script\n"
                       "    Tool: write_script\n"
                       "    Parameters: { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 200.0\\n\\nfunc _physics_process(delta):\\n\\tvar input_dir = Input.get_vector('ui_left', 'ui_right', 'ui_up', 'ui_down')\\n\\tvelocity = input_dir * SPEED\\n\\tmove_and_slide()\" }\n"
                       "  Step 5: Attach script\n"
                       "    Tool: attach_script\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Player\", \"script_path\": \"res://scripts/player.gd\" }\n\n"

                       "Phase 3: World / Terrain\n"
                       "  Step 6: Create terrain TileMapLayer\n"
                       "    Tool: create_node\n"
                       "    Parameters: { \"type\": \"TileMapLayer\", \"name\": \"Terrain\", \"parent\": \"/root/Game\" }\n"
                       "  Step 7: Paint ground tiles\n"
                       "    Tool: set_tilemap_cells\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Terrain\", \"cells\": [\n"
                       "      // Fill 20x15 area with floor tiles\n"
                       "      { \"x\": 0, \"y\": 0 }, ... { \"x\": 19, \"y\": 14 }\n"
                       "    ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n"
                       "  Step 8: Paint walls/obstacles\n"
                       "    Tool: set_tilemap_cells\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Terrain\", \"cells\": [\n"
                       "      // Wall tiles for obstacles and boundaries\n"
                       "    ], \"source_id\": 0, \"atlas_coords\": \"Vector2i(1, 0)\" }\n\n"

                       "Phase 4: Camera\n"
                       "  Step 9: Add camera\n"
                       "    Tool: create_node\n"
                       "    Parameters: { \"type\": \"Camera2D\", \"name\": \"Camera\", \"parent\": \"/root/Game/Player\" }\n"
                       "  Step 10: Enable camera smoothing\n"
                       "    Tool: set_node_property\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Player/Camera\", \"property\": \"position_smoothing_enabled\", \"value\": \"true\" }\n"
                       "    Tool: set_node_property\n"
                       "    Parameters: { \"node_path\": \"/root/Game/Player/Camera\", \"property\": \"position_smoothing_speed\", \"value\": \"5.0\" }\n";

                // Phase 5: Genre-specific interaction
                if (genre == "adventure") {
                    text += "\nPhase 5: NPCs & Interaction (Adventure)\n"
                            "  Step 11: Create NPC\n"
                            "    Tool: create_node\n"
                            "    Parameters: { \"type\": \"Area2D\", \"name\": \"NPC\", \"parent\": \"/root/Game\" }\n"
                            "    Tool: create_collision_shape\n"
                            "    Parameters: { \"parent_path\": \"/root/Game/NPC\", \"shape_type\": \"circle\", \"radius\": 24 }\n"
                            "  Step 12: Add NPC visual\n"
                            "    Tool: create_node\n"
                            "    Parameters: { \"type\": \"Sprite2D\", \"name\": \"NPCSprite\", \"parent\": \"/root/Game/NPC\" }\n"
                            "  Step 13: Connect interaction signal\n"
                            "    Tool: connect_signal\n"
                            "    Parameters: { \"source_path\": \"/root/Game/NPC\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/NPC\", \"method_name\": \"_on_body_entered\" }\n"
                            "  Step 14: Write NPC interaction script\n"
                            "    Tool: write_script\n"
                            "    Parameters: { \"path\": \"res://scripts/npc.gd\", \"content\": \"extends Area2D\\n\\nfunc _on_body_entered(body):\\n\\tif body.name == 'Player':\\n\\t\\tprint('NPC: Hello traveler!')\" }\n"
                            "    Tool: attach_script\n"
                            "    Parameters: { \"node_path\": \"/root/Game/NPC\", \"script_path\": \"res://scripts/npc.gd\" }\n";
                } else if (genre == "shooter") {
                    text += "\nPhase 5: Projectiles & Combat (Shooter)\n"
                            "  Step 11: Write bullet script\n"
                            "    Tool: write_script\n"
                            "    Parameters: { \"path\": \"res://scripts/bullet.gd\", \"content\": \"extends Area2D\\n\\nconst SPEED = 500.0\\nvar direction = Vector2.RIGHT\\n\\nfunc _physics_process(delta):\\n\\tposition += direction * SPEED * delta\\n\\nfunc _on_body_entered(body):\\n\\tif body.has_method('take_damage'):\\n\\t\\tbody.take_damage(10)\\n\\tqueue_free()\" }\n"
                            "  Step 12: Update player script with shooting\n"
                            "    Tool: write_script\n"
                            "    Parameters: { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 200.0\\nvar bullet_scene = preload('res://scenes/bullet.tscn')\\n\\nfunc _physics_process(delta):\\n\\tvar input_dir = Input.get_vector('ui_left', 'ui_right', 'ui_up', 'ui_down')\\n\\tvelocity = input_dir * SPEED\\n\\tmove_and_slide()\\n\\nfunc _input(event):\\n\\tif event.is_action_pressed('ui_accept'):\\n\\t\\tvar bullet = bullet_scene.instantiate()\\n\\t\\tbullet.position = position\\n\\t\\tget_parent().add_child(bullet)\" }\n"
                            "  Step 13: Create enemy with Area2D for hit detection\n"
                            "    Tool: create_character\n"
                            "    Parameters: { \"type\": \"2d\", \"name\": \"Enemy\", \"parent_path\": \"/root/Game\", \"shape\": \"circle\", \"radius\": 16 }\n"
                            "  Step 14: Connect enemy hit signal\n"
                            "    Tool: connect_signal\n"
                            "    Parameters: { \"source_path\": \"/root/Game/Enemy\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/Enemy\", \"method_name\": \"_on_body_entered\" }\n";
                } else if (genre == "rpg") {
                    text += "\nPhase 5: NPCs & Items (RPG)\n"
                            "  Step 11: Create NPC\n"
                            "    Tool: create_node\n"
                            "    Parameters: { \"type\": \"Area2D\", \"name\": \"NPC\", \"parent\": \"/root/Game\" }\n"
                            "    Tool: create_collision_shape\n"
                            "    Parameters: { \"parent_path\": \"/root/Game/NPC\", \"shape_type\": \"circle\", \"radius\": 24 }\n"
                            "  Step 12: Connect NPC interaction\n"
                            "    Tool: connect_signal\n"
                            "    Parameters: { \"source_path\": \"/root/Game/NPC\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/NPC\", \"method_name\": \"_on_body_entered\" }\n"
                            "  Step 13: Create collectable item\n"
                            "    Tool: create_node\n"
                            "    Parameters: { \"type\": \"Area2D\", \"name\": \"HealthPotion\", \"parent\": \"/root/Game\" }\n"
                            "    Tool: create_collision_shape\n"
                            "    Parameters: { \"parent_path\": \"/root/Game/HealthPotion\", \"shape_type\": \"circle\", \"radius\": 12 }\n"
                            "  Step 14: Connect item pickup signal\n"
                            "    Tool: connect_signal\n"
                            "    Parameters: { \"source_path\": \"/root/Game/HealthPotion\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/HealthPotion\", \"method_name\": \"_on_body_entered\" }\n";
                }

                // Phase 6: HUD (all genres)
                text += "\nPhase 6: HUD\n"
                        "  Step 15: Create HUD\n"
                        "    Tool: create_ui_panel\n"
                        "    Parameters: { \"root_type\": \"HBoxContainer\", \"name\": \"HUD\", \"parent_path\": \"/root/Game\", \"children\": ["
                        "{\"type\": \"Label\", \"name\": \"HealthLabel\"}, "
                        "{\"type\": \"Label\", \"name\": \"ScoreLabel\"}] }\n"
                        "  Step 16: Set HUD layout\n"
                        "    Tool: set_layout_preset\n"
                        "    Parameters: { \"node_path\": \"/root/Game/HUD\", \"preset\": \"top_wide\" }\n"
                        "  Step 17: Style HUD\n"
                        "    Tool: set_theme_override\n"
                        "    Parameters: { \"node_path\": \"/root/Game/HUD/HealthLabel\", \"overrides\": { \"font_size\": 20 } }\n";

                // Phase 7: Test (all genres)
                text += "\nPhase 7: Test & Iterate\n"
                        "  Step 18: Run the game\n"
                        "    Tool: run_game\n"
                        "    Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                        "  Step 19: Capture screenshot\n"
                        "    Tool: capture_game_viewport\n"
                        "    Parameters: { \"width\": 1280 }\n"
                        "  Step 20: Test movement\n"
                        "    Tool: inject_input\n"
                        "    Parameters: { \"type\": \"key\", \"keycode\": \"right\", \"pressed\": true }\n"
                        "  Step 21: Stop game\n"
                        "    Tool: stop_game\n"
                        "    Parameters: { }";

                if (genre != "adventure" && genre != "shooter" && genre != "rpg") {
                    text = "Build a 2D Top-Down Game: Genre Options\n\n"
                           "Available genres (pass as genre argument):\n"
                           "  adventure - NPCs with interaction zones (Area2D + signals)\n"
                           "  shooter   - Projectile spawning, aim mechanics, enemies\n"
                           "  rpg       - NPCs + collectable items + dialog potential\n\n"
                           "Key tools used: create_scene, create_character, write_script, attach_script, "
                           "create_node, set_tilemap_cells, create_collision_shape, connect_signal, "
                           "create_ui_panel, set_layout_preset, run_game, capture_game_viewport, stop_game";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 15. create_game_from_scratch (PROMPT-07)
        {
            "create_game_from_scratch",
            "Parameterized full game creation guide: specify a genre and get a complete workflow from project setup to playable prototype",
            nlohmann::json::array({
                {{"name", "genre"}, {"description", "Game genre: platformer, top_down, puzzle, shooter, visual_novel"}, {"required", true}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string genre = "";
                if (args.contains("genre") && args["genre"].is_string()) {
                    genre = args["genre"].get<std::string>();
                }

                std::string text;
                if (genre == "platformer") {
                    text = "Create Game From Scratch: Platformer\n\n"
                           "Step 1: Create the main scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/game.tscn\" }\n\n"
                           "Step 2: Create player character with collision\n"
                           "  Tool: create_character\n"
                           "  Parameters: { \"type\": \"2d\", \"name\": \"Player\", \"parent_path\": \"/root/Game\", \"shape\": \"rectangle\", \"shape_size\": \"Vector2(32, 64)\" }\n\n"
                           "Step 3: Write platformer movement script (gravity + jump + horizontal)\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 300.0\\nconst JUMP_VELOCITY = -400.0\\nvar gravity = ProjectSettings.get_setting('physics/2d/default_gravity')\\n\\nfunc _physics_process(delta):\\n\\tif not is_on_floor():\\n\\t\\tvelocity.y += gravity * delta\\n\\tif Input.is_action_just_pressed('ui_accept') and is_on_floor():\\n\\t\\tvelocity.y = JUMP_VELOCITY\\n\\tvar dir = Input.get_axis('ui_left', 'ui_right')\\n\\tvelocity.x = dir * SPEED if dir else move_toward(velocity.x, 0, SPEED)\\n\\tmove_and_slide()\" }\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Player\", \"script_path\": \"res://scripts/player.gd\" }\n\n"
                           "Step 4: Create ground with collision\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"StaticBody2D\", \"name\": \"Ground\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: create_collision_shape\n"
                           "  Parameters: { \"parent_path\": \"/root/Game/Ground\", \"shape_type\": \"rectangle\", \"shape_size\": \"Vector2(1200, 40)\" }\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Ground\", \"property\": \"position\", \"value\": \"Vector2(600, 500)\" }\n\n"
                           "Step 5: Add platforms\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"StaticBody2D\", \"name\": \"Platform\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: create_collision_shape\n"
                           "  Parameters: { \"parent_path\": \"/root/Game/Platform\", \"shape_type\": \"rectangle\", \"shape_size\": \"Vector2(200, 20)\" }\n"
                           "  Tool: duplicate_node\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Platform\", \"new_name\": \"Platform2\" }\n\n"
                           "Step 6: Add camera\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Camera2D\", \"name\": \"Camera\", \"parent\": \"/root/Game/Player\" }\n\n"
                           "Step 7: Create enemies\n"
                           "  Tool: create_character\n"
                           "  Parameters: { \"type\": \"2d\", \"name\": \"Enemy\", \"parent_path\": \"/root/Game\" }\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/enemy.gd\", \"content\": \"extends CharacterBody2D\\n...\" }\n\n"
                           "Step 8: Create collectibles with signals\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Area2D\", \"name\": \"Coin\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/Coin\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/Coin\", \"method_name\": \"_on_body_entered\" }\n\n"
                           "Step 9: Build HUD\n"
                           "  Tool: create_ui_panel\n"
                           "  Parameters: { \"root_type\": \"HBoxContainer\", \"name\": \"HUD\", \"parent_path\": \"/root/Game\", \"children\": [{\"type\": \"Label\", \"name\": \"Score\"}, {\"type\": \"Label\", \"name\": \"Health\"}] }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/HUD\", \"preset\": \"top_wide\" }\n\n"
                           "Step 10: Test the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                           "  Tool: capture_game_viewport\n"
                           "  Parameters: { \"width\": 1280 }\n"
                           "  Tool: stop_game\n"
                           "  Parameters: { }\n\n"
                           "Step 11: Save scene\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/game.tscn\" }";
                } else if (genre == "top_down") {
                    text = "Create Game From Scratch: Top-Down\n\n"
                           "Step 1: Create main scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/game.tscn\" }\n\n"
                           "Step 2: Create player\n"
                           "  Tool: create_character\n"
                           "  Parameters: { \"type\": \"2d\", \"name\": \"Player\", \"parent_path\": \"/root/Game\", \"shape\": \"circle\", \"radius\": 16 }\n\n"
                           "Step 3: Write 8-direction movement script\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 200.0\\n\\nfunc _physics_process(delta):\\n\\tvar input_dir = Input.get_vector('ui_left', 'ui_right', 'ui_up', 'ui_down')\\n\\tvelocity = input_dir * SPEED\\n\\tmove_and_slide()\" }\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Player\", \"script_path\": \"res://scripts/player.gd\" }\n\n"
                           "Step 4: Create world with TileMap\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TileMapLayer\", \"name\": \"Terrain\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: set_tilemap_cells\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Terrain\", \"cells\": [...], \"source_id\": 0, \"atlas_coords\": \"Vector2i(0, 0)\" }\n\n"
                           "Step 5: Add camera with smoothing\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Camera2D\", \"name\": \"Camera\", \"parent\": \"/root/Game/Player\" }\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Player/Camera\", \"property\": \"position_smoothing_enabled\", \"value\": \"true\" }\n\n"
                           "Step 6: Create NPCs with interaction\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Area2D\", \"name\": \"NPC\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: create_collision_shape\n"
                           "  Parameters: { \"parent_path\": \"/root/Game/NPC\", \"shape_type\": \"circle\", \"radius\": 24 }\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/NPC\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/NPC\", \"method_name\": \"_on_body_entered\" }\n\n"
                           "Step 7: Create items\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Area2D\", \"name\": \"Item\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/Item\", \"signal_name\": \"body_entered\", \"target_path\": \"/root/Game/Item\", \"method_name\": \"_on_body_entered\" }\n\n"
                           "Step 8: Build HUD\n"
                           "  Tool: create_ui_panel\n"
                           "  Parameters: { \"root_type\": \"HBoxContainer\", \"name\": \"HUD\", \"parent_path\": \"/root/Game\", \"children\": [{\"type\": \"Label\", \"name\": \"Health\"}, {\"type\": \"Label\", \"name\": \"Inventory\"}] }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/HUD\", \"preset\": \"top_wide\" }\n\n"
                           "Step 9: Test the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                           "  Tool: capture_game_viewport\n"
                           "  Parameters: { \"width\": 1280 }\n"
                           "  Tool: stop_game\n\n"
                           "Step 10: Save\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/game.tscn\" }";
                } else if (genre == "puzzle") {
                    text = "Create Game From Scratch: Puzzle\n\n"
                           "Step 1: Create main scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/game.tscn\" }\n\n"
                           "Step 2: Create puzzle board container\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"GridContainer\", \"name\": \"Board\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Board\", \"property\": \"columns\", \"value\": \"4\" }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Board\", \"preset\": \"center\" }\n\n"
                           "Step 3: Create puzzle cells\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"ColorRect\", \"name\": \"Cell0\", \"parent\": \"/root/Game/Board\" }\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Board/Cell0\", \"property\": \"custom_minimum_size\", \"value\": \"Vector2(64, 64)\" }\n"
                           "  Repeat for all cells or use duplicate_node\n\n"
                           "Step 4: Write game logic script\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/game_logic.gd\", \"content\": \"extends Node2D\\n\\nvar score = 0\\nvar moves = 0\\n\\nfunc _ready():\\n\\tsetup_board()\\n\\nfunc setup_board():\\n\\t# Initialize puzzle grid state\\n\\tpass\\n\\nfunc check_match(pos1, pos2):\\n\\t# Check for matching tiles, swap, score\\n\\tpass\\n\\nfunc update_score(points):\\n\\tscore += points\\n\\t$HUD/ScoreLabel.text = 'Score: ' + str(score)\" }\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Game\", \"script_path\": \"res://scripts/game_logic.gd\" }\n\n"
                           "Step 5: Create HUD (score + moves)\n"
                           "  Tool: create_ui_panel\n"
                           "  Parameters: { \"root_type\": \"HBoxContainer\", \"name\": \"HUD\", \"parent_path\": \"/root/Game\", \"children\": [{\"type\": \"Label\", \"name\": \"ScoreLabel\"}, {\"type\": \"Label\", \"name\": \"MovesLabel\"}] }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/HUD\", \"preset\": \"top_wide\" }\n"
                           "  Tool: set_theme_override\n"
                           "  Parameters: { \"node_path\": \"/root/Game/HUD/ScoreLabel\", \"overrides\": { \"font_size\": 24 } }\n\n"
                           "Step 6: Add match/clear animation\n"
                           "  Tool: create_animation\n"
                           "  Parameters: { \"animation_name\": \"match_effect\", \"parent_path\": \"/root/Game\" }\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"player_path\": \"/root/Game/AnimationPlayer\", \"animation_name\": \"match_effect\", \"track_type\": \"value\", \"track_path\": \"Board:modulate\" }\n"
                           "  Tool: set_keyframe\n"
                           "  Parameters: { \"player_path\": \"/root/Game/AnimationPlayer\", \"animation_name\": \"match_effect\", \"track_index\": 0, \"time\": 0.0, \"action\": \"set\", \"value\": \"Color(1,1,1,1)\" }\n\n"
                           "Step 7: Connect cell click signals\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/Board/Cell0\", \"signal_name\": \"gui_input\", \"target_path\": \"/root/Game\", \"method_name\": \"_on_cell_input\" }\n\n"
                           "Step 8: Test the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                           "  Tool: capture_game_viewport\n"
                           "  Parameters: { \"width\": 800 }\n"
                           "  Tool: stop_game\n\n"
                           "Step 9: Save\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/game.tscn\" }";
                } else if (genre == "shooter") {
                    text = "Create Game From Scratch: Shooter (Top-Down)\n\n"
                           "Step 1: Create main scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Node2D\", \"path\": \"res://scenes/game.tscn\" }\n\n"
                           "Step 2: Create player with collision\n"
                           "  Tool: create_character\n"
                           "  Parameters: { \"type\": \"2d\", \"name\": \"Player\", \"parent_path\": \"/root/Game\", \"shape\": \"circle\", \"radius\": 16 }\n\n"
                           "Step 3: Write player script (move + aim + shoot)\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/player.gd\", \"content\": \"extends CharacterBody2D\\n\\nconst SPEED = 250.0\\nvar bullet_scene = preload('res://scenes/bullet.tscn')\\n\\nfunc _physics_process(delta):\\n\\tvar input_dir = Input.get_vector('ui_left', 'ui_right', 'ui_up', 'ui_down')\\n\\tvelocity = input_dir * SPEED\\n\\tmove_and_slide()\\n\\nfunc _input(event):\\n\\tif event.is_action_pressed('ui_accept'):\\n\\t\\tvar bullet = bullet_scene.instantiate()\\n\\t\\tbullet.position = position\\n\\t\\tbullet.direction = (get_global_mouse_position() - position).normalized()\\n\\t\\tget_parent().add_child(bullet)\" }\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Player\", \"script_path\": \"res://scripts/player.gd\" }\n\n"
                           "Step 4: Create bullet scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Area2D\", \"path\": \"res://scenes/bullet.tscn\" }\n"
                           "  Tool: create_collision_shape\n"
                           "  Parameters: { \"parent_path\": \"/root/Bullet\", \"shape_type\": \"circle\", \"radius\": 4 }\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/bullet.gd\", \"content\": \"extends Area2D\\n\\nconst SPEED = 500.0\\nvar direction = Vector2.RIGHT\\n\\nfunc _physics_process(delta):\\n\\tposition += direction * SPEED * delta\\n\\nfunc _on_body_entered(body):\\n\\tif body.has_method('take_damage'):\\n\\t\\tbody.take_damage(10)\\n\\tqueue_free()\" }\n\n"
                           "Step 5: Create enemies with hit detection\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Area2D\", \"name\": \"Enemy\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: create_collision_shape\n"
                           "  Parameters: { \"parent_path\": \"/root/Game/Enemy\", \"shape_type\": \"circle\", \"radius\": 16 }\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/Enemy\", \"signal_name\": \"area_entered\", \"target_path\": \"/root/Game/Enemy\", \"method_name\": \"_on_area_entered\" }\n\n"
                           "Step 6: Write enemy script\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/enemy.gd\", \"content\": \"extends Area2D\\n\\nvar health = 30\\n\\nfunc take_damage(amount):\\n\\thealth -= amount\\n\\tif health <= 0:\\n\\t\\tqueue_free()\" }\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Enemy\", \"script_path\": \"res://scripts/enemy.gd\" }\n\n"
                           "Step 7: Create HUD (health + ammo)\n"
                           "  Tool: create_ui_panel\n"
                           "  Parameters: { \"root_type\": \"HBoxContainer\", \"name\": \"HUD\", \"parent_path\": \"/root/Game\", \"children\": [{\"type\": \"Label\", \"name\": \"HealthLabel\"}, {\"type\": \"Label\", \"name\": \"AmmoLabel\"}] }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/HUD\", \"preset\": \"top_wide\" }\n\n"
                           "Step 8: Add crosshair\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TextureRect\", \"name\": \"Crosshair\", \"parent\": \"/root/Game/HUD\" }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/HUD/Crosshair\", \"preset\": \"center\" }\n\n"
                           "Step 9: Add camera\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Camera2D\", \"name\": \"Camera\", \"parent\": \"/root/Game/Player\" }\n\n"
                           "Step 10: Test the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                           "  Tool: capture_game_viewport\n"
                           "  Parameters: { \"width\": 1280 }\n"
                           "  Tool: stop_game\n\n"
                           "Step 11: Save\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/game.tscn\" }";
                } else if (genre == "visual_novel") {
                    text = "Create Game From Scratch: Visual Novel\n\n"
                           "Step 1: Create main scene\n"
                           "  Tool: create_scene\n"
                           "  Parameters: { \"root_type\": \"Control\", \"path\": \"res://scenes/game.tscn\" }\n\n"
                           "Step 2: Create dialog box layout\n"
                           "  Tool: create_ui_panel\n"
                           "  Parameters: { \"root_type\": \"VBoxContainer\", \"name\": \"DialogBox\", \"parent_path\": \"/root/Game\", \"children\": ["
                           "{\"type\": \"Label\", \"name\": \"SpeakerName\"}, "
                           "{\"type\": \"RichTextLabel\", \"name\": \"DialogText\"}, "
                           "{\"type\": \"HBoxContainer\", \"name\": \"Choices\"}] }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/DialogBox\", \"preset\": \"bottom_wide\" }\n\n"
                           "Step 3: Style dialog box\n"
                           "  Tool: create_stylebox\n"
                           "  Parameters: { \"node_path\": \"/root/Game/DialogBox\", \"override_name\": \"panel\", \"bg_color\": \"#1a1a2ecc\", \"corner_radius\": 12, \"content_margin\": 16 }\n"
                           "  Tool: set_theme_override\n"
                           "  Parameters: { \"node_path\": \"/root/Game/DialogBox/SpeakerName\", \"overrides\": { \"font_size\": 20, \"font_color\": \"#ffcc00\" } }\n"
                           "  Tool: set_theme_override\n"
                           "  Parameters: { \"node_path\": \"/root/Game/DialogBox/DialogText\", \"overrides\": { \"font_size\": 16 } }\n\n"
                           "Step 4: Write dialog system script\n"
                           "  Tool: write_script\n"
                           "  Parameters: { \"path\": \"res://scripts/dialog_system.gd\", \"content\": \"extends Control\\n\\nvar dialog_data = [\\n\\t{\\\"speaker\\\": \\\"Alice\\\", \\\"text\\\": \\\"Hello, welcome to the story!\\\"},\\n\\t{\\\"speaker\\\": \\\"Bob\\\", \\\"text\\\": \\\"Nice to meet you.\\\", \\\"choices\\\": [\\\"Be friendly\\\", \\\"Be rude\\\"]}\\n]\\nvar current_line = 0\\n\\nfunc _ready():\\n\\tshow_line(0)\\n\\nfunc show_line(index):\\n\\tif index >= dialog_data.size():\\n\\t\\treturn\\n\\tcurrent_line = index\\n\\tvar line = dialog_data[index]\\n\\t$DialogBox/SpeakerName.text = line.speaker\\n\\t$DialogBox/DialogText.text = line.text\\n\\tif line.has('choices'):\\n\\t\\tshow_choices(line.choices)\\n\\nfunc show_choices(choices):\\n\\tfor c in choices:\\n\\t\\tvar btn = Button.new()\\n\\t\\tbtn.text = c\\n\\t\\t$DialogBox/Choices.add_child(btn)\\n\\nfunc _input(event):\\n\\tif event.is_action_pressed('ui_accept'):\\n\\t\\tshow_line(current_line + 1)\" }\n"
                           "  Tool: attach_script\n"
                           "  Parameters: { \"node_path\": \"/root/Game\", \"script_path\": \"res://scripts/dialog_system.gd\" }\n\n"
                           "Step 5: Create character portrait area\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TextureRect\", \"name\": \"CharacterPortrait\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/CharacterPortrait\", \"preset\": \"center_left\" }\n"
                           "  Tool: set_node_property\n"
                           "  Parameters: { \"node_path\": \"/root/Game/CharacterPortrait\", \"property\": \"custom_minimum_size\", \"value\": \"Vector2(300, 400)\" }\n\n"
                           "Step 6: Create choice buttons with signals\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Button\", \"name\": \"Choice1\", \"parent\": \"/root/Game/DialogBox/Choices\" }\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Button\", \"name\": \"Choice2\", \"parent\": \"/root/Game/DialogBox/Choices\" }\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/DialogBox/Choices/Choice1\", \"signal_name\": \"pressed\", \"target_path\": \"/root/Game\", \"method_name\": \"_on_choice_selected\" }\n"
                           "  Tool: connect_signal\n"
                           "  Parameters: { \"source_path\": \"/root/Game/DialogBox/Choices/Choice2\", \"signal_name\": \"pressed\", \"target_path\": \"/root/Game\", \"method_name\": \"_on_choice_selected\" }\n\n"
                           "Step 7: Create background\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TextureRect\", \"name\": \"Background\", \"parent\": \"/root/Game\" }\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Game/Background\", \"preset\": \"full_rect\" }\n\n"
                           "Step 8: Test the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/game.tscn\" }\n"
                           "  Tool: capture_game_viewport\n"
                           "  Parameters: { \"width\": 1280 }\n"
                           "  Tool: stop_game\n\n"
                           "Step 9: Save\n"
                           "  Tool: save_scene\n"
                           "  Parameters: { \"path\": \"res://scenes/game.tscn\" }";
                } else {
                    text = "Create Game From Scratch: Genre Guide\n\n"
                           "Available genres (pass as genre argument):\n\n"
                           "  platformer    - 2D side-scrolling with gravity, jumping, platforms\n"
                           "                  Key tools: create_character, create_collision_shape, write_script\n\n"
                           "  top_down      - 2D top-down with 8-direction movement, NPCs, items\n"
                           "                  Key tools: create_character, set_tilemap_cells, connect_signal\n\n"
                           "  puzzle        - Grid-based puzzle with match/swap mechanics\n"
                           "                  Key tools: create_node (GridContainer), write_script, create_animation\n\n"
                           "  shooter       - Top-down shooter with projectiles, enemies, aiming\n"
                           "                  Key tools: create_character, create_collision_shape, connect_signal\n\n"
                           "  visual_novel  - Dialog-driven story with character portraits and choices\n"
                           "                  Key tools: create_ui_panel, write_script, connect_signal\n\n"
                           "Each genre provides 8-12 numbered steps with exact tool names and parameter JSON.\n"
                           "All tools referenced are from the MCP tool registry.";
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
