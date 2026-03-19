---
phase: quick
plan: 260319-qli
type: execute
wave: 1
depends_on: []
files_modified:
  - project/backpack_ui.gd
  - project/backpack_ui.tscn
autonomous: false
requirements: [quick-task]

must_haves:
  truths:
    - "Scene runs standalone in Godot editor (F6) and displays a dark-themed backpack game UI"
    - "Three core action buttons (搜/打/撤) are visible and functional"
    - "Clicking 搜 randomly discovers loot items or triggers enemy encounters"
    - "Clicking 打 executes a combat round with damage exchange and possible victory/defeat"
    - "Clicking 撤 attempts retreat from encounter with success/failure outcome"
    - "Backpack grid shows collected items with rarity-colored borders and hover tooltips"
    - "BBCode-colored action log scrolls and shows round-numbered entries"
    - "HP bars for player and enemy update in real-time during combat"
    - "XP/leveling system grants stat bonuses on level up"
    - "Sell all and use herb buttons provide inventory management actions"
  artifacts:
    - path: "project/backpack_ui.gd"
      provides: "Game logic: search/fight/retreat loop, inventory, XP, BBCode log"
      min_lines: 300
    - path: "project/backpack_ui.tscn"
      provides: "Scene tree with dark themed UI: panels, buttons, HP bars, backpack grid"
  key_links:
    - from: "project/backpack_ui.tscn"
      to: "project/backpack_ui.gd"
      via: "script attachment on root node"
      pattern: "script.*backpack_ui"
---

<objective>
Create a complete interactive "搜打撤背包" (Search-Fight-Retreat Backpack) UI test scene for Godot.

Purpose: Demonstrate a polished roguelike game loop with inventory management, combat, BBCode colored log, XP/leveling, item rarity, and dark-themed UI -- all built via Godot MCP tools for live editor interaction.

Output: Two files -- backpack_ui.gd (game logic ~400 lines) and backpack_ui.tscn (scene tree ~30 nodes) -- runnable standalone in Godot editor.
</objective>

<execution_context>
@C:/Users/28186/.claude/get-shit-done/workflows/execute-plan.md
@C:/Users/28186/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@CLAUDE.md

<interfaces>
<!-- Prior art patterns from completed quick tasks (files now deleted but patterns documented) -->

Game loop pattern (from 260319-kcw-SUMMARY):
- extends Control with @onready node references
- ITEM_POOL / ENEMY_POOL dictionaries as static data
- search (70% loot / 30% encounter), fight (damage calc), retreat (80% success)
- _add_to_backpack() with stackable items, _update_backpack_grid() with dynamic PanelContainer cells
- _add_log() with round counter prefix, auto-scroll

Enhanced patterns (from 260319-log-SUMMARY):
- BBCode RichTextLabel for colored log (green=damage dealt, red=damage taken, yellow=loot, purple=level up)
- RARITY_COLORS dict: common=gray, uncommon=green, rare=blue, epic=purple
- StyleBoxFlat.border_color for rarity-colored backpack cells
- XP/leveling with scaling stats (attack + max_hp per level)
- Sell all + use herb extra actions
- ProgressBar for HP display

MCP tools available (from CLAUDE.md):
- create_scene, create_node, set_node_property, set_layout_preset
- write_script, attach_script, save_scene
- set_theme_override, create_stylebox, set_container_layout
- connect_signal, run_game, capture_game_viewport
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create backpack UI game script via MCP write_script</name>
  <files>project/backpack_ui.gd</files>
  <action>
Use `mcp__godot__write_script` to create `res://backpack_ui.gd` extending Control. This implements the full "搜打撤" roguelike game loop with inventory, XP, and BBCode logging.

**Game Data:**

ITEM_POOL -- 10 items with rarity:
```
{"name": "药草", "icon": "[药]", "value": 5, "rarity": "common"},
{"name": "木材", "icon": "[木]", "value": 3, "rarity": "common"},
{"name": "皮革", "icon": "[皮]", "value": 8, "rarity": "common"},
{"name": "铁矿", "icon": "[铁]", "value": 10, "rarity": "common"},
{"name": "卷轴", "icon": "[卷]", "value": 15, "rarity": "uncommon"},
{"name": "毒瓶", "icon": "[毒]", "value": 12, "rarity": "uncommon"},
{"name": "羽毛", "icon": "[羽]", "value": 2, "rarity": "uncommon"},
{"name": "魔晶", "icon": "[魔]", "value": 20, "rarity": "rare"},
{"name": "宝石", "icon": "[宝]", "value": 25, "rarity": "rare"},
{"name": "金锭", "icon": "[金]", "value": 30, "rarity": "epic"},
```

ENEMY_POOL -- 7 enemies:
```
{"name": "史莱姆", "hp": 30, "attack": 5, "loot_count": 1},
{"name": "哥布林", "hp": 50, "attack": 10, "loot_count": 2},
{"name": "暗影狼", "hp": 60, "attack": 20, "loot_count": 2},
{"name": "火蜥蜴", "hp": 70, "attack": 25, "loot_count": 2},
{"name": "骷髅兵", "hp": 80, "attack": 15, "loot_count": 3},
{"name": "巨型蜘蛛", "hp": 100, "attack": 12, "loot_count": 4},
{"name": "石像鬼", "hp": 120, "attack": 18, "loot_count": 3},
```

RARITY_COLORS:
```
{"common": Color(0.7, 0.7, 0.7), "uncommon": Color(0.3, 1.0, 0.3), "rare": Color(0.3, 0.5, 1.0), "epic": Color(0.8, 0.3, 1.0)}
```

**State variables:**
- player_hp := 100, player_max_hp := 100, gold := 0
- player_attack_base := 15, player_level := 1, player_xp := 0, xp_to_next := 50
- backpack: Array = [], max_backpack_slots := 16
- in_encounter := false, encounter_enemy: Dictionary = {}
- round_counter := 0, total_kills := 0, total_searches := 0

**@onready node references (paths must match scene tree in Task 2):**
- status_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/StatusLabel
- encounter_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/EncounterLabel
- stats_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/StatsLabel
- hp_bar: ProgressBar = $MainLayout/LeftPanel/StatusPanel/StatusContent/HPBarContainer/HPBar
- enemy_hp_bar: ProgressBar = $MainLayout/LeftPanel/StatusPanel/StatusContent/EnemyHPBar
- log_label: RichTextLabel = $MainLayout/LeftPanel/LogPanel/LogLabel
- backpack_grid: GridContainer = $MainLayout/RightPanel/BackpackScroll/BackpackGrid
- backpack_count_label: Label = $MainLayout/RightPanel/BackpackHeader/BackpackCountLabel
- btn_search: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow1/BtnSearch
- btn_fight: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow1/BtnFight
- btn_retreat: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow1/BtnRetreat
- btn_sell_all: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow2/BtnSellAll
- btn_use_herb: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow2/BtnUseHerb

**_ready():**
- Connect all 5 button pressed signals to their handlers
- hp_bar.max_value = player_max_hp, hp_bar.value = player_hp
- enemy_hp_bar.visible = false
- btn_fight.disabled = true, btn_retreat.disabled = true
- _update_ui()
- _add_colored_log("欢迎来到搜打撤! 点击[搜]开始探索。", Color(1, 0.85, 0.3))

**_on_search():**
- If in_encounter, return
- round_counter += 1, total_searches += 1
- 70% chance: find 1-3 random items from ITEM_POOL, add via _add_to_backpack, log each in yellow
- 30% chance: encounter random enemy, set in_encounter=true, store enemy with max_hp copy, log in red, show enemy_hp_bar
- Update button states: during encounter enable fight+retreat, disable search
- _update_ui()

**_on_fight():**
- If not in_encounter, return
- Player deals randi_range(player_attack_base - 5, player_attack_base + 10) damage -- log in green
- Enemy deals randi_range(max(1, encounter_enemy.attack - 3), encounter_enemy.attack + 3) -- log in red
- If enemy hp <= 0: victory -- gain gold (loot_count * 10 + randi_range(0, 20)), gain loot items, xp = enemy max_hp / 2, total_kills += 1, check level up, in_encounter = false, enemy_hp_bar.visible = false
- If player hp <= 0: game over -- log "你倒下了..." in red, _reset_game()
- Update button states, _update_ui()

**Level up check (_check_level_up):**
- While player_xp >= xp_to_next: player_level += 1, player_xp -= xp_to_next, xp_to_next = int(xp_to_next * 1.5), player_attack_base += 3, player_max_hp += 15, player_hp = player_max_hp, log in purple

**_on_retreat():**
- If not in_encounter, return
- 80% success: escape, lose randi_range(gold/10, gold/5) gold (min 0), in_encounter=false, enemy_hp_bar.visible=false, log in gray
- 20% fail: enemy free hit, still in encounter, log in red
- Update button states, _update_ui()

**_on_sell_all():**
- Sum all items value * count, add to gold, clear backpack
- Log total gold earned in yellow
- _update_ui()

**_on_use_herb():**
- Find "药草" in backpack, if found and player_hp < player_max_hp: heal min(25, player_max_hp - player_hp), reduce herb count (remove entry if count reaches 0), log healing in green
- If not found or hp full: log "没有药草或HP已满" in gray
- _update_ui()

**_add_to_backpack(item_name: String, icon: String, rarity: String):**
- Check if item already in backpack (match by name) -> increment count
- Otherwise add {"name": item_name, "icon": icon, "count": 1, "rarity": rarity} if slots available
- If full: log "背包已满!" in red

**_add_colored_log(msg: String, color: Color):**
- Format: "[color=#%s][R%d][/color] [color=#%s]%s[/color]" using Color(0.5,0.5,0.5).to_html(false) for round prefix, color.to_html(false) for message
- Append to log_label.text with newline
- Keep last 100 lines: if get_line_count > 100, find first newline and trim
- Auto-scroll: await get_tree().process_frame then log_label.scroll_to_line(log_label.get_line_count() - 1)

**_add_log(msg: String):**
- Call _add_colored_log(msg, Color(0.85, 0.85, 0.9))

**_update_ui():**
- hp_bar.value = player_hp, hp_bar.max_value = player_max_hp
- status_label.text = "HP: %d/%d | 金币: %d | 等级: %d" % [player_hp, player_max_hp, gold, player_level]
- stats_label.text = "击杀: %d | 探索: %d | XP: %d/%d" % [total_kills, total_searches, player_xp, xp_to_next]
- If in_encounter: encounter_label.text with enemy name+hp, enemy_hp_bar.value/max_value/visible=true
- Else: encounter_label.text = "安全区域 - 准备探索", enemy_hp_bar.visible=false
- backpack_count_label.text = "%d/%d" % [backpack.size(), max_backpack_slots]
- btn_sell_all.disabled = backpack.is_empty()
- btn_use_herb.disabled = not _has_herb() or player_hp >= player_max_hp
- _update_backpack_grid()

**_has_herb() -> bool:** Return true if any backpack entry has name == "药草"

**_update_backpack_grid():**
- Clear: for c in backpack_grid.get_children(): c.queue_free()
- For each item in backpack:
  - PanelContainer cell, custom_minimum_size = Vector2(80, 80)
  - StyleBoxFlat: bg_color=Color(0.2, 0.22, 0.28), corner_radius=4 all, border_width=2 all, border_color=RARITY_COLORS[item.rarity]
  - VBoxContainer with alignment=CENTER
  - Label for icon: font_size=22, horizontal_alignment=CENTER
  - Label for name: text="%s x%d" % [item.name, item.count], font_size=11, horizontal_alignment=CENTER, font_color=RARITY_COLORS[item.rarity]
  - cell.tooltip_text = "%s\n价值: %d 金\n稀有度: %s" % [item.name, value_from_pool, item.rarity]
  - Add cell to backpack_grid
- Fill remaining empty slots (up to max_backpack_slots) with lighter bg cells (Color(0.15, 0.15, 0.2), no border)

**_get_item_value(item_name: String) -> int:** Look up value in ITEM_POOL by name

**_reset_game():**
- Reset all state to defaults, clear backpack
- enemy_hp_bar.visible = false
- btn_fight.disabled = true, btn_retreat.disabled = true, btn_search.disabled = false
- _update_ui()
- _add_colored_log("游戏重置 - 重新开始冒险!", Color(1, 0.85, 0.3))

Target: ~400-450 lines of clean GDScript 4.x with typed variables, @onready, StringName signals.
  </action>
  <verify>
    <automated>test -f D:/Workspace/Godot/godot-mcp-meow/project/backpack_ui.gd && wc -l D:/Workspace/Godot/godot-mcp-meow/project/backpack_ui.gd | awk '{if ($1 >= 300) print "PASS: "$1" lines"; else print "FAIL: only "$1" lines"}'</automated>
  </verify>
  <done>backpack_ui.gd exists with 300+ lines containing full game loop (search/fight/retreat), XP/leveling, BBCode log, rarity system, sell/use actions, and all @onready paths matching the scene tree</done>
</task>

<task type="auto">
  <name>Task 2: Build scene tree via Godot MCP tools</name>
  <files>project/backpack_ui.tscn</files>
  <action>
Use Godot MCP tools to build the scene tree interactively in the editor. This is the preferred approach per CLAUDE.md.

**Step 1: Create root scene**
`mcp__godot__create_scene` with root_type="Control", root_name="BackpackUI"

**Step 2: Build node tree** using `mcp__godot__create_node` for each node. Full tree structure:

```
BackpackUI (Control) -- root
  Background (ColorRect)
  MainLayout (HBoxContainer)
    LeftPanel (VBoxContainer)
      StatusPanel (PanelContainer)
        StatusContent (VBoxContainer)
          TitleLabel (Label)
          HPBarContainer (HBoxContainer)
            HPLabel (Label)
            HPBar (ProgressBar)
          StatusLabel (Label)
          StatsLabel (Label)
          EncounterLabel (Label)
          EnemyHPBar (ProgressBar)
      ActionPanel (PanelContainer)
        ActionContent (VBoxContainer)
          ActionRow1 (HBoxContainer)
            BtnSearch (Button)
            BtnFight (Button)
            BtnRetreat (Button)
          ActionRow2 (HBoxContainer)
            BtnSellAll (Button)
            BtnUseHerb (Button)
      LogPanel (PanelContainer)
        LogLabel (RichTextLabel)
    RightPanel (VBoxContainer)
      BackpackHeader (HBoxContainer)
        BackpackTitle (Label)
        BackpackCountLabel (Label)
      BackpackScroll (ScrollContainer)
        BackpackGrid (GridContainer)
```

Total: ~31 nodes.

**Step 3: Set layout presets** using `mcp__godot__set_layout_preset`:
- BackpackUI: "full_rect"
- Background: "full_rect"
- MainLayout: "full_rect" (then adjust offsets for margins)

**Step 4: Set node properties** using `mcp__godot__set_node_property`:

Background:
- color = Color(0.08, 0.08, 0.12, 1)

MainLayout:
- offset_left = 16, offset_top = 16, offset_right = -16, offset_bottom = -16

LeftPanel:
- size_flags_horizontal = 3 (EXPAND_FILL)
- size_flags_stretch_ratio = 0.6

RightPanel:
- size_flags_horizontal = 3
- size_flags_stretch_ratio = 0.4

LogPanel:
- size_flags_vertical = 3 (EXPAND_FILL)

BackpackScroll:
- size_flags_vertical = 3

All Buttons:
- size_flags_horizontal = 3

BtnSearch: text="搜 探索", custom_minimum_size=Vector2(90, 44)
BtnFight: text="打 战斗", custom_minimum_size=Vector2(90, 44)
BtnRetreat: text="撤 撤退", custom_minimum_size=Vector2(90, 44)
BtnSellAll: text="卖 全部出售", custom_minimum_size=Vector2(0, 38)
BtnUseHerb: text="药 使用药草", custom_minimum_size=Vector2(0, 38)

TitleLabel: text="搜打撤 Plus", horizontal_alignment=1 (CENTER)
StatusLabel: text="HP: 100/100 | 金币: 0 | 等级: 1"
StatsLabel: text="击杀: 0 | 探索: 0 | XP: 0/50"
EncounterLabel: text="安全区域 - 准备探索", autowrap_mode=2
HPLabel: text="HP:"
HPBar: max_value=100, value=100, custom_minimum_size=Vector2(0, 20), size_flags_horizontal=3
EnemyHPBar: max_value=100, value=100, visible=false, custom_minimum_size=Vector2(0, 16)
BackpackTitle: text="背包", horizontal_alignment=1, size_flags_horizontal=3
BackpackCountLabel: text="0/16"
BackpackGrid: columns=4, size_flags_horizontal=3
LogLabel: bbcode_enabled=true, scroll_active=true

**Step 5: Set theme overrides** using `mcp__godot__set_theme_override`:

TitleLabel: font_size=28, font_color=Color(1, 0.85, 0.3, 1)
StatusLabel: font_size=16, font_color=Color(0.9, 0.9, 0.95, 1)
StatsLabel: font_size=14, font_color=Color(0.6, 0.6, 0.7, 1)
EncounterLabel: font_size=15, font_color=Color(0.9, 0.9, 0.95, 1)
HPLabel: font_size=16, font_color=Color(0.9, 0.3, 0.3, 1)
BackpackTitle: font_size=22, font_color=Color(1, 0.85, 0.3, 1)
BackpackCountLabel: font_size=16, font_color=Color(0.6, 0.6, 0.7, 1)
LogLabel: font_size=13, font_color=Color(0.85, 0.85, 0.9, 1)
BtnSearch: font_size=16
BtnFight: font_size=16
BtnRetreat: font_size=16
BtnSellAll: font_size=14
BtnUseHerb: font_size=14

**Step 6: Create styleboxes** using `mcp__godot__create_stylebox`:

StatusPanel: bg_color=Color(0.15, 0.15, 0.22, 1), corner_radius=8
ActionPanel: bg_color=Color(0.14, 0.14, 0.21, 1), corner_radius=6
LogPanel: bg_color=Color(0.1, 0.1, 0.16, 1), corner_radius=8

**Step 7: Set container layout** using `mcp__godot__set_container_layout`:

MainLayout: separation=16
LeftPanel: separation=10
StatusContent: separation=6
HPBarContainer: separation=8
ActionContent: separation=6
ActionRow1: separation=8
ActionRow2: separation=8
RightPanel: separation=8
BackpackGrid: h_separation=6, v_separation=6 (use set_node_property for theme_override_constants)

**Step 8: Attach script**
`mcp__godot__attach_script` -- attach res://backpack_ui.gd to BackpackUI root node

**Step 9: Save scene**
`mcp__godot__save_scene` -- save to res://backpack_ui.tscn

Order of MCP calls matters -- create parent nodes before children. Batch related property sets where possible to minimize round trips.

If any MCP tool call fails (e.g., connection issue), fall back to writing the .tscn file directly using the Write tool, following the ext_resource + sub_resource pattern from previous quick tasks.
  </action>
  <verify>
    <automated>test -f D:/Workspace/Godot/godot-mcp-meow/project/backpack_ui.tscn && echo "PASS: scene file exists" || echo "FAIL: scene file not found"</automated>
  </verify>
  <done>backpack_ui.tscn exists on disk with ~30 nodes, dark-themed styleboxes, all node paths matching @onready declarations in backpack_ui.gd, script attached to root node</done>
</task>

<task type="checkpoint:human-verify" gate="informational">
  <name>Task 3: Verify backpack UI game runs correctly</name>
  <files>project/backpack_ui.tscn</files>
  <action>
User visually verifies the complete scene runs as expected in Godot editor.
  </action>
  <what-built>Complete "搜打撤 Plus" backpack UI game scene with search/fight/retreat loop, BBCode colored log, XP/leveling, item rarity borders, sell/use actions, HP bars, and dark-themed layout (~30 nodes, ~400 lines GDScript)</what-built>
  <how-to-verify>
    1. In Godot editor, open project/backpack_ui.tscn
    2. Press F6 to run the scene standalone
    3. Click "搜 探索" several times -- should see loot items (yellow log) and occasional enemy encounters (red log)
    4. During encounter, click "打 战斗" -- should see damage dealt (green) and received (red), HP bars update
    5. Try "撤 撤退" during encounter -- 80% success with gold loss, 20% fail with enemy hit
    6. Check backpack grid -- items should have rarity-colored borders (gray/green/blue/purple)
    7. Hover backpack items -- tooltip should show name, value, rarity
    8. Click "卖 全部出售" -- should sell all items for gold
    9. With herbs in backpack and HP below max, click "药 使用药草" -- should heal 25 HP
    10. Fight enough enemies to level up -- should see purple level-up message and stat increases
  </how-to-verify>
  <verify>
    <automated>echo "CHECKPOINT: Manual verification required -- run scene in Godot editor"</automated>
  </verify>
  <done>User confirms all 10 verification steps pass: game loop works, log is colored, backpack shows rarity borders, XP/leveling functions, sell/use actions work</done>
  <resume-signal>Type "approved" or describe any issues</resume-signal>
</task>

</tasks>

<verification>
1. project/backpack_ui.gd exists with 300+ lines
2. project/backpack_ui.tscn exists with scene tree
3. Script has all functions: _on_search, _on_fight, _on_retreat, _on_sell_all, _on_use_herb, _add_colored_log, _update_backpack_grid, _check_level_up, _reset_game
4. Scene node paths match @onready declarations in script
5. Scene runs standalone in Godot editor with full interactive game loop
</verification>

<success_criteria>
- backpack_ui.gd contains 300+ lines with complete game loop, XP/leveling, BBCode colored log, rarity system, sell/use actions
- backpack_ui.tscn has ~30 nodes with dark theme, HP bars, action buttons, backpack grid, and script attachment
- Scene is runnable via F6 with full interactive search-fight-retreat game loop
- Log entries appear in different colors (green/red/yellow/purple/gray) based on event type
- Backpack items display rarity-colored borders and hover tooltips
</success_criteria>

<output>
After completion, create `.planning/quick/260319-qli-ui/260319-qli-SUMMARY.md`
</output>
