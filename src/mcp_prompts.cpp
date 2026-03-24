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
        },

        // 8. tool_composition_guide (PROMPT-01)
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
                           "1. find_nodes { \"pattern\": \"Enemy*\", \"type_filter\": \"CharacterBody2D\" }\n"
                           "2. batch_set_property { \"node_paths\": [\"Enemy1\", \"Enemy2\"], \"property\": \"speed\", \"value\": \"150\" }\n\n"
                           "=== Duplicate existing node ===\n"
                           "1. duplicate_node { \"node_path\": \"/root/Level/Player\", \"new_name\": \"Player2\" }";
                } else if (category == "ui_building") {
                    text = "Tool Composition Guide: UI Building\n\n"
                           "=== Create a UI panel (quick) ===\n"
                           "1. create_ui_panel { \"root_type\": \"VBoxContainer\", \"name\": \"MenuPanel\", \"parent_path\": \"/root/Scene\", "
                              "\"children\": [{\"type\": \"Label\", \"name\": \"Title\"}, {\"type\": \"Button\", \"name\": \"Play\"}] }\n\n"
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
                           "2. find_nodes { \"pattern\": \"*Player*\" }\n\n"
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
                           "  Parameters: { \"pattern\": \"<expected_node_name>\" }\n"
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
                           "  Parameters: { \"pattern\": \"<node_name>\" }\n"
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
                           "  Parameters: { \"pattern\": \"CollisionShape*\" }\n"
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
                           "  Parameters: { \"pattern\": \"CollisionShape*\" }\n"
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
                           "  Parameters: { \"pattern\": \"<partial_name>\" }\n"
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
