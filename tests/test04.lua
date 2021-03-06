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
                                            assert(not thisState:isowner())
                                        elseif cmd == "add" then
                                            return 100 + arg
                                        elseif cmd == "add2" then
                                            return thisState:call("add", 1000 + arg)
                                        elseif cmd == "err1" then
                                            local x = thisState:call("add", {})
                                        elseif cmd == "err2" then
                                            return thisState:call("err1")
                                        end
                                    end
                                end)
    assert(s:isowner())
    local stateId = s:id()
    s:call("setstate", stateId)
    assert(s:call("add",  2) ==  102)
    assert(s:call("add2", 3) == 1103)

    local _, err = pcall(function() s:call("err1") end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.invoking_state))

    local _, err = pcall(function() s:call("err2") end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.invoking_state))
    assert(err:match("bad argument #2 to 'call'"))

    s = nil
    collectgarbage()
    
    local _, err = pcall(function() mtstates.state(stateId) end)
    assert(err:match(mtstates.error.unknown_object)) -- no memory leak
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
                return otherState:call("add2", 10000 + arg)
            end
        end
    end

    local s1 = mtstates.newstate(state_setup)
    local s2 = mtstates.newstate(state_setup)
    
    local id1 = s1:id()
    local id2 = s2:id()

    s1:call("setstate", id2)
    s2:call("setstate", id1)

    assert(s1:call("add",  2) ==   102)
    assert(s2:call("add",  3) ==   103)
    
    assert(s1:call("add2", 4) ==  1104)
    assert(s1:call("add3", 5) == 11105)

    s1 = nil
    s2 = nil
    collectgarbage()

    local _, err = pcall(function() mtstates.state(id1) end)
    assert(err:match(mtstates.error.unknown_object)) -- no memory leak

    local _, err = pcall(function() mtstates.state(id2) end)
    assert(err:match(mtstates.error.unknown_object)) -- no memory leak
end
PRINT("==================================================================================")
do
    local s1 = mtstates.newstate("return function() end")
    local id1 = s1:id()
    local s2 = mtstates.state(id1)
    assert(s1:isowner() and not s2:isowner())
    s1 = nil
    collectgarbage()
    local _, err = pcall(function()
        s2:call()
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.object_closed))
    
    local _, err = pcall(function()
        mtstates.state(id1)
    end)
    assert(err:match(mtstates.error.unknown_object))
end
PRINT("==================================================================================")
print("OK.")
