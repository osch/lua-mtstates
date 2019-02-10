package = "mtstates"
version = "0.4.1-1"
local versionNumber = version:gsub("^(.*)-.-$", "%1")
source = {
  url = "https://github.com/osch/lua-mtstates/archive/v"..versionNumber..".zip",
  dir = "lua-mtstates-"..versionNumber,
}
description = {
  summary = "Multi-threading Lua states",
  homepage = "https://github.com/osch/lua-mtstates",
  license = "MIT/X11",
  detailed = [[
    This package provides a way to create new Lua states from within Lua for using
    them in arbitrary threads. The implementation is independent from the
    underlying threading library (e.g. Lanes or lua-llthreads2).
  ]],
}
dependencies = {
  "lua >= 5.1, < 5.4",
}
build = {
  type = "builtin",
  platforms = {
    unix = {
      modules = {
        mtstates = {
          libraries = {"pthread"},
        }
      }
    },
    windows = {
      modules = {
        mtstates = {
          libraries = {"kernel32"},
        }
      }
    }
  },
  modules = {
    mtstates = {
      sources = { 
          "src/main.c",
          "src/state.c",
          "src/error.c",
          "src/util.c",
          "src/async_util.c",
          "src/mtstates_compat.c",
      },
      defines = { "MTSTATES_VERSION="..versionNumber },
    },
  }
}