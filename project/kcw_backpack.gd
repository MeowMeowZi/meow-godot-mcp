extends Control

## 搜打撤背包 -- Search-Fight-Retreat Backpack Game
## A roguelike loop prototype with inventory management.

# ---------- Constants ----------

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

var ENEMY_POOL := [
	{"name": "史莱姆", "hp": 30, "attack": 5, "loot_count": 1},
	{"name": "哥布林", "hp": 50, "attack": 10, "loot_count": 2},
	{"name": "骷髅兵", "hp": 80, "attack": 15, "loot_count": 3},
	{"name": "暗影狼", "hp": 60, "attack": 20, "loot_count": 2},
	{"name": "巨型蜘蛛", "hp": 100, "attack": 12, "loot_count": 4},
]

# ---------- Game State ----------

var player_hp: int = 100
var player_max_hp: int = 100
var gold: int = 0
var backpack: Array[Dictionary] = []
var max_backpack_slots: int = 16
var in_encounter: bool = false
var encounter_enemy: Dictionary = {}
var log_messages: PackedStringArray = PackedStringArray()
var round_counter: int = 0

# ---------- Node References ----------

@onready var status_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/StatusLabel
@onready var encounter_label: Label = $MainLayout/LeftPanel/StatusPanel/StatusContent/EncounterLabel
@onready var log_label: RichTextLabel = $MainLayout/LeftPanel/LogPanel/LogLabel
@onready var backpack_grid: GridContainer = $MainLayout/RightPanel/BackpackScroll/BackpackGrid
@onready var btn_search: Button = $MainLayout/LeftPanel/ActionPanel/BtnSearch
@onready var btn_fight: Button = $MainLayout/LeftPanel/ActionPanel/BtnFight
@onready var btn_retreat: Button = $MainLayout/LeftPanel/ActionPanel/BtnRetreat

# ---------- Lifecycle ----------

func _ready() -> void:
	btn_search.pressed.connect(_on_search)
	btn_fight.pressed.connect(_on_fight)
	btn_retreat.pressed.connect(_on_retreat)
	btn_fight.disabled = true
	btn_retreat.disabled = true
	_update_ui()
	_add_log("欢迎来到搜打撤! 点击[搜]开始探索。")


# ---------- Actions ----------

func _on_search() -> void:
	if in_encounter:
		return
	round_counter += 1
	var roll := randf()
	if roll < 0.7:
		# Find items
		var item_count := randi_range(1, 3)
		var found_names: PackedStringArray = PackedStringArray()
		for i in range(item_count):
			var item: Dictionary = ITEM_POOL[randi_range(0, ITEM_POOL.size() - 1)]
			_add_to_backpack(item.name, item.icon)
			found_names.append(item.name)
		_add_log("探索发现: %s" % ", ".join(found_names))
	else:
		# Encounter enemy
		var template: Dictionary = ENEMY_POOL[randi_range(0, ENEMY_POOL.size() - 1)]
		encounter_enemy = {
			"name": template.name,
			"hp": template.hp,
			"max_hp": template.hp,
			"attack": template.attack,
			"loot_count": template.loot_count,
		}
		in_encounter = true
		_add_log("遭遇了 %s! HP: %d/%d 攻击: %d" % [
			encounter_enemy.name,
			encounter_enemy.hp,
			encounter_enemy.max_hp,
			encounter_enemy.attack,
		])
	_update_button_states()
	_update_ui()


func _on_fight() -> void:
	if not in_encounter:
		return
	round_counter += 1
	# Player attacks
	var player_dmg: int = randi_range(10, 25)
	encounter_enemy.hp = maxi(encounter_enemy.hp - player_dmg, 0)
	_add_log("你对 %s 造成了 %d 点伤害!" % [encounter_enemy.name, player_dmg])

	# Enemy attacks
	var enemy_dmg: int = maxi(encounter_enemy.attack + randi_range(-3, 3), 1)
	player_hp = maxi(player_hp - enemy_dmg, 0)
	_add_log("%s 对你造成了 %d 点伤害!" % [encounter_enemy.name, enemy_dmg])

	if encounter_enemy.hp <= 0:
		# Victory
		var gold_gain: int = encounter_enemy.loot_count * 10 + randi_range(0, 20)
		gold += gold_gain
		_add_log("击败了 %s! 获得 %d 金币!" % [encounter_enemy.name, gold_gain])
		# Loot items
		var loot_names: PackedStringArray = PackedStringArray()
		for i in range(encounter_enemy.loot_count):
			var item: Dictionary = ITEM_POOL[randi_range(0, ITEM_POOL.size() - 1)]
			_add_to_backpack(item.name, item.icon)
			loot_names.append(item.name)
		if loot_names.size() > 0:
			_add_log("战利品: %s" % ", ".join(loot_names))
		in_encounter = false
		encounter_enemy = {}
	elif player_hp <= 0:
		# Game over
		_add_log("你倒下了... 点击[搜]重新开始")
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
		var gold_loss: int = int(gold * randf_range(0.1, 0.2))
		gold = maxi(gold - gold_loss, 0)
		in_encounter = false
		encounter_enemy = {}
		if gold_loss > 0:
			_add_log("成功撤退! 但丢失了 %d 金币。" % gold_loss)
		else:
			_add_log("成功撤退!")
	else:
		# Failed retreat -- enemy gets free hit
		var enemy_dmg: int = maxi(encounter_enemy.attack + randi_range(-3, 3), 1)
		player_hp = maxi(player_hp - enemy_dmg, 0)
		_add_log("撤退失败! %s 趁机攻击造成 %d 点伤害!" % [encounter_enemy.name, enemy_dmg])
		if player_hp <= 0:
			_add_log("你倒下了... 点击[搜]重新开始")
			_reset_game()
	_update_button_states()
	_update_ui()


# ---------- Inventory ----------

func _add_to_backpack(item_name: String, icon: String) -> void:
	# Check if item already exists in backpack
	for i in range(backpack.size()):
		if backpack[i].name == item_name:
			backpack[i].count += 1
			return
	# New item
	if backpack.size() >= max_backpack_slots:
		_add_log("背包已满!")
		return
	backpack.append({"name": item_name, "icon": icon, "count": 1})


# ---------- UI Updates ----------

func _update_ui() -> void:
	# Status
	status_label.text = "HP: %d/%d | 金币: %d" % [player_hp, player_max_hp, gold]

	# Encounter info
	if in_encounter:
		var hp_bar := ""
		var hp_ratio := float(encounter_enemy.hp) / float(encounter_enemy.max_hp)
		var filled := int(hp_ratio * 10)
		for i in range(10):
			if i < filled:
				hp_bar += "█"
			else:
				hp_bar += "░"
		encounter_label.text = "遭遇: %s [%s] HP: %d/%d" % [
			encounter_enemy.name,
			hp_bar,
			encounter_enemy.hp,
			encounter_enemy.max_hp,
		]
	else:
		encounter_label.text = "安全区域 - 准备探索"

	# Backpack grid
	_update_backpack_grid()


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

	# Add empty slots
	var empty_count := max_backpack_slots - backpack.size()
	for i in range(empty_count):
		var cell := PanelContainer.new()
		cell.custom_minimum_size = Vector2(80, 80)
		var sb := StyleBoxFlat.new()
		sb.bg_color = Color(0.15, 0.16, 0.2, 0.5)
		sb.corner_radius_top_left = 4
		sb.corner_radius_top_right = 4
		sb.corner_radius_bottom_left = 4
		sb.corner_radius_bottom_right = 4
		cell.add_theme_stylebox_override("panel", sb)
		backpack_grid.add_child(cell)


func _update_button_states() -> void:
	btn_search.disabled = in_encounter
	btn_fight.disabled = not in_encounter
	btn_retreat.disabled = not in_encounter


# ---------- Log ----------

func _add_log(msg: String) -> void:
	var entry := "[%d] %s" % [round_counter, msg]
	log_messages.append(entry)
	# Keep last 50 messages
	if log_messages.size() > 50:
		log_messages = log_messages.slice(log_messages.size() - 50)
	log_label.text = "\n".join(log_messages)
	# Auto-scroll to bottom
	await get_tree().process_frame
	log_label.scroll_to_line(log_label.get_line_count() - 1)


# ---------- Reset ----------

func _reset_game() -> void:
	player_hp = player_max_hp
	gold = 0
	backpack.clear()
	in_encounter = false
	encounter_enemy = {}
	_update_ui()
