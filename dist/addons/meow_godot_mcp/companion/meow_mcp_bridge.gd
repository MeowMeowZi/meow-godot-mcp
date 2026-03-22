# meow_mcp_bridge.gd -- Game-side MCP bridge companion autoload
# Runs in the GAME process. Receives commands from editor via EngineDebugger,
# executes input injection and viewport capture, sends results back.
extends Node

func _ready():
	if not EngineDebugger.is_active():
		queue_free()
		return
	EngineDebugger.register_message_capture("meow_mcp", _on_message)
	EngineDebugger.send_message("meow_mcp:ready", [])

func _exit_tree():
	if EngineDebugger.is_active():
		EngineDebugger.unregister_message_capture("meow_mcp")

func _on_message(message: String, data: Array) -> bool:
	match message:
		"inject_key":
			return _inject_key(data)
		"inject_mouse_click":
			return _inject_mouse_click(data)
		"inject_mouse_move":
			return _inject_mouse_move(data)
		"inject_mouse_scroll":
			return _inject_mouse_scroll(data)
		"inject_action":
			return _inject_action(data)
		"capture_viewport":
			return _capture_viewport()
	return false

func _inject_key(data: Array) -> bool:
	var keycode_str: String = data[0]
	var pressed: bool = data[1]
	var event = InputEventKey.new()
	event.keycode = OS.find_keycode_from_string(keycode_str)
	event.physical_keycode = event.keycode
	event.pressed = pressed
	Input.parse_input_event(event)
	EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
	return true

func _inject_mouse_click(data: Array) -> bool:
	var x: float = data[0]
	var y: float = data[1]
	var button_str: String = data[2]
	var pressed: bool = data[3]
	var button_map = {
		"left": MOUSE_BUTTON_LEFT,
		"right": MOUSE_BUTTON_RIGHT,
		"middle": MOUSE_BUTTON_MIDDLE
	}
	var event = InputEventMouseButton.new()
	event.position = Vector2(x, y)
	event.global_position = Vector2(x, y)
	event.button_index = button_map.get(button_str, MOUSE_BUTTON_LEFT)
	event.pressed = pressed
	Input.parse_input_event(event)
	EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
	return true

func _inject_mouse_move(data: Array) -> bool:
	var x: float = data[0]
	var y: float = data[1]
	var event = InputEventMouseMotion.new()
	event.position = Vector2(x, y)
	event.global_position = Vector2(x, y)
	Input.parse_input_event(event)
	EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
	return true

func _inject_mouse_scroll(data: Array) -> bool:
	var x: float = data[0]
	var y: float = data[1]
	var direction_str: String = data[2]
	var event = InputEventMouseButton.new()
	event.position = Vector2(x, y)
	event.global_position = Vector2(x, y)
	event.button_index = MOUSE_BUTTON_WHEEL_UP if direction_str == "up" else MOUSE_BUTTON_WHEEL_DOWN
	event.pressed = true
	Input.parse_input_event(event)
	EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
	return true

func _inject_action(data: Array) -> bool:
	var action_name: String = data[0]
	var pressed: bool = data[1]
	var event = InputEventAction.new()
	event.action = action_name
	event.pressed = pressed
	event.strength = 1.0 if pressed else 0.0
	Input.parse_input_event(event)
	EngineDebugger.send_message("meow_mcp:input_result", [true, ""])
	return true

func _capture_viewport() -> bool:
	var image = get_viewport().get_texture().get_image()
	if image == null or image.is_empty():
		EngineDebugger.send_message("meow_mcp:viewport_data", ["", 0, 0])
		return true
	var png_data = image.save_png_to_buffer()
	var base64_str = Marshalls.raw_to_base64(png_data)
	EngineDebugger.send_message("meow_mcp:viewport_data",
		[base64_str, image.get_width(), image.get_height()])
	return true
