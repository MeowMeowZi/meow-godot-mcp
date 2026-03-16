#!/usr/bin/env python

import os

env = SConscript("godot-cpp/SConstruct")

# Add include paths for project source and third-party headers
env.Append(CPPPATH=["src/", "thirdparty/"])

# GDExtension library target
sources = Glob("src/*.cpp")

library = env.SharedLibrary(
    "project/addons/godot_mcp_meow/bin/libgodot_mcp_meow{}{}".format(
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
    "project/addons/godot_mcp_meow/bin/godot-mcp-bridge",
    source=bridge_sources,
)
Alias("bridge", bridge)
