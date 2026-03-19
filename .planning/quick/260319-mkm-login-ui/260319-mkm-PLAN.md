---
phase: quick
plan: 260319-mkm
type: execute
wave: 1
depends_on: []
files_modified:
  - project/login_ui.gd
autonomous: true
requirements: [quick-task]

must_haves:
  truths:
    - "Scene displays a centered login form with username and password fields"
    - "Password field masks input with bullet characters"
    - "Login button validates non-empty fields and shows success/error feedback"
    - "Register button switches to a registration form with confirm password field"
    - "Remember Me checkbox persists visual toggle state"
    - "UI has a polished dark theme with accent colors"
  artifacts:
    - path: "project/login_ui.gd"
      provides: "Login/register form logic with validation and UI state switching"
      min_lines: 100
  key_links:
    - from: "login_ui scene (built via MCP)"
      to: "project/login_ui.gd"
      via: "script attachment on root node"
      pattern: "script.*login_ui"
---

<objective>
Create a polished login/register UI test scene for Godot 4.6 using MCP tools to build the scene interactively in the editor.

Purpose: Demonstrate a classic login interface pattern with form validation, login/register mode switching, and dark-themed styling. This is a standalone test scene.

Output: A GDScript file (login_ui.gd) written via the Write tool, and a complete scene tree built interactively via Godot MCP tools, then saved to disk.
</objective>

<execution_context>
@C:/Users/28186/.claude/get-shit-done/workflows/execute-plan.md
@C:/Users/28186/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@project/project.godot (Godot 4.6, window config)
</context>

<tasks>

<task type="auto">
  <name>Task 1: Write the login UI script</name>
  <files>project/login_ui.gd</files>
  <action>
Create `project/login_ui.gd` extending Control. This script manages a login/register form with validation and mode switching.

**State variables:**
- `is_register_mode: bool = false`
- `VALID_USERS: Dictionary` -- hardcoded demo accounts: `{"admin": "123456", "test": "test"}`

**@onready node references (these MUST match the scene tree built in Task 2):**
- `title_label: Label` at `%TitleLabel`
- `username_input: LineEdit` at `%UsernameInput`
- `password_input: LineEdit` at `%PasswordInput`
- `confirm_password_container: VBoxContainer` at `%ConfirmPasswordContainer`
- `confirm_password_input: LineEdit` at `%ConfirmPasswordInput`
- `remember_check: CheckBox` at `%RememberCheck`
- `login_button: Button` at `%LoginButton`
- `switch_button: Button` at `%SwitchButton`
- `message_label: Label` at `%MessageLabel`

Use `unique_name_in_owner = true` pattern (% prefix) so node paths are resilient to hierarchy changes.

**_ready():**
- Connect signals:
  - `login_button.pressed.connect(_on_login_pressed)`
  - `switch_button.pressed.connect(_on_switch_pressed)`
  - `username_input.text_submitted.connect(func(_t): _on_login_pressed())`
  - `password_input.text_submitted.connect(func(_t): _on_login_pressed())`
- Set `password_input.secret = true`
- Set `confirm_password_input.secret = true`
- Hide confirm_password_container initially (`confirm_password_container.visible = false`)
- Set placeholder texts: username_input.placeholder_text = "请输入用户名", password_input.placeholder_text = "请输入密码", confirm_password_input.placeholder_text = "请确认密码"
- Call `_update_mode()`

**_on_login_pressed():**
- Get trimmed text from username_input and password_input
- Validate both non-empty, show error "请填写用户名和密码" via `_show_message(text, is_error)` if empty
- If `is_register_mode`:
  - Check confirm_password matches password, show error "两次密码不一致" if mismatch
  - Check username not already in VALID_USERS, show error "用户名已存在" if duplicate
  - Add to VALID_USERS dict, show success "注册成功! 请登录", switch to login mode
- If login mode:
  - Check username exists in VALID_USERS and password matches
  - Show success "登录成功! 欢迎回来, {username}" with green color
  - Show error "用户名或密码错误" if invalid

**_on_switch_pressed():**
- Toggle `is_register_mode`
- Call `_update_mode()`
- Clear message_label

**_update_mode():**
- If register mode:
  - title_label.text = "注册"
  - login_button.text = "注 册"
  - switch_button.text = "已有账号? 去登录"
  - confirm_password_container.visible = true
- If login mode:
  - title_label.text = "登录"
  - login_button.text = "登 录"
  - switch_button.text = "没有账号? 去注册"
  - confirm_password_container.visible = false

**_show_message(msg: String, is_error: bool = true):**
- Set message_label.text = msg
- Set font color override: red Color(1.0, 0.4, 0.4) for error, green Color(0.4, 1.0, 0.5) for success
- Create a Timer to clear message after 3 seconds:
  ```
  var timer := get_tree().create_timer(3.0)
  timer.timeout.connect(func(): message_label.text = "")
  ```

Use GDScript 4.x syntax throughout (typed vars, @onready, lambdas, StringName signals).
  </action>
  <verify>
    <automated>cd D:/Workspace/Godot/godot-mcp-meow && test -f project/login_ui.gd && wc -l project/login_ui.gd | awk '{if ($1 >= 80) print "PASS: "$1" lines"; else print "FAIL: only "$1" lines"}'</automated>
  </verify>
  <done>login_ui.gd exists with 80+ lines, contains _on_login_pressed (validation logic), _on_switch_pressed (mode toggle), _update_mode (UI state), _show_message (feedback display)</done>
</task>

<task type="auto">
  <name>Task 2: Build login UI scene via MCP tools</name>
  <files>project/login_ui.tscn (created via MCP save)</files>
  <action>
Use Godot MCP tools to build the login scene interactively. Execute these MCP calls in sequence:

**Step 1: Create scene with root node**
- `mcp__godot__create_scene` with root_type="Control", root_name="LoginUI"
- `mcp__godot__set_layout_preset` on "/root/LoginUI" preset="full_rect"

**Step 2: Background**
- `mcp__godot__create_node` parent="/root/LoginUI", node_name="Background", node_type="ColorRect"
- `mcp__godot__set_layout_preset` on Background, preset="full_rect"
- `mcp__godot__set_node_property` Background color = Color(0.08, 0.08, 0.12, 1.0)

**Step 3: CenterContainer for the form card**
- `mcp__godot__create_node` parent="/root/LoginUI", node_name="CenterContainer", node_type="CenterContainer"
- `mcp__godot__set_layout_preset` on CenterContainer, preset="full_rect"

**Step 4: FormCard (PanelContainer) inside CenterContainer**
- `mcp__godot__create_node` parent CenterContainer, node_name="FormCard", node_type="PanelContainer"
- `mcp__godot__set_node_property` FormCard custom_minimum_size = Vector2(400, 0)
- `mcp__godot__create_stylebox` target FormCard, override_name="panel", bg_color="Color(0.14, 0.14, 0.2, 1.0)", corner_radius=12, content_margin_left=32, content_margin_right=32, content_margin_top=32, content_margin_bottom=32

**Step 5: FormLayout (VBoxContainer) inside FormCard**
- `mcp__godot__create_node` parent FormCard, node_name="FormLayout", node_type="VBoxContainer"
- `mcp__godot__set_container_layout` on FormLayout, separation=16

**Step 6: TitleLabel inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="TitleLabel", node_type="Label"
- `mcp__godot__set_node_property` TitleLabel text="登录", horizontal_alignment=1 (center)
- `mcp__godot__set_node_property` TitleLabel unique_name_in_owner=true
- `mcp__godot__set_theme_override` TitleLabel overrides={"font_size": 28, "font_color": "Color(0.95, 0.85, 0.4, 1.0)"}

**Step 7: UsernameInput (LineEdit) inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="UsernameInput", node_type="LineEdit"
- `mcp__godot__set_node_property` UsernameInput placeholder_text="请输入用户名", unique_name_in_owner=true
- `mcp__godot__set_node_property` UsernameInput custom_minimum_size=Vector2(0, 40)

**Step 8: PasswordInput (LineEdit) inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="PasswordInput", node_type="LineEdit"
- `mcp__godot__set_node_property` PasswordInput placeholder_text="请输入密码", secret=true, unique_name_in_owner=true
- `mcp__godot__set_node_property` PasswordInput custom_minimum_size=Vector2(0, 40)

**Step 9: ConfirmPasswordContainer (VBoxContainer) inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="ConfirmPasswordContainer", node_type="VBoxContainer"
- `mcp__godot__set_node_property` ConfirmPasswordContainer unique_name_in_owner=true, visible=false

**Step 10: ConfirmPasswordInput (LineEdit) inside ConfirmPasswordContainer**
- `mcp__godot__create_node` parent ConfirmPasswordContainer, node_name="ConfirmPasswordInput", node_type="LineEdit"
- `mcp__godot__set_node_property` ConfirmPasswordInput placeholder_text="请确认密码", secret=true, unique_name_in_owner=true
- `mcp__godot__set_node_property` ConfirmPasswordInput custom_minimum_size=Vector2(0, 40)

**Step 11: RememberCheck (CheckBox) inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="RememberCheck", node_type="CheckBox"
- `mcp__godot__set_node_property` RememberCheck text="记住我", unique_name_in_owner=true

**Step 12: LoginButton (Button) inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="LoginButton", node_type="Button"
- `mcp__godot__set_node_property` LoginButton text="登 录", unique_name_in_owner=true
- `mcp__godot__set_node_property` LoginButton custom_minimum_size=Vector2(0, 45)
- `mcp__godot__set_theme_override` LoginButton overrides={"font_size": 18}
- `mcp__godot__create_stylebox` target LoginButton, override_name="normal", bg_color="Color(0.2, 0.45, 0.85, 1.0)", corner_radius=8
- `mcp__godot__create_stylebox` target LoginButton, override_name="hover", bg_color="Color(0.25, 0.5, 0.95, 1.0)", corner_radius=8
- `mcp__godot__create_stylebox` target LoginButton, override_name="pressed", bg_color="Color(0.15, 0.35, 0.75, 1.0)", corner_radius=8

**Step 13: SwitchButton (Button, flat/link style) inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="SwitchButton", node_type="Button"
- `mcp__godot__set_node_property` SwitchButton text="没有账号? 去注册", flat=true, unique_name_in_owner=true
- `mcp__godot__set_theme_override` SwitchButton overrides={"font_color": "Color(0.5, 0.7, 1.0, 1.0)", "font_size": 14}

**Step 14: MessageLabel inside FormLayout**
- `mcp__godot__create_node` parent FormLayout, node_name="MessageLabel", node_type="Label"
- `mcp__godot__set_node_property` MessageLabel text="", horizontal_alignment=1 (center), unique_name_in_owner=true
- `mcp__godot__set_theme_override` MessageLabel overrides={"font_size": 14}

**Step 15: Attach script and save**
- `mcp__godot__write_script` or verify project/login_ui.gd is on disk (from Task 1)
- `mcp__godot__attach_script` on "/root/LoginUI" script_path="res://login_ui.gd"
- `mcp__godot__save_scene` path="res://login_ui.tscn"

**Final scene tree:**
```
LoginUI (Control) -- script: login_ui.gd, full_rect
  Background (ColorRect) -- full_rect, dark bg
  CenterContainer (CenterContainer) -- full_rect
    FormCard (PanelContainer) -- min width 400, rounded dark card
      FormLayout (VBoxContainer) -- separation 16
        TitleLabel (Label) -- "登录", gold color, centered, unique
        UsernameInput (LineEdit) -- placeholder, unique
        PasswordInput (LineEdit) -- secret, unique
        ConfirmPasswordContainer (VBoxContainer) -- hidden initially, unique
          ConfirmPasswordInput (LineEdit) -- secret, unique
        RememberCheck (CheckBox) -- "记住我", unique
        LoginButton (Button) -- blue styled, unique
        SwitchButton (Button) -- flat/link style, unique
        MessageLabel (Label) -- feedback text, unique
```

IMPORTANT: All nodes with unique_name_in_owner=true so the script can reference them via %NodeName syntax.
  </action>
  <verify>
    <automated>cd D:/Workspace/Godot/godot-mcp-meow && test -f project/login_ui.tscn && echo "PASS: login_ui.tscn exists" || echo "FAIL: login_ui.tscn not found"</automated>
  </verify>
  <done>Login UI scene saved to project/login_ui.tscn via MCP tools, contains all form nodes (TitleLabel, UsernameInput, PasswordInput, ConfirmPasswordContainer, ConfirmPasswordInput, RememberCheck, LoginButton, SwitchButton, MessageLabel) with dark theme styling, script attached</done>
</task>

</tasks>

<verification>
1. Script file exists: `project/login_ui.gd` with validation logic
2. Scene file exists: `project/login_ui.tscn` saved via MCP
3. All nodes have unique_name_in_owner=true matching script % references
4. Scene can be run in editor: shows centered login card on dark background
5. Run with `mcp__godot__run_game` to visually confirm the login form renders
</verification>

<success_criteria>
- login_ui.gd contains login/register mode switching, form validation, success/error feedback
- login_ui.tscn built via MCP tools with centered card layout, dark theme, blue login button
- Typing "admin" / "123456" and clicking login shows success message
- Clicking "没有账号? 去注册" switches to register mode with confirm password field
- Scene runs standalone without errors
</success_criteria>

<output>
After completion, create `.planning/quick/260319-mkm-login-ui/260319-mkm-SUMMARY.md`
</output>
