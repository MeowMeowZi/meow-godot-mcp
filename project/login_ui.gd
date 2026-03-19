extends Control
## Login/Register UI - Demo login form with validation and mode switching.

var is_register_mode: bool = false
var VALID_USERS: Dictionary = {"admin": "123456", "test": "test"}

@onready var title_label: Label = %TitleLabel
@onready var username_input: LineEdit = %UsernameInput
@onready var password_input: LineEdit = %PasswordInput
@onready var confirm_password_container: VBoxContainer = %ConfirmPasswordContainer
@onready var confirm_password_input: LineEdit = %ConfirmPasswordInput
@onready var remember_check: CheckBox = %RememberCheck
@onready var login_button: Button = %LoginButton
@onready var switch_button: Button = %SwitchButton
@onready var message_label: Label = %MessageLabel


func _ready() -> void:
	login_button.pressed.connect(_on_login_pressed)
	switch_button.pressed.connect(_on_switch_pressed)
	username_input.text_submitted.connect(func(_t: String) -> void: _on_login_pressed())
	password_input.text_submitted.connect(func(_t: String) -> void: _on_login_pressed())

	password_input.secret = true
	confirm_password_input.secret = true
	confirm_password_container.visible = false

	username_input.placeholder_text = "请输入用户名"
	password_input.placeholder_text = "请输入密码"
	confirm_password_input.placeholder_text = "请确认密码"

	_update_mode()


func _on_login_pressed() -> void:
	var username := username_input.text.strip_edges()
	var password := password_input.text.strip_edges()

	if username.is_empty() or password.is_empty():
		_show_message("请填写用户名和密码", true)
		return

	if is_register_mode:
		var confirm := confirm_password_input.text.strip_edges()
		if password != confirm:
			_show_message("两次密码不一致", true)
			return
		if VALID_USERS.has(username):
			_show_message("用户名已存在", true)
			return
		VALID_USERS[username] = password
		_show_message("注册成功! 请登录", false)
		is_register_mode = false
		_update_mode()
		return

	# Login mode
	if VALID_USERS.has(username) and VALID_USERS[username] == password:
		_show_message("登录成功! 欢迎回来, " + username, false)
	else:
		_show_message("用户名或密码错误", true)


func _on_switch_pressed() -> void:
	is_register_mode = not is_register_mode
	_update_mode()
	message_label.text = ""


func _update_mode() -> void:
	if is_register_mode:
		title_label.text = "注册"
		login_button.text = "注 册"
		switch_button.text = "已有账号? 去登录"
		confirm_password_container.visible = true
	else:
		title_label.text = "登录"
		login_button.text = "登 录"
		switch_button.text = "没有账号? 去注册"
		confirm_password_container.visible = false


func _show_message(msg: String, is_error: bool = true) -> void:
	message_label.text = msg
	if is_error:
		message_label.add_theme_color_override("font_color", Color(1.0, 0.4, 0.4))
	else:
		message_label.add_theme_color_override("font_color", Color(0.4, 1.0, 0.5))

	var timer := get_tree().create_timer(3.0)
	timer.timeout.connect(func() -> void: message_label.text = "")
