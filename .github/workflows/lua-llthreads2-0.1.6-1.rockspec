package = "lua-llthreads2"
version = "0.1.6-1"
source = {
  url = "https://github.com/moteus/lua-llthreads2/archive/v0.1.6.zip",
  dir = "lua-llthreads2-0.1.6",
}
description = {
  summary = "Low-Level threads for Lua",
  homepage = "http://github.com/moteus/lua-llthreads2",
  license = "MIT/X11",
  detailed = [[
    This is drop-in replacement for `lua-llthread` module but the module called `llthreads2`.
    In additional module supports: thread join  with zero timeout; logging thread errors with 
    custom logger; run detached joinable threads; pass cfunctions as argument to child thread.
  ]],
}
dependencies = {
  "lua >= 5.1, < 5.5",
}
build = {
  type = "builtin",
  platforms = {
    linux = {
      modules = {
        llthreads2 = {
          libraries = {"pthread"},
        }
      }
    },
    windows = {
      modules = {
        llthreads2 = {
          libraries = {"kernel32"},
        }
      }
    },
  },
  modules = {
    llthreads2 = {
      sources = { "src/l52util.c", "src/llthread.c" },
      defines = { "LLTHREAD_MODULE_NAME=llthreads2" },
    },
    ["llthreads2.ex"] = "src/lua/llthreads2/ex.lua",
  }
}