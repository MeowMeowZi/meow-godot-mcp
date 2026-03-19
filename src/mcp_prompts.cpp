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

        // 5. build_ui_layout (PMPT-01)
        {
            "build_ui_layout",
            "Step-by-step workflow for building a complete UI layout in Godot using MCP tools",
            nlohmann::json::array({
                {{"name", "layout_type"}, {"description", "UI layout type: main_menu, hud, settings, inventory, dialog (default: main_menu)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string layout_type = "main_menu";
                if (args.contains("layout_type") && args["layout_type"].is_string()) {
                    layout_type = args["layout_type"].get<std::string>();
                }

                std::string text;
                if (layout_type == "main_menu") {
                    text = "Build a Main Menu UI layout in Godot using MCP tools:\n\n"
                           "Step 1: Create the root Control node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Control\", \"name\": \"MainMenu\", \"parent\": \"/root/Scene\" }\n"
                           "  Result: Root Control node for the menu\n\n"
                           "Step 2: Set full-rect layout on root\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/MainMenu\", \"preset\": \"full_rect\" }\n"
                           "  Result: MainMenu fills the entire screen\n\n"
                           "Step 3: Create a centered VBoxContainer for menu items\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"VBoxContainer\", \"name\": \"MenuContainer\", \"parent\": \"/root/Scene/MainMenu\" }\n"
                           "  Then: set_layout_preset with preset \"center\"\n\n"
                           "Step 4: Add a title Label\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Label\", \"name\": \"TitleLabel\", \"parent\": \"/root/Scene/MainMenu/MenuContainer\" }\n"
                           "  Then: set_theme_override with overrides { \"font_size\": 48 }\n\n"
                           "Step 5: Add menu buttons\n"
                           "  Tool: create_node (repeated for each button)\n"
                           "  Create Button nodes: \"PlayButton\", \"SettingsButton\", \"QuitButton\"\n"
                           "  Parent: /root/Scene/MainMenu/MenuContainer\n\n"
                           "Step 6: Style with a custom StyleBox\n"
                           "  Tool: create_stylebox\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/MainMenu/MenuContainer\", \"override_name\": \"panel\", \"bg_color\": \"#2a2a3e\", \"corner_radius\": 8, \"content_margin\": 20 }\n"
                           "  Result: Container gets a dark rounded background\n\n"
                           "Step 7: Configure container layout\n"
                           "  Tool: set_container_layout\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/MainMenu/MenuContainer\", \"separation\": 12, \"alignment\": 1 }\n"
                           "  Result: Menu items are centered with 12px spacing\n\n"
                           "Step 8: Verify the layout\n"
                           "  Tool: get_ui_properties\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/MainMenu\" }\n"
                           "  Result: Confirm anchors, size, and children are correct\n\n"
                           "Step 9: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Result: Scene saved to disk";
                } else if (layout_type == "hud") {
                    text = "Build a HUD UI layout in Godot using MCP tools:\n\n"
                           "Step 1: Create the root MarginContainer node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"MarginContainer\", \"name\": \"HUD\", \"parent\": \"/root/Scene\" }\n"
                           "  Result: Root container for the HUD overlay\n\n"
                           "Step 2: Set full-rect layout on root\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/HUD\", \"preset\": \"full_rect\" }\n"
                           "  Result: HUD fills the entire screen\n\n"
                           "Step 3: Create a top bar HBoxContainer\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"HBoxContainer\", \"name\": \"TopBar\", \"parent\": \"/root/Scene/HUD\" }\n"
                           "  Then: set_layout_preset with preset \"top_wide\"\n\n"
                           "Step 4: Add a ProgressBar for health\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"ProgressBar\", \"name\": \"HealthBar\", \"parent\": \"/root/Scene/HUD/TopBar\" }\n"
                           "  Result: Health bar in the top bar\n\n"
                           "Step 5: Style the ProgressBar with a custom StyleBox\n"
                           "  Tool: create_stylebox\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/HUD/TopBar/HealthBar\", \"override_name\": \"fill\", \"bg_color\": \"#ff3333\", \"corner_radius\": 4 }\n"
                           "  Result: Custom red health bar appearance\n\n"
                           "Step 6: Add a score Label\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"Label\", \"name\": \"ScoreLabel\", \"parent\": \"/root/Scene/HUD/TopBar\" }\n"
                           "  Then: set_theme_override with overrides { \"font_size\": 24 }\n\n"
                           "Step 7: Configure container layout\n"
                           "  Tool: set_container_layout\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/HUD/TopBar\", \"separation\": 16 }\n"
                           "  Result: Top bar items spaced evenly\n\n"
                           "Step 8: Verify the layout\n"
                           "  Tool: get_ui_properties\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/HUD\" }\n"
                           "  Result: Confirm anchors, size, and children are correct\n\n"
                           "Step 9: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Result: Scene saved to disk";
                } else if (layout_type == "settings") {
                    text = "Build a Settings UI layout in Godot using MCP tools:\n\n"
                           "Step 1: Create the root TabContainer node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"TabContainer\", \"name\": \"Settings\", \"parent\": \"/root/Scene\" }\n"
                           "  Result: Root tabbed container for settings sections\n\n"
                           "Step 2: Set full-rect layout on root\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Settings\", \"preset\": \"full_rect\" }\n"
                           "  Result: Settings fills the entire screen\n\n"
                           "Step 3: Create Audio section\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"VBoxContainer\", \"name\": \"Audio\", \"parent\": \"/root/Scene/Settings\" }\n"
                           "  Add HSlider for master volume, HSlider for SFX, HSlider for music\n\n"
                           "Step 4: Create Video section\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"VBoxContainer\", \"name\": \"Video\", \"parent\": \"/root/Scene/Settings\" }\n"
                           "  Add OptionButton for resolution, CheckButton for fullscreen, HSlider for brightness\n\n"
                           "Step 5: Create Controls section\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"VBoxContainer\", \"name\": \"Controls\", \"parent\": \"/root/Scene/Settings\" }\n"
                           "  Add relevant input remapping controls\n\n"
                           "Step 6: Style with theme overrides\n"
                           "  Tool: set_theme_override\n"
                           "  Apply consistent font sizes and colors across all sections\n"
                           "  Tool: create_stylebox\n"
                           "  Add panel backgrounds to each section for visual separation\n\n"
                           "Step 7: Configure container layout\n"
                           "  Tool: set_container_layout\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Settings/Audio\", \"separation\": 10 }\n"
                           "  Apply consistent spacing to all tab sections\n\n"
                           "Step 8: Verify the layout\n"
                           "  Tool: get_ui_properties\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Settings\" }\n"
                           "  Result: Confirm tabs, children, and layout are correct\n\n"
                           "Step 9: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Result: Scene saved to disk";
                } else if (layout_type == "inventory") {
                    text = "Build an Inventory UI layout in Godot using MCP tools:\n\n"
                           "Step 1: Create the root PanelContainer node\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"PanelContainer\", \"name\": \"Inventory\", \"parent\": \"/root/Scene\" }\n"
                           "  Result: Root panel for the inventory\n\n"
                           "Step 2: Set center layout preset\n"
                           "  Tool: set_layout_preset\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Inventory\", \"preset\": \"center\" }\n"
                           "  Result: Inventory panel centered on screen\n\n"
                           "Step 3: Create a GridContainer for item slots\n"
                           "  Tool: create_node\n"
                           "  Parameters: { \"type\": \"GridContainer\", \"name\": \"ItemGrid\", \"parent\": \"/root/Scene/Inventory\" }\n"
                           "  Result: Grid layout with columns=5 for inventory slots\n\n"
                           "Step 4: Add TextureRect slots\n"
                           "  Tool: create_node (repeated for each slot)\n"
                           "  Create TextureRect nodes as item slots inside the GridContainer\n"
                           "  Set custom_minimum_size to 64x64 for each slot\n\n"
                           "Step 5: Style the inventory panel\n"
                           "  Tool: create_stylebox\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Inventory\", \"override_name\": \"panel\", \"bg_color\": \"#1a1a2e\", \"corner_radius\": 12, \"content_margin\": 16 }\n"
                           "  Result: Dark themed inventory panel\n\n"
                           "Step 6: Apply theme overrides\n"
                           "  Tool: set_theme_override\n"
                           "  Apply consistent styling to slot labels and item counts\n\n"
                           "Step 7: Configure grid layout\n"
                           "  Tool: set_container_layout\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Inventory/ItemGrid\", \"columns\": 5, \"separation\": 4 }\n"
                           "  Result: 5-column grid with 4px spacing between slots\n\n"
                           "Step 8: Verify the layout\n"
                           "  Tool: get_ui_properties\n"
                           "  Parameters: { \"node_path\": \"/root/Scene/Inventory\" }\n"
                           "  Result: Confirm grid, slots, and styling are correct\n\n"
                           "Step 9: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Result: Scene saved to disk";
                } else {
                    text = "Build a " + layout_type + " UI layout in Godot using MCP tools:\n\n"
                           "Step 1: Create the root Control node\n"
                           "  Tool: create_node\n"
                           "  Create the main container node for your " + layout_type + " layout\n\n"
                           "Step 2: Set layout preset\n"
                           "  Tool: set_layout_preset\n"
                           "  Configure anchors and sizing for the root container\n\n"
                           "Step 3: Build child nodes\n"
                           "  Tool: create_node (repeated for each UI element)\n"
                           "  Add containers (VBoxContainer, HBoxContainer, GridContainer) and control nodes\n\n"
                           "Step 4: Apply theme overrides\n"
                           "  Tool: set_theme_override\n"
                           "  Set font sizes, colors, and other theme properties on controls\n\n"
                           "Step 5: Style with custom StyleBoxes\n"
                           "  Tool: create_stylebox\n"
                           "  Add background panels, borders, and rounded corners\n\n"
                           "Step 6: Configure container layout\n"
                           "  Tool: set_container_layout\n"
                           "  Set separation, alignment, and container-specific properties\n\n"
                           "Step 7: Verify the layout\n"
                           "  Tool: get_ui_properties\n"
                           "  Confirm anchors, sizes, and children are correct\n\n"
                           "Step 8: Save the scene\n"
                           "  Tool: save_scene\n"
                           "  Save the completed UI layout to disk";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 6. setup_animation (PMPT-02)
        {
            "setup_animation",
            "Step-by-step workflow for creating a complete animation in Godot using MCP tools",
            nlohmann::json::array({
                {{"name", "animation_type"}, {"description", "Animation type: walk_cycle, ui_transition, idle, attack, fade_in (default: ui_transition)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string animation_type = "ui_transition";
                if (args.contains("animation_type") && args["animation_type"].is_string()) {
                    animation_type = args["animation_type"].get<std::string>();
                }

                std::string text;
                if (animation_type == "ui_transition") {
                    text = "Set up a UI Transition animation in Godot using MCP tools:\n\n"
                           "Step 1: Create an AnimationPlayer node\n"
                           "  Tool: create_animation\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"parent_path\": \"/root/Scene/UI\" }\n"
                           "  Result: AnimationPlayer with AnimationLibrary containing \"fade_in\" animation\n\n"
                           "Step 2: Add a track for the modulate alpha property\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"track_type\": \"value\", \"node_path\": \".:modulate\", \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: Value track targeting the UI node's modulate property\n\n"
                           "Step 3: Set the starting keyframe (fully transparent)\n"
                           "  Tool: set_keyframe\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"track_index\": 0, \"time\": 0.0, \"value\": \"Color(1, 1, 1, 0)\", \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: At t=0, the node is fully transparent\n\n"
                           "Step 4: Set the ending keyframe (fully visible)\n"
                           "  Tool: set_keyframe\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"track_index\": 0, \"time\": 0.5, \"value\": \"Color(1, 1, 1, 1)\", \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: At t=0.5s, the node is fully opaque\n\n"
                           "Step 5: Configure animation properties\n"
                           "  Tool: set_animation_properties\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"duration\": 0.5, \"loop_mode\": 0, \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: Animation is 0.5s long, plays once (no loop)\n\n"
                           "Step 6: Verify the animation\n"
                           "  Tool: get_animation_info\n"
                           "  Parameters: { \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: Confirm animation list, tracks, keyframes, and properties";
                } else if (animation_type == "walk_cycle") {
                    text = "Set up a Walk Cycle animation in Godot using MCP tools:\n\n"
                           "Step 1: Create an AnimationPlayer node\n"
                           "  Tool: create_animation\n"
                           "  Parameters: { \"animation_name\": \"walk\", \"parent_path\": \"/root/Scene/Player\" }\n"
                           "  Result: AnimationPlayer with AnimationLibrary containing \"walk\" animation\n\n"
                           "Step 2: Add a position track for bobbing motion\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"walk\", \"track_type\": \"value\", \"node_path\": \".:position\", \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Value track for position changes during walk\n\n"
                           "Step 3: Add a rotation track for body sway\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"walk\", \"track_type\": \"value\", \"node_path\": \".:rotation\", \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Value track for rotation during walk\n\n"
                           "Step 4: Set keyframes for walk cycle positions\n"
                           "  Tool: set_keyframe\n"
                           "  Set keyframes at t=0.0, 0.25, 0.5, 0.75, 1.0 for a complete loop cycle\n"
                           "  Start and end positions must match for seamless loop\n\n"
                           "Step 5: Configure animation properties for loop\n"
                           "  Tool: set_animation_properties\n"
                           "  Parameters: { \"animation_name\": \"walk\", \"duration\": 1.0, \"loop_mode\": 1, \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Animation is 1.0s long, set to loop continuously\n\n"
                           "Step 6: Verify the animation\n"
                           "  Tool: get_animation_info\n"
                           "  Parameters: { \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Confirm tracks, keyframes, duration, and loop mode are correct";
                } else if (animation_type == "idle") {
                    text = "Set up an Idle animation in Godot using MCP tools:\n\n"
                           "Step 1: Create an AnimationPlayer node\n"
                           "  Tool: create_animation\n"
                           "  Parameters: { \"animation_name\": \"idle\", \"parent_path\": \"/root/Scene/Player\" }\n"
                           "  Result: AnimationPlayer with AnimationLibrary containing \"idle\" animation\n\n"
                           "Step 2: Add a position track for gentle bobbing\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"idle\", \"track_type\": \"value\", \"node_path\": \".:position\", \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Value track for subtle position changes\n\n"
                           "Step 3: Set keyframes for idle bob\n"
                           "  Tool: set_keyframe\n"
                           "  Set keyframes at t=0.0 (origin), t=1.0 (slight up), t=2.0 (origin) for smooth bob\n"
                           "  Use small Y offset (e.g., 2-3 pixels) for subtle breathing effect\n\n"
                           "Step 4: Configure animation properties for loop\n"
                           "  Tool: set_animation_properties\n"
                           "  Parameters: { \"animation_name\": \"idle\", \"duration\": 2.0, \"loop_mode\": 1, \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Animation is 2.0s long, set to loop continuously\n\n"
                           "Step 5: Verify the animation\n"
                           "  Tool: get_animation_info\n"
                           "  Parameters: { \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Confirm track, keyframes, duration, and loop mode are correct";
                } else if (animation_type == "attack") {
                    text = "Set up an Attack animation in Godot using MCP tools:\n\n"
                           "Step 1: Create an AnimationPlayer node\n"
                           "  Tool: create_animation\n"
                           "  Parameters: { \"animation_name\": \"attack\", \"parent_path\": \"/root/Scene/Player\" }\n"
                           "  Result: AnimationPlayer with AnimationLibrary containing \"attack\" animation\n\n"
                           "Step 2: Add a scale track for attack lunge\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"attack\", \"track_type\": \"value\", \"node_path\": \".:scale\", \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Value track for scale burst during attack\n\n"
                           "Step 3: Add a modulate track for flash effect\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"attack\", \"track_type\": \"value\", \"node_path\": \".:modulate\", \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Value track for color flash on attack\n\n"
                           "Step 4: Set keyframes for quick attack burst\n"
                           "  Tool: set_keyframe\n"
                           "  Scale: t=0.0 (1,1), t=0.1 (1.2,1.2), t=0.3 (1,1) for quick punch\n"
                           "  Modulate: t=0.0 (white), t=0.05 (red flash), t=0.3 (white) for hit feedback\n\n"
                           "Step 5: Configure animation properties (no loop)\n"
                           "  Tool: set_animation_properties\n"
                           "  Parameters: { \"animation_name\": \"attack\", \"duration\": 0.3, \"loop_mode\": 0, \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Animation is 0.3s long, plays once (no loop)\n\n"
                           "Step 6: Verify the animation\n"
                           "  Tool: get_animation_info\n"
                           "  Parameters: { \"player_path\": \"/root/Scene/Player/AnimationPlayer\" }\n"
                           "  Result: Confirm tracks, keyframes, and duration are correct";
                } else if (animation_type == "fade_in") {
                    text = "Set up a Fade In animation in Godot using MCP tools:\n\n"
                           "Step 1: Create an AnimationPlayer node\n"
                           "  Tool: create_animation\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"parent_path\": \"/root/Scene/UI\" }\n"
                           "  Result: AnimationPlayer with AnimationLibrary containing \"fade_in\" animation\n\n"
                           "Step 2: Add a track for the modulate alpha property\n"
                           "  Tool: add_animation_track\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"track_type\": \"value\", \"node_path\": \".:modulate\", \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: Value track targeting the node's modulate property\n\n"
                           "Step 3: Set the starting keyframe (fully transparent)\n"
                           "  Tool: set_keyframe\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"track_index\": 0, \"time\": 0.0, \"value\": \"Color(1, 1, 1, 0)\", \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: At t=0, the node is fully transparent\n\n"
                           "Step 4: Set the ending keyframe (fully visible)\n"
                           "  Tool: set_keyframe\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"track_index\": 0, \"time\": 0.5, \"value\": \"Color(1, 1, 1, 1)\", \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: At t=0.5s, the node is fully opaque\n\n"
                           "Step 5: Configure animation properties\n"
                           "  Tool: set_animation_properties\n"
                           "  Parameters: { \"animation_name\": \"fade_in\", \"duration\": 0.5, \"loop_mode\": 0, \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: Animation is 0.5s long, plays once (no loop)\n\n"
                           "Step 6: Verify the animation\n"
                           "  Tool: get_animation_info\n"
                           "  Parameters: { \"player_path\": \"/root/Scene/UI/AnimationPlayer\" }\n"
                           "  Result: Confirm animation list, tracks, keyframes, and properties";
                } else {
                    text = "Set up a " + animation_type + " animation in Godot using MCP tools:\n\n"
                           "Step 1: Create an AnimationPlayer node\n"
                           "  Tool: create_animation\n"
                           "  Create an AnimationPlayer with your animation in an AnimationLibrary\n\n"
                           "Step 2: Add animation tracks\n"
                           "  Tool: add_animation_track\n"
                           "  Add value, position, rotation, or other tracks targeting node properties\n\n"
                           "Step 3: Set keyframes\n"
                           "  Tool: set_keyframe\n"
                           "  Define keyframe values at specific times along each track\n\n"
                           "Step 4: Configure animation properties\n"
                           "  Tool: set_animation_properties\n"
                           "  Set duration, loop mode, and other animation settings\n\n"
                           "Step 5: Verify the animation\n"
                           "  Tool: get_animation_info\n"
                           "  Confirm tracks, keyframes, duration, and properties are correct";
                }

                return nlohmann::json::array({
                    {{"role", "user"}, {"content", {{"type", "text"}, {"text", text}}}}
                });
            }
        },

        // 7. test_game_ui (TEST-03)
        {
            "test_game_ui",
            "Step-by-step workflow for automated UI testing in a running Godot game using MCP tools",
            nlohmann::json::array({
                {{"name", "test_type"}, {"description", "Test scenario: button_click, form_validation, navigation, state_verification (default: button_click)"}, {"required", false}}
            }),
            [](const nlohmann::json& args) -> nlohmann::json {
                std::string test_type = "button_click";
                if (args.contains("test_type") && args["test_type"].is_string()) {
                    test_type = args["test_type"].get<std::string>();
                }

                std::string text;
                if (test_type == "button_click") {
                    text = "Automated UI Button Click Test workflow using MCP tools:\n\n"
                           "Step 1: Start the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"current\" }  (or \"main\" / \"custom\" with scene_path)\n"
                           "  Wait for bridge connection with get_game_bridge_status\n\n"
                           "Step 2: Get the game scene tree to find UI nodes\n"
                           "  Tool: get_game_scene_tree\n"
                           "  Parameters: { \"max_depth\": 3 }\n"
                           "  Result: Identify Control node paths for buttons to test\n\n"
                           "Step 3: Verify initial UI state before clicking\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"ButtonName\", \"property\": \"text\" }\n"
                           "  Result: Confirm the button text/label is as expected\n\n"
                           "Step 4: Click the button\n"
                           "  Tool: click_node\n"
                           "  Parameters: { \"node_path\": \"ButtonName\" }\n"
                           "  Result: Button receives press+release click at its center\n\n"
                           "Step 5: Verify state change after click\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"ResultLabel\", \"property\": \"text\" }\n"
                           "  Result: Verify the label updated to reflect the button action\n\n"
                           "Step 6: Check game output for expected logs\n"
                           "  Tool: get_game_output\n"
                           "  Parameters: { \"keyword\": \"button_pressed\" }\n"
                           "  Result: Verify the button handler printed expected output\n\n"
                           "Step 7: Batch test with run_test_sequence (optional)\n"
                           "  Tool: run_test_sequence\n"
                           "  Parameters: { \"steps\": [\n"
                           "    { \"action\": \"click_node\", \"args\": { \"node_path\": \"Button1\" }, \"description\": \"Click first button\" },\n"
                           "    { \"action\": \"get_game_node_property\", \"args\": { \"node_path\": \"Label\", \"property\": \"text\" },\n"
                           "      \"assert\": { \"property\": \"value\", \"contains\": \"clicked\" }, \"description\": \"Verify label updated\" }\n"
                           "  ]}\n"
                           "  Result: Structured pass/fail report for the entire sequence\n\n"
                           "Step 8: Stop the game\n"
                           "  Tool: stop_game\n"
                           "  Result: Game process terminated cleanly";
                } else if (test_type == "form_validation") {
                    text = "Automated UI Form Validation Test workflow using MCP tools:\n\n"
                           "Step 1: Start the game scene containing the form\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"custom\", \"scene_path\": \"res://scenes/form.tscn\" }\n"
                           "  Wait for bridge connection with get_game_bridge_status\n\n"
                           "Step 2: Inspect the form structure\n"
                           "  Tool: get_game_scene_tree\n"
                           "  Parameters: { \"max_depth\": 4 }\n"
                           "  Result: Identify LineEdit, TextEdit, and Button node paths\n\n"
                           "Step 3: Check initial form state\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"Form/SubmitButton\", \"property\": \"disabled\" }\n"
                           "  Result: Verify submit button is disabled when form is empty\n\n"
                           "Step 4: Simulate text input via eval_in_game\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Form/EmailInput').text = 'test@example.com'\" }\n"
                           "  Result: Form field populated programmatically\n\n"
                           "Step 5: Click submit button\n"
                           "  Tool: click_node\n"
                           "  Parameters: { \"node_path\": \"Form/SubmitButton\" }\n"
                           "  Result: Form submission triggered\n\n"
                           "Step 6: Verify validation result\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"Form/ErrorLabel\", \"property\": \"text\" }\n"
                           "  Result: Check for validation error or success message\n\n"
                           "Step 7: Check game output for validation logs\n"
                           "  Tool: get_game_output\n"
                           "  Parameters: { \"keyword\": \"validation\" }\n"
                           "  Result: Verify validation logic ran and logged results\n\n"
                           "Step 8: Stop the game\n"
                           "  Tool: stop_game";
                } else if (test_type == "navigation") {
                    text = "Automated UI Navigation Test workflow using MCP tools:\n\n"
                           "Step 1: Start the game at the main menu\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"main\" }\n"
                           "  Wait for bridge connection with get_game_bridge_status\n\n"
                           "Step 2: Verify we're on the main menu\n"
                           "  Tool: get_game_scene_tree\n"
                           "  Parameters: { \"max_depth\": 2 }\n"
                           "  Result: Confirm MainMenu or equivalent root node is present\n\n"
                           "Step 3: Click 'Play' button to navigate\n"
                           "  Tool: click_node\n"
                           "  Parameters: { \"node_path\": \"MainMenu/PlayButton\" }\n"
                           "  Result: Scene transition triggered\n\n"
                           "Step 4: Wait for scene transition\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_tree().current_scene.name\" }\n"
                           "  Result: Verify we navigated to the game scene (not still on menu)\n\n"
                           "Step 5: Navigate back (press Escape or Back button)\n"
                           "  Tool: inject_input\n"
                           "  Parameters: { \"type\": \"key\", \"keycode\": \"escape\" }\n"
                           "  Result: Trigger pause/back navigation\n\n"
                           "Step 6: Verify pause menu appeared\n"
                           "  Tool: get_game_node_property\n"
                           "  Parameters: { \"node_path\": \"PauseMenu\", \"property\": \"visible\" }\n"
                           "  Result: Pause menu is now visible\n\n"
                           "Step 7: Batch test entire flow with run_test_sequence\n"
                           "  Tool: run_test_sequence\n"
                           "  Use steps array to automate the full navigation flow with assertions\n\n"
                           "Step 8: Stop the game\n"
                           "  Tool: stop_game";
                } else if (test_type == "state_verification") {
                    text = "Automated Game State Verification Test workflow using MCP tools:\n\n"
                           "Step 1: Start the game\n"
                           "  Tool: run_game\n"
                           "  Parameters: { \"mode\": \"current\" }\n"
                           "  Wait for bridge connection with get_game_bridge_status\n\n"
                           "Step 2: Query initial game state\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Player').health\" }\n"
                           "  Result: Read initial player health value\n\n"
                           "Step 3: Trigger a game action\n"
                           "  Tool: click_node\n"
                           "  Parameters: { \"node_path\": \"HealButton\" }\n"
                           "  Result: Player heals\n\n"
                           "Step 4: Verify state changed\n"
                           "  Tool: eval_in_game\n"
                           "  Parameters: { \"expression\": \"get_node('Player').health\" }\n"
                           "  Result: Health should have increased\n\n"
                           "Step 5: Check game output for state change logs\n"
                           "  Tool: get_game_output\n"
                           "  Parameters: { \"keyword\": \"heal\" }\n"
                           "  Result: Verify heal action was logged\n\n"
                           "Step 6: Capture game viewport for visual verification\n"
                           "  Tool: capture_game_viewport\n"
                           "  Parameters: { \"width\": 800 }\n"
                           "  Result: Screenshot of current game state\n\n"
                           "Step 7: Run batch assertions with run_test_sequence\n"
                           "  Tool: run_test_sequence\n"
                           "  Parameters: { \"steps\": [\n"
                           "    { \"action\": \"eval_in_game\", \"args\": { \"expression\": \"get_node('Player').health\" },\n"
                           "      \"assert\": { \"property\": \"result\", \"not_empty\": true }, \"description\": \"Player health exists\" },\n"
                           "    { \"action\": \"get_game_node_property\", \"args\": { \"node_path\": \"Player\", \"property\": \"visible\" },\n"
                           "      \"assert\": { \"property\": \"value\", \"equals\": \"true\" }, \"description\": \"Player is visible\" }\n"
                           "  ]}\n"
                           "  Result: Structured pass/fail report\n\n"
                           "Step 8: Stop the game\n"
                           "  Tool: stop_game";
                } else {
                    text = "Automated " + test_type + " UI Test workflow using MCP tools:\n\n"
                           "Step 1: Start the game\n"
                           "  Tool: run_game\n"
                           "  Launch the appropriate scene for testing\n\n"
                           "Step 2: Explore the scene tree\n"
                           "  Tool: get_game_scene_tree\n"
                           "  Identify the UI nodes and their paths\n\n"
                           "Step 3: Verify initial state\n"
                           "  Tool: get_game_node_property\n"
                           "  Check initial property values on target nodes\n\n"
                           "Step 4: Interact with UI\n"
                           "  Tool: click_node / inject_input\n"
                           "  Trigger UI actions by clicking buttons or injecting input\n\n"
                           "Step 5: Verify state changes\n"
                           "  Tool: get_game_node_property / eval_in_game\n"
                           "  Assert that UI state changed as expected\n\n"
                           "Step 6: Check logs\n"
                           "  Tool: get_game_output\n"
                           "  Verify expected output was logged\n\n"
                           "Step 7: Batch test with run_test_sequence\n"
                           "  Tool: run_test_sequence\n"
                           "  Combine steps into automated sequence with assertions\n\n"
                           "Step 8: Stop the game\n"
                           "  Tool: stop_game";
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
