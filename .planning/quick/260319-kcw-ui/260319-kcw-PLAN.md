---
phase: quick
plan: 260319-kcw
type: execute
wave: 1
depends_on: []
files_modified:
  - project/kcw_backpack.gd
  - project/kcw_backpack.tscn
autonomous: true
requirements: [quick-task]

must_haves:
  truths:
    - "Scene runs standalone and displays a dark-themed game UI"
    - "Three action buttons (搜/打/撤) are visible and clickable"
    - "Clicking 搜 randomly generates loot items into the backpack grid"
    - "Clicking 打 triggers a simple combat round with HP changes and result log"
    - "Clicking 撤 clears the encounter area and resets for next exploration"
    - "Backpack grid displays collected item icons with names and counts"
    - "Status area shows player HP, gold, and current encounter info"
  artifacts:
    - path: "project/kcw_backpack.gd"
      provides: "Game loop logic: search/fight/retreat + inventory management"
      min_lines: 120
    - path: "project/kcw_backpack.tscn"
      provides: "Complete UI scene with panels, buttons, grid, and labels"
  key_links:
    - from: "project/kcw_backpack.tscn"
      to: "project/kcw_backpack.gd"
      via: "script attachment on root node"
      pattern: "script.*kcw_backpack"
---

<objective>
Create a complete interactive "搜打撤背包" (Search-Fight-Retreat Backpack) UI test scene for Godot 4.6.

Purpose: Provide a runnable game UI prototype demonstrating the search-fight-retreat loop with inventory management. This serves as an interactive test scene alongside the existing mini_game.tscn.

Output: Two files -- kcw_backpack.gd (game logic) and kcw_backpack.tscn (scene tree) -- that can be run directly in the Godot editor.
</objective>

<execution_context>
@C:/Users/28186/.claude/get-shit-done/workflows/execute-plan.md
@C:/Users/28186/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@project/mini_game.tscn (reference pattern for inline tscn scenes)
@project/project.godot (Godot 4.6, window config)
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create backpack UI game script</name>
  <files>project/kcw_backpack.gd</files>
  <action>
Create `project/kcw_backpack.gd` extending Control as the main game logic script. This implements a "搜打撤" (Search-Fight-Retreat) roguelike loop with inventory.

**Game state variables:**
- `player_hp: int = 100`, `player_max_hp: int = 100`, `gold: int = 0`
- `backpack: Array[Dictionary]` -- each entry: `{ "name": String, "icon": String, "count": int }`
- `max_backpack_slots: int = 16`
- `in_encounter: bool = false`, `encounter_enemy: Dictionary` (name, hp, max_hp, attack, loot)
- `log_messages: PackedStringArray`

**Item pool (static data):**
```
var ITEM_POOL := [
    {"name": "药草", "icon": "[药]", "value": 5},
    {"name": "铁矿", "icon": "[铁]", "value": 10},
    {"name": "宝石", "icon": "[宝]", "value": 25},
    {"name": "卷轴", "icon": "[卷]", "value": 15},
    {"name": "魔晶", "icon": "[魔]", "value": 20},
    {"name": "木材", "icon": "[木]", "value": 3},
    {"name": "皮革", "icon": "[皮]", "value": 8},
    {"name": "毒瓶", "icon": "[毒]", "value": 12},
]
```

**Enemy pool:**
```
var ENEMY_POOL := [
    {"name": "史莱姆", "hp": 30, "attack": 5, "loot_count": 1},
    {"name": "哥布林", "hp": 50, "attack": 10, "loot_count": 2},
    {"name": "骷髅兵", "hp": 80, "attack": 15, "loot_count": 3},
    {"name": "暗影狼", "hp": 60, "attack": 20, "loot_count": 2},
    {"name": "巨型蜘蛛", "hp": 100, "attack": 12, "loot_count": 4},
]
```

**@onready node references** (all bound to child nodes via NodePath):
- `status_label: Label` -- shows HP, gold
- `encounter_label: Label` -- shows current encounter info
- `log_label: RichTextLabel` -- scrollable action log
- `backpack_grid: GridContainer` -- 4-column grid for items
- `btn_search: Button`, `btn_fight: Button`, `btn_retreat: Button`

**_ready():**
- Connect button signals: `btn_search.pressed.connect(_on_search)`, same for fight/retreat
- Call `_update_ui()` and `_add_log("欢迎来到搜打撤! 点击[搜]开始探索。")`
- Set `btn_fight.disabled = true`, `btn_retreat.disabled = true` initially

**_on_search():**
- If `in_encounter`, return (already in encounter)
- 70% chance: find items (1-3 random items from ITEM_POOL, add to backpack via `_add_to_backpack`)
- 30% chance: encounter enemy (random from ENEMY_POOL, set `in_encounter = true`, store enemy data with full hp as max_hp)
- Enable/disable buttons accordingly: during encounter enable fight+retreat, disable search
- `_add_log()` with description of what happened
- `_update_ui()`

**_on_fight():**
- If not `in_encounter`, return
- Player deals `randi_range(10, 25)` damage to enemy
- Enemy deals its attack value to player (with randi_range variance +/- 3)
- Log each action
- If enemy hp <= 0: victory -- gain gold (enemy loot_count * 10 + randi_range(0, 20)), gain loot items, set `in_encounter = false`
- If player hp <= 0: game over -- show "你倒下了... 点击[搜]重新开始", call `_reset_game()`
- Enable/disable buttons, `_update_ui()`

**_on_retreat():**
- If not `in_encounter`, return
- 80% success: escape, lose some gold (10-20% of current), `in_encounter = false`, log success
- 20% fail: enemy gets a free hit, still in encounter, log failure
- Enable/disable buttons, `_update_ui()`

**_add_to_backpack(item_name: String, icon: String):**
- Check if item already in backpack -> increment count
- Otherwise add new entry if slots available, else log "背包已满!"

**_update_ui():**
- Status label: `"HP: %d/%d | 金币: %d" % [player_hp, player_max_hp, gold]`
- Encounter label: if in_encounter show enemy name + hp bar, else "安全区域 - 准备探索"
- Rebuild backpack grid: clear all children, for each backpack item create a PanelContainer with VBoxContainer inside containing Label (icon) and Label (name x count), set minimum size 80x80, use theme_override for dark background
- Button states: search disabled during encounter, fight/retreat disabled outside encounter

**_add_log(msg: String):**
- Prepend timestamp-like round counter, append to log_messages, keep last 50
- Update log_label.text with joined messages (newest at bottom)
- Auto-scroll to bottom: `log_label.scroll_to_line(log_label.get_line_count() - 1)`

**_reset_game():**
- Reset hp, gold, backpack, encounter state
- `_update_ui()`

**_update_backpack_grid():** (called from _update_ui)
- Clear existing children with `for c in backpack_grid.get_children(): c.queue_free()`
- For each item in backpack array, create item cell:
  ```
  var cell := PanelContainer.new()
  cell.custom_minimum_size = Vector2(80, 80)
  var sb := StyleBoxFlat.new()
  sb.bg_color = Color(0.2, 0.22, 0.28, 1.0)
  sb.corner_radius_top_left = 4
  sb.corner_radius_top_right = 4
  sb.corner_radius_bottom_left = 4
  sb.corner_radius_bottom_right = 4
  cell.add_theme_stylebox_override("panel", sb)
  var vbox := VBoxContainer.new()
  vbox.alignment = BoxContainer.ALIGNMENT_CENTER
  var icon_label := Label.new()
  icon_label.text = item.icon
  icon_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
  icon_label.add_theme_font_size_override("font_size", 20)
  var name_label := Label.new()
  name_label.text = "%s x%d" % [item.name, item.count]
  name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
  name_label.add_theme_font_size_override("font_size", 12)
  vbox.add_child(icon_label)
  vbox.add_child(name_label)
  cell.add_child(vbox)
  backpack_grid.add_child(cell)
  ```
- For empty slots up to max_backpack_slots, add empty cells with lighter bg

Use GDScript 4.x syntax throughout (typed vars, @onready, StringName signals).
  </action>
  <verify>
    <automated>cd D:/Workspace/Godot/godot-mcp-meow && test -f project/kcw_backpack.gd && wc -l project/kcw_backpack.gd | awk '{if ($1 >= 120) print "PASS: "$1" lines"; else print "FAIL: only "$1" lines"}'</automated>
  </verify>
  <done>kcw_backpack.gd exists with 120+ lines, contains all game loop functions (_on_search, _on_fight, _on_retreat), item/enemy pools, backpack management, and UI update logic</done>
</task>

<task type="auto">
  <name>Task 2: Create backpack UI scene file</name>
  <files>project/kcw_backpack.tscn</files>
  <action>
Create `project/kcw_backpack.tscn` as a complete Godot scene. Use the .tscn text format directly (like mini_game.tscn pattern but with external script reference instead of inline).

**Scene tree structure:**

```
BackpackUI (Control) -- script: res://kcw_backpack.gd, full_rect anchors
  Background (ColorRect) -- full_rect, color: Color(0.1, 0.1, 0.15, 1)
  MainLayout (HBoxContainer) -- full_rect with margins (20px all sides)
    LeftPanel (VBoxContainer) -- size_flags_horizontal=3 (expand+fill), stretch_ratio=0.6
      StatusPanel (PanelContainer)
        StatusContent (VBoxContainer)
          TitleLabel (Label) -- text="搜打撤", font_size=32, center aligned
          StatusLabel (Label) -- text="HP: 100/100 | 金币: 0", font_size=18
          EncounterLabel (Label) -- text="安全区域 - 准备探索", font_size=16, autowrap
      ActionPanel (HBoxContainer) -- separation=10
        BtnSearch (Button) -- text="搜 探索", custom_minimum_size=Vector2(100, 50), font_size=18
        BtnFight (Button) -- text="打 战斗", custom_minimum_size=Vector2(100, 50), font_size=18
        BtnRetreat (Button) -- text="撤 撤退", custom_minimum_size=Vector2(100, 50), font_size=18
      LogPanel (PanelContainer) -- size_flags_vertical=3 (expand+fill)
        LogLabel (RichTextLabel) -- scroll_active=true, bbcode_enabled=false, text="", fit_content=false
    RightPanel (VBoxContainer) -- size_flags_horizontal=3 (expand+fill), stretch_ratio=0.4
      BackpackTitle (Label) -- text="背包", font_size=24, center aligned
      BackpackScroll (ScrollContainer) -- size_flags_vertical=3 (expand+fill)
        BackpackGrid (GridContainer) -- columns=4
```

**Anchors/layout:**
- BackpackUI: anchor_right=1, anchor_bottom=1 (full rect)
- Background: anchor_right=1, anchor_bottom=1 (full rect)
- MainLayout: anchor_right=1, anchor_bottom=1, offset_left=20, offset_top=20, offset_right=-20, offset_bottom=-20

**Theme overrides for dark style:**
- StatusPanel: StyleBoxFlat bg_color Color(0.15, 0.15, 0.22, 1), corner_radius 8
- LogPanel: StyleBoxFlat bg_color Color(0.12, 0.12, 0.18, 1), corner_radius 8
- All Labels: font_color Color(0.9, 0.9, 0.95, 1)
- Buttons: Use default theme (Godot 4.6 dark theme looks fine)
- TitleLabel: font_color Color(1.0, 0.85, 0.3, 1) -- gold accent

**Node path references must match script @onready vars:**
- `$MainLayout/LeftPanel/StatusPanel/StatusContent/StatusLabel` -- bind as status_label
- `$MainLayout/LeftPanel/StatusPanel/StatusContent/EncounterLabel` -- bind as encounter_label
- `$MainLayout/LeftPanel/LogPanel/LogLabel` -- bind as log_label
- `$MainLayout/RightPanel/BackpackScroll/BackpackGrid` -- bind as backpack_grid
- `$MainLayout/LeftPanel/ActionPanel/BtnSearch` -- bind as btn_search
- `$MainLayout/LeftPanel/ActionPanel/BtnFight` -- bind as btn_fight
- `$MainLayout/LeftPanel/ActionPanel/BtnRetreat` -- bind as btn_retreat

Write the .tscn file using `[gd_scene format=3]` with `[ext_resource]` for the script and proper node hierarchy using `parent` paths. Use `[sub_resource]` for StyleBoxFlat overrides on panels.

Ensure GDScript @onready paths in kcw_backpack.gd match the actual scene tree paths exactly. If needed, update kcw_backpack.gd to use the correct NodePath strings.
  </action>
  <verify>
    <automated>cd D:/Workspace/Godot/godot-mcp-meow && test -f project/kcw_backpack.tscn && grep -c "\[node" project/kcw_backpack.tscn | awk '{if ($1 >= 12) print "PASS: "$1" nodes"; else print "FAIL: only "$1" nodes"}'</automated>
  </verify>
  <done>kcw_backpack.tscn exists with 12+ nodes forming the full UI layout, references kcw_backpack.gd as external script, all node paths match @onready declarations in the script</done>
</task>

</tasks>

<verification>
1. Both files exist: `project/kcw_backpack.gd` and `project/kcw_backpack.tscn`
2. Script has all required functions: _on_search, _on_fight, _on_retreat, _add_to_backpack, _update_ui, _reset_game
3. Scene has correct node hierarchy with matching @onready paths
4. Scene can be opened in Godot editor without parse errors
</verification>

<success_criteria>
- kcw_backpack.gd contains complete game loop logic (search finds items or triggers encounters, fight handles combat rounds, retreat allows escape)
- kcw_backpack.tscn defines full UI layout with status panel, action buttons, log area, and backpack grid
- Node paths in .tscn match @onready declarations in .gd exactly
- Scene is runnable: set as main scene in Godot editor and interact with the three buttons
</success_criteria>

<output>
After completion, create `.planning/quick/260319-kcw-ui/260319-kcw-SUMMARY.md`
</output>
