---
phase: quick
plan: 260319-nzt
type: execute
wave: 1
depends_on: []
files_modified: [project/hello_world.tscn, project/hello_world.gd]
autonomous: true
requirements: [quick-task]
must_haves:
  truths:
    - "A scene named hello_world.tscn exists with a Label displaying 'Hello World'"
    - "The Label is centered on screen"
    - "A script prints 'Hello World' to console on _ready"
  artifacts:
    - path: "project/hello_world.tscn"
      provides: "HelloWorld scene file"
    - path: "project/hello_world.gd"
      provides: "Script that prints Hello World on ready"
  key_links:
    - from: "hello_world.gd"
      to: "Label node"
      via: "attach_script"
---

<objective>
Create a minimal HelloWorld test scene using Godot MCP tools.

Purpose: Demonstrate MCP tool usage for scene construction -- a Label centered on screen saying "Hello World" with a script that prints to console.
Output: project/hello_world.tscn, project/hello_world.gd
</objective>

<execution_context>
@C:/Users/28186/.claude/get-shit-done/workflows/execute-plan.md
@C:/Users/28186/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@CLAUDE.md
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create HelloWorld scene with centered Label via MCP tools</name>
  <files>project/hello_world.tscn, project/hello_world.gd</files>
  <action>
Use Godot MCP tools in this exact sequence:

1. **Create the scene** with a Control root node:
   - `mcp__godot__create_scene` with node_type="Control", node_name="HelloWorld"

2. **Set root to full rect** so it fills the viewport:
   - `mcp__godot__set_layout_preset` with node_path="HelloWorld", preset="full_rect"

3. **Add a Label node** as child of root:
   - `mcp__godot__create_node` with parent_path="HelloWorld", node_type="Label", node_name="HelloLabel"

4. **Set the Label text**:
   - `mcp__godot__set_node_property` with node_path="HelloWorld/HelloLabel", property="text", value="Hello World"

5. **Center the Label** on screen:
   - `mcp__godot__set_layout_preset` with node_path="HelloWorld/HelloLabel", preset="center"

6. **Set font size** so text is clearly visible:
   - `mcp__godot__set_theme_override` with node_path="HelloWorld/HelloLabel", overrides={"font_size": 48}

7. **Write the script** that prints on ready:
   - `mcp__godot__write_script` with path="res://hello_world.gd", content:
     ```gdscript
     extends Control

     func _ready() -> void:
         print("Hello World")
     ```

8. **Attach the script** to the root node:
   - `mcp__godot__attach_script` with node_path="HelloWorld", script_path="res://hello_world.gd"

9. **Save the scene**:
   - `mcp__godot__save_scene` with path="res://hello_world.tscn"

Do NOT write .tscn files directly. All scene construction MUST go through MCP tool calls.
  </action>
  <verify>
    <automated>
Run the scene to visually confirm:
- `mcp__godot__run_game` with scene_path="res://hello_world.tscn"
- Wait 2 seconds for the scene to load
- `mcp__godot__capture_game_viewport` to screenshot and verify "Hello World" is visible on screen
- `mcp__godot__stop_game`
    </automated>
  </verify>
  <done>
- hello_world.tscn exists with Control root and centered Label child
- Label displays "Hello World" in large (48pt) font
- Script attached to root prints "Hello World" to console on _ready
- Scene runs without errors
  </done>
</task>

</tasks>

<verification>
- Scene file saved at project/hello_world.tscn
- Script file saved at project/hello_world.gd
- Running the scene shows "Hello World" centered on screen
- Console output includes "Hello World" from _ready
</verification>

<success_criteria>
A minimal HelloWorld scene exists that displays centered text and prints to console, built entirely via MCP tool calls.
</success_criteria>

<output>
After completion, create `.planning/quick/260319-nzt-mcp-helloworld/260319-nzt-SUMMARY.md`
</output>
