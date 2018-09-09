local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtstates  = require("mtstates")

local function PRINT(s)
    print(s.." ("..debug.getinfo(2).currentline..")")
end
local function msgh(err)
    return debug.traceback(err, 2)
end
local function pcall(f, ...)
    return xpcall(f, msgh, ...)
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.unknown_object))
    
    local _, err = pcall(function() mtstates.state(firstId + 1) end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.unknown_object))

    local s2 = mtstates.newstate("foo", "return function() end")
    assert(s2:name() == "foo")
    assert(mtstates.state("foo"):id() == s2:id())

    threadIn:addmsg("stop")
    assert(threadOut:nextmsg() == "created")

    local _, err = pcall(function() mtstates.state("foo") end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.ambiguous_name))
    
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
    
    local ok = state:tcall(0.1);
    assert(not ok)
    local _, err = pcall (function() 
        state:close()
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.concurrent_access))

    stateIn:addmsg("stop")
    assert(threadOut:nextmsg() == "finished")
    
    stateIn:addmsg("foo")
    assert(state:call() == "foo")
    
    state:close()
    
    local _, err = pcall (function() 
        state:call()
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.object_closed))

end
PRINT("==================================================================================")
do
    local stateIn   = mtmsg.newbuffer()
    local stateOut  = mtmsg.newbuffer()
    local threadIn  = mtmsg.newbuffer()
    local threadOut = mtmsg.newbuffer()

    local state = mtstates.newstate(function(stateInId, stateOutId)
                                        local function msgh(err)
                                            return debug.traceback(err, 2)
                                        end
                                        local function pcall(f, ...)
                                            return xpcall(f, msgh, ...)
                                        end
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
                                            print("-- Expected error1:")
                                            print(err)
                                            print("-------------------------------------")
                                            assert(err:match(mtstates.error.interrupted))
                                            assert(not err:match(mtstates.error.invoking_state))
                                            local _, err = pcall(function()
                                                stateOut:addmsg("continue1")
                                                while true do stateIn:nextmsg(0.1) end
                                            end)
                                        end
                                    end,
                                    stateIn:id(), stateOut:id())
    
    local thread = llthreads.new(function(threadInId, threadOutId, stateId)
                                    local function msgh(err)
                                        return debug.traceback(err, 2)
                                    end
                                    local function pcall(f, ...)
                                        return xpcall(f, msgh, ...)
                                    end
                                    local mtstates  = require("mtstates")
                                    local mtmsg     = require("mtmsg")
                                    local threadIn  = mtmsg.buffer(threadInId)
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    local state     = mtstates.state(stateId)
                                    local _, err = pcall(function()
                                        state:call()
                                    end)
                                    print("-------------------------------------")
                                    print("-- Expected error2:")
                                    print(err)
                                    print("-------------------------------------")
                                    assert(err:match(mtstates.error.interrupted))
                                    assert(err:match(mtstates.error.invoking_state))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.interrupted))
    assert(err:match(mtstates.error.invoking_state))

    state:interrupt(false)
    
    assert(state:call("add5", 100) == 105)
    
    threadIn:addmsg("continue2")

    local ok = thread:join()
    assert(ok)
end
PRINT("==================================================================================")
do
    local threadIn   = mtmsg.newbuffer()
    local threadOut  = mtmsg.newbuffer()
    local stateOut   = mtmsg.newbuffer()

    local s = mtstates.newstate(function(stateOutId) 
        local mtmsg    = require("mtmsg")
        local stateOut = mtmsg.buffer(stateOutId)
        return function(cmd, arg)
            if cmd == "sleep" then
                mtmsg.sleep(arg)
            elseif cmd == "sleepa" then
                stateOut:addmsg("started")
                mtmsg.sleep(arg)
            elseif cmd == "add" then 
                return 100 + arg
            end
        end
    end, stateOut:id())

    local thread = llthreads.new(function(threadInId, threadOutId, sId)
                                    local mtstates  = require("mtstates")
                                    local mtmsg     = require("mtmsg")
                                    local threadIn  = mtmsg.buffer(threadInId)
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    local s         = mtstates.state(sId)
                                    while true do
                                        s:call(threadIn:nextmsg())
                                    end
                                end,
                                threadIn:id(), threadOut:id(), s:id())
    thread:start()
    
    local startTime = mtmsg.time()
    assert(s:call("sleep", 1) == nil)
    local diffTime = mtmsg.time() - startTime
    print(diffTime)
    assert(math.abs(diffTime - 1) < 0.1)

    local startTime = mtmsg.time()
    assert(s:call("sleep", 0.5) == nil)
    local diffTime = mtmsg.time() - startTime
    print(diffTime)
    assert(math.abs(diffTime - 0.5) < 0.1)
    
    
    threadIn:addmsg("sleepa", 1)
    assert(stateOut:nextmsg() == "started")
    local startTime = mtmsg.time()
    local ok, rslt = s:tcall(0.5, "add", 3)
    print(ok, rslt)
    local diffTime = mtmsg.time() - startTime
    print(diffTime)
    assert(not ok and rslt == nil)
    assert(math.abs(diffTime - 0.5) < 0.1)
    
    threadIn:addmsg("sleepa", 1)
    assert(stateOut:nextmsg() == "started")
    local startTime = mtmsg.time()
    local ok, rslt = s:tcall(1.5, "add", 4)
    print(ok, rslt)
    local diffTime = mtmsg.time() - startTime
    print(diffTime)
    assert(ok and rslt == 104)
    assert(math.abs(diffTime - 1) < 0.1)

    threadIn:addmsg("sleepa", 0.5)
    assert(stateOut:nextmsg() == "started")
    local startTime = mtmsg.time()
    local rslt = s:call("add", 5)
    print(rslt)
    local diffTime = mtmsg.time() - startTime
    print(diffTime)
    assert(rslt == 105)
    assert(math.abs(diffTime - 0.5) < 0.1)

    threadIn:addmsg("sleepa", 10)
    assert(stateOut:nextmsg() == "started")
    local startTime = mtmsg.time()
    mtmsg.abort(true)
    local ok, err = thread:join()
    local diffTime = mtmsg.time() - startTime
    print(diffTime)
    assert(math.abs(diffTime) < 0.1)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match(mtmsg.error.operation_aborted)
                  and err:match(mtstates.error.invoking_state))
    mtmsg.abort(false)
end
PRINT("==================================================================================")
print("OK.")
