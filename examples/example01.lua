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
