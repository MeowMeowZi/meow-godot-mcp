---
phase: quick
plan: 260319-log
type: execute
wave: 1
depends_on: []
files_modified:
  - project/log_test_scene.gd
  - project/log_test_scene.tscn
autonomous: true
requirements: [quick-task]

must_haves:
  truths:
    - "Scene runs in Godot editor via F6 and displays a full interactive backpack game"
    - "User can click search/fight/retreat buttons and see game state update"
    - "Action log scrolls and shows timestamped colored entries"
    - "Backpack grid displays items with tooltips on hover"
    - "Player stats (HP bar, gold, round) update in real-time"
  artifacts:
    - path: "project/log_test_scene.gd"
      provides: "Game logic with enhanced log system and interactive backpack"
      min_lines: 300
    - path: "project/log_test_scene.tscn"
      provides: "Scene tree with dark themed UI layout"
  key_links:
    - from: "project/log_test_scene.tscn"
      to: "project/log_test_scene.gd"
      via: "ext_resource script reference"
      pattern: 'ext_resource.*log_test_scene\.gd'
---

<objective>
Create an enhanced interactive test scene for the search-fight-retreat backpack UI game with improved log display, better visual feedback, and richer interactions.

Purpose: Build a polished, self-contained game prototype that demonstrates the search-fight-retreat loop with an emphasis on a rich action log UI, hover-interactive backpack slots, HP/enemy health bars, and visual polish.

Output: Two files -- `log_test_scene.gd` (game script) and `log_test_scene.tscn` (scene file) that run standalone in Godot editor.
</objective>

<execution_context>
@C:/Users/28186/.claude/get-shit-done/workflows/execute-plan.md
@C:/Users/28186/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@project/kcw_backpack.gd (reference implementation for game loop logic)
@project/kcw_backpack.tscn (reference implementation for scene structure)

<interfaces>
<!-- Existing backpack game patterns to follow -->

From project/kcw_backpack.gd:
- extends Control with @onready node references
- ITEM_POOL and ENEMY_POOL dictionaries for game data
- search (70% loot / 30% encounter), fight (damage calc), retreat (80% success) game loop
- _add_to_backpack() with stackable items, _update_backpack_grid() with dynamic PanelContainer cells
- _add_log() with round counter prefix, auto-scroll via await process_frame

From project/kcw_backpack.tscn:
- ext_resource script reference pattern (not inline GDScript)
- HBoxContainer main layout: 60% left panel (status + actions + log), 40% right panel (backpack)
- StyleBoxFlat sub_resources for dark panels (bg_color ~0.12-0.15, corner_radius 8)
- theme_override_colors and theme_override_font_sizes on Labels
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create enhanced backpack game script with rich log and interactions</name>
  <files>project/log_test_scene.gd</files>
  <action>
Create `project/log_test_scene.gd` extending Control. This is an enhanced version of the existing kcw_backpack.gd with these improvements:

**Game Data (same core pools, expanded):**
- ITEM_POOL: 10 items (add "金锭"=[金]/30, "羽毛"=[羽]/2 to existing 8)
- ENEMY_POOL: 7 enemies (add "石像鬼" hp:120 atk:18 loot:3, "火蜥蜴" hp:70 atk:25 loot:2 to existing 5)
- RARITY_COLORS dictionary mapping rarity tiers: "common" -> Color(0.7,0.7,0.7), "uncommon" -> Color(0.3,1.0,0.3), "rare" -> Color(0.3,0.5,1.0), "epic" -> Color(0.8,0.3,1.0)
- Each item in ITEM_POOL gets a "rarity" field (first 4 common, next 3 uncommon, next 2 rare, last 1 epic)

**Game State (expanded from original):**
- player_hp, player_max_hp (100), gold (0), backpack Array[Dictionary], max_backpack_slots (16)
- in_encounter, encounter_enemy, round_counter
- NEW: player_attack_base := 15 (used in damage calc)
- NEW: player_level := 1, player_xp := 0, xp_to_next := 50
- NEW: total_kills := 0, total_searches := 0 (stats tracking)

**Node References (@onready):**
- status_label, encounter_label, log_label (RichTextLabel), backpack_grid
- btn_search, btn_fight, btn_retreat
- NEW: hp_bar (ProgressBar), enemy_hp_bar (ProgressBar)
- NEW: stats_label (Label for kills/searches/level)
- NEW: btn_sell_all (Button to sell all backpack items for gold)
- NEW: btn_use_herb (Button to use herb item for healing)
- NEW: backpack_count_label (Label showing "X/16" slots used)

**Enhanced _ready():**
- Connect all buttons including btn_sell_all.pressed -> _on_sell_all, btn_use_herb.pressed -> _on_use_herb
- Initialize hp_bar.max_value = player_max_hp, hp_bar.value = player_hp
- Set enemy_hp_bar.visible = false initially
- Call _update_ui() and _add_log with BBCode welcome message

**Enhanced Log System (_add_log with BBCode colors):**
- Use RichTextLabel with bbcode_enabled = true
- Format: `[color=#888888][R{round}][/color] {msg}` for normal
- Add helper _add_colored_log(msg: String, color: Color) that wraps in [color] tags
- Combat damage dealt -> green color
- Combat damage received -> red color
- Loot found -> yellow/gold color
- Level up -> purple color
- Retreat -> gray color
- Keep last 100 messages (up from 50)
- Auto-scroll via `await get_tree().process_frame` then `log_label.scroll_to_line(log_label.get_line_count() - 1)`

**Enhanced Combat:**
- Player damage: randi_range(player_attack_base - 5, player_attack_base + 10)
- XP gain on kill: enemy max_hp / 2 (integer)
- Level up when xp >= xp_to_next: player_level += 1, player_attack_base += 3, player_max_hp += 15, player_hp = player_max_hp, xp_to_next = int(xp_to_next * 1.5), log level up in purple

**New Actions:**
- _on_sell_all(): Sell all backpack items, sum values * count, add to gold, clear backpack, log total gold earned in yellow. Disabled if backpack empty.
- _on_use_herb(): Find "药草" in backpack, if found: heal 25 HP (capped at max), reduce count by 1 (remove if 0), log healing in green. Disabled if no herbs or HP full.

**Enhanced _update_ui():**
- hp_bar.value = player_hp, hp_bar.max_value = player_max_hp
- status_label.text with HP ratio, gold, level
- stats_label.text = "等级: %d | 击杀: %d | 探索: %d | XP: %d/%d"
- enemy_hp_bar visible/value/max_value based on in_encounter
- encounter_label with enemy info including text HP bar (same as original)
- backpack_count_label.text = "%d/%d" % [backpack.size(), max_backpack_slots]
- Update btn_sell_all.disabled and btn_use_herb.disabled states

**Enhanced _update_backpack_grid():**
- Same dynamic PanelContainer pattern as original
- Add rarity-colored border: get item rarity from ITEM_POOL, set sb.border_color to RARITY_COLORS[rarity], sb.border_width_* = 2
- Item cell shows icon (font_size 22), name + "x{count}" (font_size 11), and rarity-colored name label
- Tooltip on each cell: "{name}\n价值: {value} 金\n稀有度: {rarity}" via cell.tooltip_text

**_reset_game() enhanced:**
- Reset all new state (level, xp, kills, searches) back to defaults
- Log reset message

Total target: ~350-400 lines of GDScript.
  </action>
  <verify>
    <automated>test -f D:/Workspace/Godot/godot-mcp-meow/project/log_test_scene.gd && wc -l D:/Workspace/Godot/godot-mcp-meow/project/log_test_scene.gd | awk '{if ($1 >= 300) print "OK: "$1" lines"; else print "FAIL: only "$1" lines"}'</automated>
  </verify>
  <done>GDScript file exists with 300+ lines, containing enhanced game loop with BBCode log, XP/leveling, sell/use actions, rarity system, and tooltip interactions</done>
</task>

<task type="auto">
  <name>Task 2: Create enhanced scene tree with HP bars, extra buttons, and styled layout</name>
  <files>project/log_test_scene.tscn</files>
  <action>
Create `project/log_test_scene.tscn` as a Godot scene file (gd_scene format=3). Use ext_resource to reference `res://log_test_scene.gd`.

**Sub-resources (StyleBoxFlat):**
1. StyleBoxFlat_status: bg_color Color(0.15, 0.15, 0.22, 1), corner_radius 8 all
2. StyleBoxFlat_log: bg_color Color(0.1, 0.1, 0.16, 1), corner_radius 8 all
3. StyleBoxFlat_backpack_panel: bg_color Color(0.13, 0.13, 0.2, 1), corner_radius 8 all
4. StyleBoxFlat_action_panel: bg_color Color(0.14, 0.14, 0.21, 1), corner_radius 6 all

**Scene Tree Structure:**

```
LogTestScene (Control, full rect, script=ext_resource)
  Background (ColorRect, full rect, color=Color(0.08, 0.08, 0.12, 1))
  MainLayout (HBoxContainer, full rect with 16px margins, separation=16)
    LeftPanel (VBoxContainer, size_flags_horizontal=3, stretch_ratio=0.6, separation=10)
      StatusPanel (PanelContainer, StyleBoxFlat_status)
        StatusContent (VBoxContainer, separation=6)
          TitleLabel (Label, "搜打撤 Plus", font_size=28, color=gold Color(1,0.85,0.3,1), center)
          HPBarContainer (HBoxContainer, separation=8)
            HPLabel (Label, "HP:", font_size=16, color=Color(0.9,0.3,0.3,1))
            HPBar (ProgressBar, size_flags_horizontal=3, custom_minimum_size=Vector2(0,20), max=100, value=100)
          StatusLabel (Label, "HP: 100/100 | 金币: 0 | 等级: 1", font_size=16, color=white-ish)
          StatsLabel (Label, "击杀: 0 | 探索: 0 | XP: 0/50", font_size=14, color=Color(0.6,0.6,0.7,1))
          EncounterLabel (Label, "安全区域 - 准备探索", font_size=15, color=white-ish, autowrap=2)
          EnemyHPBar (ProgressBar, visible=false, custom_minimum_size=Vector2(0,16), max=100, value=100)
      ActionPanel (PanelContainer, StyleBoxFlat_action_panel)
        ActionContent (VBoxContainer, separation=6)
          ActionRow1 (HBoxContainer, separation=8)
            BtnSearch (Button, "搜 探索", font_size=16, min_size=Vector2(90,44), expand horizontal)
            BtnFight (Button, "打 战斗", font_size=16, min_size=Vector2(90,44), expand horizontal)
            BtnRetreat (Button, "撤 撤退", font_size=16, min_size=Vector2(90,44), expand horizontal)
          ActionRow2 (HBoxContainer, separation=8)
            BtnSellAll (Button, "卖 全部出售", font_size=14, min_size=Vector2(0,38), expand horizontal)
            BtnUseHerb (Button, "药 使用药草", font_size=14, min_size=Vector2(0,38), expand horizontal)
      LogPanel (PanelContainer, StyleBoxFlat_log, size_flags_vertical=3 expand+fill)
        LogLabel (RichTextLabel, bbcode_enabled=true, scroll_active=true, color=Color(0.85,0.85,0.9,1), font_size=13)
    RightPanel (VBoxContainer, size_flags_horizontal=3, stretch_ratio=0.4, separation=8)
      BackpackHeader (HBoxContainer, separation=0)
        BackpackTitle (Label, "背包", font_size=22, color=gold, size_flags_horizontal=3)
        BackpackCountLabel (Label, "0/16", font_size=16, color=Color(0.6,0.6,0.7,1))
      BackpackScroll (ScrollContainer, size_flags_vertical=3 expand+fill)
        BackpackGrid (GridContainer, columns=4, h_separation=6, v_separation=6, size_flags_horizontal=3)
```

**Key details:**
- Root Control: anchors_preset=15, anchor_right=1.0, anchor_bottom=1.0, grow_horizontal=2, grow_vertical=2
- MainLayout offsets: left=16, top=16, right=-16, bottom=-16
- ProgressBar nodes use default Godot styling (no custom stylebox needed, works out of box)
- All Labels use theme_override_colors/font_color and theme_override_font_sizes/font_size
- Buttons use size_flags_horizontal=3 (SIZE_EXPAND_FILL) for responsive width
- LogLabel: bbcode_enabled = true (critical for colored log entries)
- load_steps count must match: 1 ext_resource + 4 sub_resources = load_steps=6

Write the .tscn file directly in Godot's text scene format, following the exact patterns from the existing kcw_backpack.tscn.
  </action>
  <verify>
    <automated>test -f D:/Workspace/Godot/godot-mcp-meow/project/log_test_scene.tscn && grep -c "\[node" D:/Workspace/Godot/godot-mcp-meow/project/log_test_scene.tscn | awk '{if ($1 >= 20) print "OK: "$1" nodes"; else print "FAIL: only "$1" nodes"}'</automated>
  </verify>
  <done>Scene file exists with 20+ nodes, ext_resource references log_test_scene.gd, includes HPBar, EnemyHPBar, BtnSellAll, BtnUseHerb, BackpackCountLabel, and bbcode_enabled LogLabel</done>
</task>

</tasks>

<verification>
1. Both files exist on disk: `project/log_test_scene.gd` and `project/log_test_scene.tscn`
2. Scene references script via ext_resource
3. GDScript has all @onready references matching scene node paths
4. Scene has bbcode_enabled=true on LogLabel
5. Scene can be opened in Godot editor without parse errors (user verifies by opening)
</verification>

<success_criteria>
- log_test_scene.gd is 300+ lines with enhanced game loop (XP, leveling, rarity, sell/use, BBCode log)
- log_test_scene.tscn has 20+ nodes with HP bars, extra action buttons, backpack counter
- Scene runs standalone in Godot via F6 with full interactive game loop
- Log entries appear in different colors based on event type (combat, loot, level up)
- Backpack items show rarity-colored borders and hover tooltips
</success_criteria>

<output>
After completion, create `.planning/quick/260319-log-ui/260319-log-SUMMARY.md`
</output>
