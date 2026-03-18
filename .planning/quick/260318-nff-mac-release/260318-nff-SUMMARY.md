# Quick Task 260318-nff: 构建 macOS 动态库并上传到 Release

## Summary

Successfully built macOS (and Linux) dynamic libraries via GitHub Actions CI and uploaded all platform packages to both Gitee and GitHub releases.

## What was done

### 1. Fixed cross-platform build errors
- **Problem**: Code used C++ `try/catch` which fails on Linux/macOS because godot-cpp disables exceptions (`-fno-exceptions`)
- **Fix**: Replaced all `try/catch` in 4 files with non-exception alternatives:
  - `mcp_protocol.cpp`: `json::parse(..., nullptr, false)` + `is_discarded()`
  - `variant_parser.cpp`: `strtoll()`/`strtod()` instead of `std::stoll()`/`std::stod()`
  - `mcp_server.cpp`: Removed catch-all exception wrapper in IO loop
  - `ui_tools.cpp`: `strtol()` for integer parsing

### 2. Set up GitHub mirror for CI
- Created GitHub repo: https://github.com/MeowMeowZi/meow-godot-mcp
- Added `github` remote to local repo
- Pushed code and v1.0 tag to trigger GitHub Actions CI

### 3. CI build results
All 3 platforms + unit tests passed:
- macOS universal (arm64 + x86_64): template_release + template_debug + bridge
- Linux x86_64: template_release + template_debug + bridge
- Windows x86_64: template_release + template_debug + bridge
- Unit tests: passed

### 4. Release uploads

**Gitee Release** (https://gitee.com/MeowMeowZi/meow-godot-mcp/releases/tag/v1.0):
- `meow-godot-mcp-v1.0.zip` (all platforms, 4.2MB)
- `meow-godot-mcp-v1.0-macos-universal.zip` (997KB)
- `meow-godot-mcp-v1.0-linux-x86_64.zip` (2.2MB)
- `meow-godot-mcp-v1.0-windows-x86_64.zip` (1.1MB)

**GitHub Release** (https://github.com/MeowMeowZi/meow-godot-mcp/releases/tag/v1.0):
- Same 4 zip files

## Commits
- `7cd0884` - fix: remove try/catch for cross-platform compatibility
