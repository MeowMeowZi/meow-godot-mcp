#!/usr/bin/env python

import os

env = SConscript("godot-cpp/SConstruct")

# Add include paths for project source and third-party headers
env.Append(CPPPATH=["src/", "thirdparty/"])

# Enable C++ exception handling (required by nlohmann/json)
if env["platform"] == "windows":
    env.Append(CXXFLAGS=["/EHsc"])

# Enable Godot-dependent code paths in dual-mode modules (e.g., variant_parser)
env.Append(CPPDEFINES=["MEOW_GODOT_MCP_GODOT_ENABLED"])

# GDExtension library target
sources = Glob("src/*.cpp")

library = env.SharedLibrary(
    "project/addons/meow_godot_mcp/bin/libmeow_godot_mcp{}{}".format(
        env["suffix"], env["SHLIBSUFFIX"]
    ),
    source=sources,
)

Default(library)

# Bridge executable target
# The bridge is a standalone executable that relays stdio <-> TCP.
# It does NOT link against godot-cpp (no Godot dependency).
# Build with: scons bridge
bridge_env = Environment()

# C++17 standard
if bridge_env["PLATFORM"] == "win32":
    bridge_env.Append(CXXFLAGS=["/std:c++17", "/EHsc"])
    bridge_env.Append(LIBS=["ws2_32"])
else:
    bridge_env.Append(CXXFLAGS=["-std=c++17"])
    bridge_env.Append(LIBS=["pthread"])

bridge_env.Append(CPPPATH=["bridge/"])

bridge_sources = Glob("bridge/*.cpp")
bridge = bridge_env.Program(
    "project/addons/meow_godot_mcp/bin/godot-mcp-bridge",
    source=bridge_sources,
)
Alias("bridge", bridge)
