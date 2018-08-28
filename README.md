# mtstates 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Build Status](https://travis-ci.org/osch/lua-mtstates.svg?branch=master)](https://travis-ci.org/osch/lua-mtstates)
[![Build status](https://ci.appveyor.com/api/projects/status/v8t6rsf45dwt60pl?svg=true)](https://ci.appveyor.com/project/osch/lua-mtstates)

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

## Examples

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

Concurrent access to the same Lua state from different threads is prohibited and
leads to an error. Therefore state access has to be synchronised by the caller.
The following example uses  [mtmsg]( https://github.com/osch/lua-mtmsg#mtmsg) to
synchronise acces to the state:

```lua
local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtstates  = require("mtstates")

local state = mtstates.newstate(function()
                                    local list = {}
                                    local cmds = {
                                        add = function(arg) list[#list + 1] = arg end,
                                        get = function() return table.concat(list, " ") end
                                    }
                                    return function(cmd, ...)
                                        return cmds[cmd](...)
                                    end
                                end)

local threadIn  = mtmsg.newbuffer()
local threadOut = mtmsg.newbuffer()

local thread = llthreads.new(function(threadInId, threadOutId)
                                 local mtstates  = require("mtstates")
                                 local mtmsg     = require("mtmsg")
                                 local threadIn  = mtmsg.buffer(threadInId)
                                 local threadOut = mtmsg.buffer(threadOutId)
                                 while true do
                                     local cmd, stateId, arg = buffer:nextmsg()
                                     if cmd == "finish" then
                                         threadOut:addmsg("finished")
                                         break
                                     else
                                        local state = mtstates.state(stateId)
                                        state:call(cmd, arg)
                                        threadOut:addmsg("done")
                                    end
                                 end
                             end,
                             threadIn:id(), threadOut:id())
thread:start()

threadIn:addmsg("add", state:id(), "Hello")
threadIn:addmsg("add", state:id(), "World")

assert(threadOut:nextmsg() == "done")
assert(threadOut:nextmsg() == "done")

assert("Hello World" == state:call("get"))

threadIn:addmsg(state:id(), "finished")
assert(threadOut:nextmsg() == "finished")
assert(thread:join())
```



<!-- ---------------------------------------------------------------------------------------- -->

## Todo

more to come....

