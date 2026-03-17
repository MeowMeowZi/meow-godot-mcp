---
status: testing
phase: 03-script-project-management
source: [03-01-SUMMARY.md, 03-02-SUMMARY.md]
started: 2026-03-17T03:00:00Z
updated: 2026-03-17T03:00:00Z
---

## Current Test
<!-- OVERWRITE each test - shows where we are -->

number: 1
name: Read Script
expected: |
  Call `read_script` with a valid .gd file path (e.g., res://test_root.gd or any existing script).
  Returns JSON with the file content as a string and a line_count integer.
awaiting: user response

## Tests

### 1. Read Script
expected: Call `read_script` with a valid .gd file path. Returns JSON with the file content as a string and a line_count integer.
result: [pending]

### 2. Write Script (new file)
expected: Call `write_script` with path `res://test_uat_script.gd` and valid GDScript content. Returns success. File appears in Godot FileSystem dock. Calling `write_script` again with the same path returns an error (file already exists).
result: [pending]

### 3. Edit Script - Insert Line
expected: Call `edit_script` with operation "insert" on `res://test_uat_script.gd`, specifying a line number and content. The new line appears at the specified position, pushing existing lines down. Verify with `read_script`.
result: [pending]

### 4. Edit Script - Replace Line
expected: Call `edit_script` with operation "replace" on the same file, specifying a line number and new content. That line's content changes to the new value. Verify with `read_script`.
result: [pending]

### 5. Edit Script - Delete Line
expected: Call `edit_script` with operation "delete" on the same file, specifying a line number. The line is removed and line_count decreases by 1. Verify with `read_script`.
result: [pending]

### 6. Attach Script to Node
expected: Call `attach_script` on a node (e.g., the root Node2D) with a valid script path. The script icon appears on the node in the Scene dock. The operation is undoable (Ctrl+Z removes the script).
result: [pending]

### 7. Detach Script from Node
expected: Call `detach_script` on the node that has a script attached. The script icon disappears from the node. The operation is undoable (Ctrl+Z re-attaches the script).
result: [pending]

### 8. List Project Files
expected: Call `list_project_files` with no arguments. Returns a JSON array of objects, each with `path` and `type` fields, listing files in the project directory.
result: [pending]

### 9. Get Project Settings
expected: Call `get_project_settings` with no arguments. Returns structured JSON containing project settings, including `application/config/name` with the project name.
result: [pending]

### 10. Get Resource Info
expected: Call `get_resource_info` on a .tscn or .tres file (e.g., `res://test_root.tscn`). Returns JSON with the resource type and its properties.
result: [pending]

### 11. MCP Resources List
expected: Send `resources/list` MCP method. Returns a list containing 2 resources: `godot://scene_tree` and `godot://project_files`.
result: [pending]

### 12. MCP Resources Read
expected: Send `resources/read` for both `godot://scene_tree` and `godot://project_files`. Each returns a JSON contents array with the resource data (scene tree hierarchy and file listing respectively).
result: [pending]

### 13. IO Thread Active
expected: Check Godot Output log for "IO thread started" message when plugin activates. Rapid successive MCP calls complete without blocking the editor.
result: [pending]

### 14. Phase 2 Regression
expected: Phase 2 scene tools still work: `get_scene_tree` returns the scene hierarchy, `create_node` adds a node, `set_node_property` changes a property, `delete_node` removes a node. No regressions from Phase 3 changes.
result: [pending]

## Summary

total: 14
passed: 0
issues: 0
pending: 14
skipped: 0

## Gaps

[none yet]
