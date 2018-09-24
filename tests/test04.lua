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
    assert(nil == mtstates.id())
    local s = mtstates.newstate(function()
                                    local mtstates = require("mtstates")
                                    return function(arg)
                                        return mtstates.id()
                                    end
                                end)
    print(s:call(), s:id())
    assert(s:call() == s:id())
end
PRINT("==================================================================================")
do
    local s = mtstates.newstate(function()
                                    local mtstates  = require("mtstates")
                                    local thisState = nil
                                    return function(cmd, arg)
                                        if cmd == "setstate" then
                                            thisState = mtstates.state(arg)
                                        elseif cmd == "add" then
                                            return 100 + arg
                                        elseif cmd == "add2" then
                                            return thisState:call("add", 1000 + arg)
                                        end
                                    end
                                end)
    local stateId = s:id()
    s:call("setstate", stateId)
    assert(s:call("add", 2) == 102)
    local _, err = pcall(function()
        s:call("add2", 3)
    end)
    assert(err:match (mtstates.error.cycle_detected))
    s = nil
    collectgarbage()
    assert(mtstates.state(stateId):id() == stateId)  -- memory leak :-(, DON'T DO THIS
end
PRINT("==================================================================================")
do
    local function state_setup()
        local mtstates  = require("mtstates")
        local otherState = nil
        return function(cmd, arg)
            if cmd == "setstate" then
                otherState = mtstates.state(arg)
            elseif cmd == "add" then
                return 100 + arg
            elseif cmd == "add2" then
                return otherState:call("add", 1000 + arg)
            elseif cmd == "add3" then
                return otherState:call("add2", 1000 + arg)
            end
        end
    end

    local s1 = mtstates.newstate(state_setup)
    local s2 = mtstates.newstate(state_setup)
    
    local id1 = s1:id()
    local id2 = s2:id()

    s1:call("setstate", id2)
    s2:call("setstate", id1)

    assert(s1:call("add",  2) ==  102)
    assert(s2:call("add",  3) ==  103)
    
    assert(s1:call("add2", 4) == 1104)
    local _, err = pcall(function()
        s1:call("add3", 5)
    end)
    assert(err:match (mtstates.error.cycle_detected))

    s1 = nil
    s2 = nil
    collectgarbage()
    assert(mtstates.state(id1):id() == id1)  -- memory leak :-(, DON'T DO THIS
    assert(mtstates.state(id2):id() == id2)  -- memory leak :-(, DON'T DO THIS
end
PRINT("==================================================================================")
print("OK.")
