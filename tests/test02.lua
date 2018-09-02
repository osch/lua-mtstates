local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtstates  = require("mtstates")

local function PRINT(s)
    print(s.." ("..debug.getinfo(2).currentline..")")
end

PRINT("==================================================================================")
do
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
    mtmsg.sleep(0.2)
    
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

    local s2 = mtstates.newstate("foo", "return function() end")
    assert(s2:name() == "foo")
    assert(mtstates.state("foo"):id() == s2:id())

    threadIn:addmsg("stop")
    assert(threadOut:nextmsg() == "created")

    local _, err = pcall(function() mtstates.state("foo") end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mtstates.error.ambiguous_name)
    
    s2 = nil
    collectgarbage()

    local s = mtstates.state("foo")
    threadIn:addmsg("stop")
    assert(s:call() == "ok")
    assert(threadOut:nextmsg() == "finished")
    print("Done.")
end
PRINT("==================================================================================")
do
    local stateIn   = mtmsg.newbuffer()
    local threadOut = mtmsg.newbuffer()

    local state = mtstates.newstate(function(stateInId)
                                        local mtmsg   = require("mtmsg")
                                        local stateIn = mtmsg.buffer(stateInId)
                                        return function()
                                            while true do
                                                local cmd = stateIn:nextmsg(0.1)
                                                if cmd then 
                                                    return cmd
                                                end
                                            end
                                        end
                                    end,
                                    stateIn:id())
    
    local thread = llthreads.new(function(threadOutId, stateId)
                                    local mtstates  = require("mtstates")
                                    local mtmsg     = require("mtmsg")
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    local state     = mtstates.state(stateId)
                                    threadOut:addmsg("started")
                                    assert(state:call() == "stop")
                                    threadOut:addmsg("finished")
                                end,
                                threadOut:id(), state:id())
    thread:start()
    
    assert(threadOut:nextmsg() == "started")
    mtmsg.sleep(0.2)
    
    local _, err = pcall (function() 
        state:call()
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mtstates.error.concurrent_access)
    
    local _, err = pcall (function() 
        state:close()
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mtstates.error.concurrent_access)

    stateIn:addmsg("stop")
    assert(threadOut:nextmsg() == "finished")
    
    stateIn:addmsg("foo")
    assert(state:call() == "foo")
    
    state:close()
    
    local _, err = pcall (function() 
        state:call()
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mtstates.error.object_closed)

end
PRINT("==================================================================================")
do
    local stateIn   = mtmsg.newbuffer()
    local stateOut  = mtmsg.newbuffer()
    local threadIn  = mtmsg.newbuffer()
    local threadOut = mtmsg.newbuffer()

    local state = mtstates.newstate(function(stateInId, stateOutId)
                                        local mtmsg    = require("mtmsg")
                                        local mtstates = require("mtstates")
                                        local stateIn  = mtmsg.buffer(stateInId)
                                        local stateOut = mtmsg.buffer(stateOutId)
                                        return function(cmd, arg)
                                            if cmd == "add5" then
                                                return arg + 5
                                            end
                                            local _, err = pcall(function()
                                                stateOut:addmsg("started")
                                                while true do stateIn:nextmsg(0.1) end
                                            end)
                                            print("-------------------------------------")
                                            print("-- Expected error:")
                                            print(err)
                                            print("-- Expected details:")
                                            print(err:details())
                                            print("-------------------------------------")
                                            assert(err == mtstates.error.interrupted)
                                            local _, err = pcall(function()
                                                stateOut:addmsg("continue1")
                                                while true do stateIn:nextmsg(0.1) end
                                            end)
                                        end
                                    end,
                                    stateIn:id(), stateOut:id())
    
    local thread = llthreads.new(function(threadInId, threadOutId, stateId)
                                    local mtstates  = require("mtstates")
                                    local mtmsg     = require("mtmsg")
                                    local threadIn  = mtmsg.buffer(threadInId)
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    local state     = mtstates.state(stateId)
                                    local _, err = pcall(function()
                                        state:call()
                                    end)
                                    print("-------------------------------------")
                                    print("-- Expected error:")
                                    print(err)
                                    print("-- Expected details:")
                                    print(err:details())
                                    print("-------------------------------------")
                                    assert(err == mtstates.error.interrupted)
                                    threadOut:addmsg("interrupted")
                                    assert(threadIn:nextmsg() == "continue2")
                                    assert(state:call("add5", 200) == 205)
                                end,
                                threadIn:id(), threadOut:id(), state:id())
    thread:start()
    
    assert(stateOut:nextmsg() == "started")

    state:interrupt()

    assert(stateOut:nextmsg() == "continue1")

    state:interrupt(true)
    
    assert(threadOut:nextmsg() == "interrupted")
    
    local _, err = pcall(function()
        state:call("add5", 10)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-- Expected details:")
    print(err:details())
    print("-------------------------------------")
    assert(err == mtstates.error.interrupted)

    state:interrupt(false)
    
    assert(state:call("add5", 100) == 105)
    
    threadIn:addmsg("continue2")

    local ok = thread:join()
    assert(ok)
end
PRINT("==================================================================================")
print("OK.")
