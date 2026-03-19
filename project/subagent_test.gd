extends Control

var click_count: int = 0

func _ready() -> void:
	print("Subagent MCP Test: Scene loaded successfully!")

func _on_test_button_pressed() -> void:
	click_count += 1
	$Layout/StatusLabel.text = "MCP works! Clicked %d time(s)" % click_count
	print("Button pressed! Count: %d" % click_count)
