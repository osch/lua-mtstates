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
    local s = mtstates.newstate(function(inId)
                                    local mtstates  = require("mtstates")
                                    local thisState = nil
                                    return function(cmd, arg)
                                        if cmd == "state" then
                                            thisState = mtstates.state(arg)
                                        elseif cmd == "add" then
                                            return 100 + arg
                                        elseif cmd == "add2" then
                                            return thisState:call("add", 1000 + arg)
                                        end
                                    end
                                end)
    local stateId = s:id()
    s:call("state", stateId)
    assert(s:call("add", 2) == 102)
    --assert(s:call("add2", 3) == 1103) -- deadlock :-(, DON'T DO THIS
    s = nil
    collectgarbage()
    assert(mtstates.state(stateId):id() == stateId)  -- memory leak :-(, DON'T DO THIS
end
PRINT("==================================================================================")
print("OK.")
