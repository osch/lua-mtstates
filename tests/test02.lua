local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtstates  = require("mtstates")

local threadIn  = mtmsg.newbuffer()
local threadOut = mtmsg.newbuffer()

local firstId = mtstates.newstate(function() return (function() end) end):id()
print("firstId", firstId)

local thread = llthreads.new(function(threadInId, threadOutId)
                                local mtstates  = require("mtstates")
                                local mtmsg     = require("mtmsg")
                                local threadIn  = mtmsg.buffer(threadInId)
                                local threadOut = mtmsg.buffer(threadOutId)
                                threadOut:addmsg("started")
                                local s = mtstates.newstate(
                                    "foo",
                                    function(threadInId)
                                        local mtmsg    = require("mtmsg")
                                        local threadIn = mtmsg.buffer(threadInId)
                                        assert(threadIn:nextmsg() == "stop")
                                        return function() return "ok" end
                                    end,
                                    threadInId
                                )
                                threadOut:addmsg("created")
                                assert(threadIn:nextmsg() == "stop")
                                threadOut:addmsg("finished")
                            end,
                            threadIn:id(), threadOut:id())
thread:start()

assert(threadOut:nextmsg() == "started")
mtmsg.sleep(0.5)

local _, err = pcall (function() mtstates.newstate("foo", function() end) end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mtstates.error.object_exists)

local _, err = pcall(function() mtstates.state("foo") end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mtstates.error.unknown_object)

local _, err = pcall(function() mtstates.state(firstId + 1) end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mtstates.error.unknown_object)

threadIn:addmsg("stop")
assert(threadOut:nextmsg() == "created")
local s = mtstates.state("foo")
threadIn:addmsg("stop")
assert(s:call() == "ok")
assert(threadOut:nextmsg() == "finished")
print("Done.")
