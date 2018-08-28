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
                                     local cmd, stateId, arg = threadIn:nextmsg()
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

threadIn:addmsg("finish")
assert(threadOut:nextmsg() == "finished")
assert(thread:join())
