--[[
    This examples demonstrates the usage of the *Notify C API* in a multithreading scenario.
    
    Mtstates' state objects implement the *Notify C API*, see [src/notify_capi.h](../src/notify_capi.h),
    i.e. the state object has an an associated meta table entry *_capi_notify* delivered by
    the C API function *notify_get_capi()* and the associated C API function *toNotifier()* returns
    a valid pointer for a given state object.
    
    In this example the *Notify C API* is used to notify a state object by invoking the state's 
    callback function without arguments each time a message is added to a [mtmsg](https://github.com/osch/lua-mtmsg)
    buffer object. 
    
    This is done by connecting the state object as a notifier object to the mtmsg buffer 
    object.
--]]

local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtstates  = require("mtstates")

local buffer = mtmsg.newbuffer()

local state = mtstates.newstate(function(bufferId)
                                    local mtmsg = require("mtmsg")
                                    local buffer = mtmsg.buffer(bufferId)
                                    local list = {}
                                    local cmds = {
                                        add = function(arg) list[#list + 1] = arg end,
                                        get = function() return table.concat(list, " ") end
                                    }
                                    return function(cmd, ...)
                                        if cmd then
                                            return cmds[cmd](...)
                                        else 
                                            assert(select("#", ...) == 0)
                                            local a1, a2 = buffer:nextmsg()
                                            print("State received", a1, a2)
                                            list[#list + 1] = a1
                                            list[#list + 1] = a2
                                        end
                                    end
                                end, buffer:id())

buffer:notifier(state) -- state will be notified each time a message is added to the buffer

local thread = llthreads.new(function(bufferId)
                                 local mtmsg  = require("mtmsg")
                                 local buffer = mtmsg.buffer(bufferId)
                                 for i = 2, 4 do
                                    print("Thread puts into buffer  ", i)
                                    buffer:addmsg("fromThread:", i)
                                 end
                             end,
                             buffer:id())

state:call("add", "init")

buffer:addmsg("fromMain:", 1)

print("Starting Thread")
thread:start()
thread:join()
print("Thread finished")
buffer:addmsg("fromMain:", 5)
local list = state:call("get")
print("list:", list)
assert(list == "init fromMain: 1 fromThread: 2 fromThread: 3 fromThread: 4 fromMain: 5")

