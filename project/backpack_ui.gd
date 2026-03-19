extends Control

## 搜打撤背包 (Search-Fight-Retreat Backpack) -- Complete Roguelike Game UI
## Features: BBCode colored log, XP/leveling, item rarity borders, sell/use actions, HP bars, tooltips.

# ---------- Constants ----------

var ITEM_POOL := [
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
]

var ENEMY_POOL := [
	{"name": "史莱姆", "hp": 30, "attack": 5, "loot_count": 1},
	{"name": "哥布林", "hp": 50, "attack": 10, "loot_count": 2},
	{"name": "暗影狼", "hp": 60, "attack": 20, "loot_count": 2},
	{"name": "火蜥蜴", "hp": 70, "attack": 25, "loot_count": 2},
	{"name": "骷髅兵", "hp": 80, "attack": 15, "loot_count": 3},
	{"name": "巨型蜘蛛", "hp": 100, "attack": 12, "loot_count": 4},
	{"name": "石像鬼", "hp": 120, "attack": 18, "loot_count": 3},
]

var RARITY_COLORS := {
	"common": Color(0.7, 0.7, 0.7),
	"uncommon": Color(0.3, 1.0, 0.3),
	"rare": Color(0.3, 0.5, 1.0),
	"epic": Color(0.8, 0.3, 1.0),
}

# ---------- Game State ----------

var player_hp: int = 100
var player_max_hp: int = 100
var gold: int = 0
var player_attack_base: int = 15
var player_level: int = 1
var player_xp: int = 0
var xp_to_next: int = 50
var backpack: Array[Dictionary] = []
var max_backpack_slots: int = 16
var in_encounter: bool = false
var encounter_enemy: Dictionary = {}
var round_counter: int = 0
var total_kills: int = 0
var total_searches: int = 0

# ---------- Node References ----------

@onready var status_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/StatusLabel
@onready var encounter_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/EncounterLabel
@onready var stats_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/StatsLabel
@onready var hp_bar: ProgressBar = $MainLayout/LeftPanel/StatusPanel/StatusContent/HPBarContainer/HPBar
@onready var enemy_hp_bar: ProgressBar = $MainLayout/LeftPanel/StatusPanel/StatusContent/EnemyHPBar
@onready var log_label: RichTextLabel = $MainLayout/LeftPanel/LogPanel/LogLabel
@onready var backpack_grid: GridContainer = $MainLayout/RightPanel/BackpackScroll/BackpackGrid
@onready var backpack_count_label: Label = $MainLayout/RightPanel/BackpackHeader/BackpackCountLabel
@onready var btn_search: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow1/BtnSearch
@onready var btn_fight: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow1/BtnFight
@onready var btn_retreat: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow1/BtnRetreat
@onready var btn_sell_all: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow2/BtnSellAll
@onready var btn_use_herb: Button = $MainLayout/LeftPanel/ActionPanel/ActionContent/ActionRow2/BtnUseHerb

# ---------- Lifecycle ----------

func _ready() -> void:
	btn_search.pressed.connect(_on_search)
	btn_fight.pressed.connect(_on_fight)
	btn_retreat.pressed.connect(_on_retreat)
	btn_sell_all.pressed.connect(_on_sell_all)
	btn_use_herb.pressed.connect(_on_use_herb)
	hp_bar.max_value = player_max_hp
	hp_bar.value = player_hp
	enemy_hp_bar.visible = false
	btn_fight.disabled = true
	btn_retreat.disabled = true
	log_label.bbcode_enabled = true
	_update_ui()
	_add_colored_log("欢迎来到搜打撤! 点击[搜]开始探索。", Color(1.0, 0.85, 0.3))


# ---------- Actions ----------

func _on_search() -> void:
	if in_encounter:
		return
	round_counter += 1
	total_searches += 1
	var roll := randf()
	if roll < 0.7:
		# Find 1-3 random items
		var item_count := randi_range(1, 3)
		var found_names: PackedStringArray = PackedStringArray()
		for i in range(item_count):
			var item: Dictionary = ITEM_POOL[randi_range(0, ITEM_POOL.size() - 1)]
			_add_to_backpack(item.name, item.icon, item.rarity)
			found_names.append(item.name)
		_add_colored_log("探索发现: %s" % ", ".join(found_names), Color(1.0, 0.9, 0.3))
	else:
		# Encounter random enemy
		var template: Dictionary = ENEMY_POOL[randi_range(0, ENEMY_POOL.size() - 1)]
		encounter_enemy = {
			"name": template.name,
			"hp": template.hp,
			"max_hp": template.hp,
			"attack": template.attack,
			"loot_count": template.loot_count,
		}
		in_encounter = true
		_add_colored_log("遭遇了 %s! HP: %d/%d 攻击: %d" % [
			encounter_enemy.name,
			encounter_enemy.hp,
			encounter_enemy.max_hp,
			encounter_enemy.attack,
		], Color(1.0, 0.4, 0.4))
		enemy_hp_bar.visible = true
		enemy_hp_bar.max_value = encounter_enemy.max_hp
		enemy_hp_bar.value = encounter_enemy.hp
	_update_button_states()
	_update_ui()


func _on_fight() -> void:
	if not in_encounter:
		return
	round_counter += 1
	# Player attacks
	var player_dmg: int = randi_range(player_attack_base - 5, player_attack_base + 10)
	encounter_enemy.hp = maxi(encounter_enemy.hp - player_dmg, 0)
	_add_colored_log("你对 %s 造成了 %d 点伤害!" % [encounter_enemy.name, player_dmg], Color(0.3, 1.0, 0.3))

	# Enemy attacks back
	var enemy_dmg: int = randi_range(maxi(1, encounter_enemy.attack - 3), encounter_enemy.attack + 3)
	player_hp = maxi(player_hp - enemy_dmg, 0)
	_add_colored_log("%s 对你造成了 %d 点伤害!" % [encounter_enemy.name, enemy_dmg], Color(1.0, 0.3, 0.3))

	if encounter_enemy.hp <= 0:
		# Victory
		total_kills += 1
		var gold_gain: int = encounter_enemy.loot_count * 10 + randi_range(0, 20)
		gold += gold_gain
		_add_colored_log("击败了 %s! 获得 %d 金币!" % [encounter_enemy.name, gold_gain], Color(1.0, 0.9, 0.3))
		# XP gain
		var xp_gain: int = int(encounter_enemy.max_hp / 2)
		player_xp += xp_gain
		_add_log("获得 %d 经验值" % xp_gain)
		_check_level_up()
		# Loot items from defeated enemy
		var loot_names: PackedStringArray = PackedStringArray()
		for i in range(encounter_enemy.loot_count):
			var item: Dictionary = ITEM_POOL[randi_range(0, ITEM_POOL.size() - 1)]
			_add_to_backpack(item.name, item.icon, item.rarity)
			loot_names.append(item.name)
		if loot_names.size() > 0:
			_add_colored_log("战利品: %s" % ", ".join(loot_names), Color(1.0, 0.9, 0.3))
		in_encounter = false
		encounter_enemy = {}
		enemy_hp_bar.visible = false
	elif player_hp <= 0:
		# Game over
		_add_colored_log("你倒下了... 游戏重置中...", Color(1.0, 0.3, 0.3))
		_reset_game()

	_update_button_states()
	_update_ui()


func _on_retreat() -> void:
	if not in_encounter:
		return
	round_counter += 1
	var roll := randf()
	if roll < 0.8:
		# Successful retreat
		var gold_loss: int = randi_range(gold / 10, gold / 5)
		gold_loss = maxi(gold_loss, 0)
		gold = maxi(gold - gold_loss, 0)
		in_encounter = false
		encounter_enemy = {}
		enemy_hp_bar.visible = false
		if gold_loss > 0:
			_add_colored_log("成功撤退! 但丢失了 %d 金币。" % gold_loss, Color(0.6, 0.6, 0.6))
		else:
			_add_colored_log("成功撤退!", Color(0.6, 0.6, 0.6))
	else:
		# Failed retreat -- enemy gets free hit
		var enemy_dmg: int = randi_range(maxi(1, encounter_enemy.attack - 3), encounter_enemy.attack + 3)
		player_hp = maxi(player_hp - enemy_dmg, 0)
		_add_colored_log("撤退失败! %s 趁机攻击造成 %d 点伤害!" % [encounter_enemy.name, enemy_dmg], Color(1.0, 0.3, 0.3))
		if player_hp <= 0:
			_add_colored_log("你倒下了... 游戏重置中...", Color(1.0, 0.3, 0.3))
			_reset_game()
	_update_button_states()
	_update_ui()


func _on_sell_all() -> void:
	if backpack.is_empty():
		return
	var total_value: int = 0
	for item in backpack:
		var item_value: int = _get_item_value(item.name)
		total_value += item_value * item.count
	gold += total_value
	backpack.clear()
	_add_colored_log("出售全部物品! 获得 %d 金币!" % total_value, Color(1.0, 0.9, 0.3))
	_update_ui()


func _on_use_herb() -> void:
	if player_hp >= player_max_hp:
		_add_colored_log("没有药草或HP已满", Color(0.6, 0.6, 0.6))
		return
	var herb_index: int = -1
	for i in range(backpack.size()):
		if backpack[i].name == "药草":
			herb_index = i
			break
	if herb_index == -1:
		_add_colored_log("没有药草或HP已满", Color(0.6, 0.6, 0.6))
		return
	var heal_amount: int = mini(25, player_max_hp - player_hp)
	player_hp += heal_amount
	backpack[herb_index].count -= 1
	if backpack[herb_index].count <= 0:
		backpack.remove_at(herb_index)
	_add_colored_log("使用药草恢复了 %d 点 HP!" % heal_amount, Color(0.3, 1.0, 0.3))
	_update_ui()


# ---------- Level System ----------

func _check_level_up() -> void:
	while player_xp >= xp_to_next:
		player_xp -= xp_to_next
		player_level += 1
		player_attack_base += 3
		player_max_hp += 15
		player_hp = player_max_hp
		xp_to_next = int(xp_to_next * 1.5)
		_add_colored_log("升级! 等级 %d! 攻击力+3 最大HP+15" % player_level, Color(0.8, 0.3, 1.0))


# ---------- Inventory ----------

func _add_to_backpack(item_name: String, icon: String, rarity: String) -> void:
	# Check if item already exists in backpack -- increment count
	for i in range(backpack.size()):
		if backpack[i].name == item_name:
			backpack[i].count += 1
			return
	# New item -- check slot capacity
	if backpack.size() >= max_backpack_slots:
		_add_colored_log("背包已满!", Color(1.0, 0.3, 0.3))
		return
	backpack.append({"name": item_name, "icon": icon, "count": 1, "rarity": rarity})


# ---------- UI Updates ----------

func _update_ui() -> void:
	# HP bar
	hp_bar.max_value = player_max_hp
	hp_bar.value = player_hp

	# Status line
	status_label.text = "HP: %d/%d | 金币: %d | 等级: %d" % [player_hp, player_max_hp, gold, player_level]

	# Stats line
	stats_label.text = "击杀: %d | 探索: %d | XP: %d/%d" % [total_kills, total_searches, player_xp, xp_to_next]

	# Encounter info
	if in_encounter:
		encounter_label.text = "遭遇: %s HP: %d/%d 攻击: %d" % [
			encounter_enemy.name,
			encounter_enemy.hp,
			encounter_enemy.max_hp,
			encounter_enemy.attack,
		]
		enemy_hp_bar.visible = true
		enemy_hp_bar.max_value = encounter_enemy.max_hp
		enemy_hp_bar.value = encounter_enemy.hp
	else:
		encounter_label.text = "安全区域 - 准备探索"
		enemy_hp_bar.visible = false

	# Backpack count
	backpack_count_label.text = "%d/%d" % [backpack.size(), max_backpack_slots]

	# Button states
	_update_button_states()

	# Sell/Use button states
	btn_sell_all.disabled = backpack.is_empty()
	btn_use_herb.disabled = not _has_herb() or player_hp >= player_max_hp

	# Backpack grid
	_update_backpack_grid()


func _has_herb() -> bool:
	for item in backpack:
		if item.name == "药草":
			return true
	return false


func _update_button_states() -> void:
	btn_search.disabled = in_encounter
	btn_fight.disabled = not in_encounter
	btn_retreat.disabled = not in_encounter


func _update_backpack_grid() -> void:
	# Clear existing children
	for c in backpack_grid.get_children():
		c.queue_free()

	# Add item cells
	for item in backpack:
		var cell := PanelContainer.new()
		cell.custom_minimum_size = Vector2(80, 80)
		var sb := StyleBoxFlat.new()
		sb.bg_color = Color(0.2, 0.22, 0.28, 1.0)
		sb.corner_radius_top_left = 4
		sb.corner_radius_top_right = 4
		sb.corner_radius_bottom_left = 4
		sb.corner_radius_bottom_right = 4
		# Rarity-colored border
		var rarity_color: Color = RARITY_COLORS.get(item.rarity, Color(0.7, 0.7, 0.7))
		sb.border_color = rarity_color
		sb.border_width_left = 2
		sb.border_width_top = 2
		sb.border_width_right = 2
		sb.border_width_bottom = 2
		cell.add_theme_stylebox_override("panel", sb)
		# Tooltip with item info
		var item_value: int = _get_item_value(item.name)
		cell.tooltip_text = "%s\n价值: %d 金\n稀有度: %s" % [item.name, item_value, item.rarity]
		# Inner layout
		var vbox := VBoxContainer.new()
		vbox.alignment = BoxContainer.ALIGNMENT_CENTER
		var icon_label := Label.new()
		icon_label.text = item.icon
		icon_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		icon_label.add_theme_font_size_override("font_size", 22)
		var name_label := Label.new()
		name_label.text = "%s x%d" % [item.name, item.count]
		name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		name_label.add_theme_font_size_override("font_size", 11)
		name_label.add_theme_color_override("font_color", rarity_color)
		vbox.add_child(icon_label)
		vbox.add_child(name_label)
		cell.add_child(vbox)
		backpack_grid.add_child(cell)

	# Fill remaining empty slots
	var empty_count := max_backpack_slots - backpack.size()
	for i in range(empty_count):
		var cell := PanelContainer.new()
		cell.custom_minimum_size = Vector2(80, 80)
		var sb := StyleBoxFlat.new()
		sb.bg_color = Color(0.15, 0.15, 0.2, 0.5)
		sb.corner_radius_top_left = 4
		sb.corner_radius_top_right = 4
		sb.corner_radius_bottom_left = 4
		sb.corner_radius_bottom_right = 4
		cell.add_theme_stylebox_override("panel", sb)
		backpack_grid.add_child(cell)


func _get_item_value(item_name: String) -> int:
	for pool_item in ITEM_POOL:
		if pool_item.name == item_name:
			return pool_item.value
	return 0


# ---------- Log ----------

func _add_log(msg: String) -> void:
	_add_colored_log(msg, Color(0.85, 0.85, 0.9))


func _add_colored_log(msg: String, color: Color) -> void:
	var round_hex := Color(0.5, 0.5, 0.5).to_html(false)
	var msg_hex := color.to_html(false)
	var entry := "[color=#%s][R%d][/color] [color=#%s]%s[/color]" % [round_hex, round_counter, msg_hex, msg]
	log_label.text += "\n" + entry if log_label.text.length() > 0 else entry
	# Keep last 100 lines
	if log_label.get_line_count() > 100:
		var full_text := log_label.text
		var first_newline := full_text.find("\n")
		if first_newline >= 0:
			log_label.text = full_text.substr(first_newline + 1)
	# Auto-scroll to bottom
	await get_tree().process_frame
	log_label.scroll_to_line(log_label.get_line_count() - 1)


# ---------- Reset ----------

func _reset_game() -> void:
	player_hp = 100
	player_max_hp = 100
	gold = 0
	player_attack_base = 15
	player_level = 1
	player_xp = 0
	xp_to_next = 50
	backpack.clear()
	in_encounter = false
	encounter_enemy = {}
	round_counter = 0
	total_kills = 0
	total_searches = 0
	enemy_hp_bar.visible = false
	btn_fight.disabled = true
	btn_retreat.disabled = true
	btn_search.disabled = false
	_update_ui()
	_add_colored_log("游戏重置 - 重新开始冒险!", Color(1.0, 0.85, 0.3))
