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

# Bridge executable target (placeholder for Plan 03)
# Bridge source files will be created in Plan 03.
# Uncomment the following when bridge/*.cpp files exist:
#
# bridge_sources = Glob("bridge/*.cpp")
# if bridge_sources:
#     bridge_env = env.Clone()
#     # Bridge is a standalone executable, not a Godot extension
#     bridge = bridge_env.Program(
#         "project/addons/godot_mcp_meow/bin/godot-mcp-bridge",
#         source=bridge_sources,
#     )
