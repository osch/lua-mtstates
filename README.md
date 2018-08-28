# mtstates 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)

<!-- ---------------------------------------------------------------------------------------- -->

Invoking interpreter states from multiple threads for the [Lua] scripting language.

This package provides a way to create new Lua states from within Lua for using
them in arbitrary threads. The implementation is independent from the
underlying threading library (e.g. [Lanes] or [lua-llthreads2]).

The general principle is to prepare a state that returns a callback function that can be
called from different threads. This can be useful in a thread-pool scenario when the
number of states exceeds the number of available threads.

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2

<!-- ---------------------------------------------------------------------------------------- -->

## Example

Simple example: a state is constructed whose callback function is called from a
parallel thread and from the main thread. The state is passed by integer id to the
created thread.

```lua
local llthreads = require("llthreads2.ex")
local mtstates  = require("mtstates")

local state = mtstates.newstate(function(init)
                                    local list = { init }
                                    local cmds = {
                                        add = function(arg) list[#list + 1] = arg end,
                                        get = function() return table.concat(list, " ") end
                                    }
                                    return function(cmd, ...)
                                        return cmds[cmd](...)
                                    end
                                end,
                                "Hello")

local thread = llthreads.new(function(stateId)
                                 local mtstates = require("mtstates")
                                 local state    = mtstates.state(stateId)
                                 return state:call("add", "World")
                             end,
                             state:id())
thread:start()
thread:join()
assert("Hello World" == state:call("get"))
```

<!-- ---------------------------------------------------------------------------------------- -->

## Todo

more to come....

