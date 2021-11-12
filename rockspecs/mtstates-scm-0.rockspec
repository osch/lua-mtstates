package = "mtstates"
version = "scm-0"
source = {
  url = "https://github.com/osch/lua-mtstates/archive/master.zip",
  dir = "lua-mtstates-master",
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
  "lua >= 5.1, <= 5.4",
}
build = {
  type = "builtin",
  platforms = {
    linux = {
      modules = {
        mtstates = {
          libraries = {"pthread"},
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
          "src/notify_capi_impl.c",
          "src/receiver_capi_impl.c",
          "src/async_util.c",
          "src/mtstates_compat.c",
      },
      defines = { "MTSTATES_VERSION="..version:gsub("^(.*)-.-$", "%1") },
    },
  }
}