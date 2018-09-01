local load = loadstring and loadstring or load
local dump = string.dump
local mtstates = require("mtstates")
local mts      = mtstates
print("test01 started")
print("mtstates._VERSION", mts._VERSION)
print("mtstates._INFO", mts._INFO)
local function PRINT(s)
    print(s.." ("..debug.getinfo(2).currentline..")")
end
local function f1()
    local a = 1
    local b = 2
    return function(x) return x + 1 end
end
local s1 = mts.newstate("s1", f1)
print(s1, s1:id())
assert(s1:name() == "s1")
assert(s1:call(10) == 11)
local _, err = pcall(function() mts.newstate("s1", "") end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mts.error.object_exists)
s1 = nil
collectgarbage()
local s1 = mts.newstate("s1", function()
    local x = 1
    local y = 2
    return function(x) return x + 2 end
end)
local s2 = mts.newstate("s2", [[return function() return x + 1 end]])
local s3, x, y = mts.newstate([[return function(x) return x + 2 end, 4, 5]])
assert(x == 4 and y == 5)
print(s1, s1:id())
print(s2, s2:id())
local _, err = pcall(function() s2:call() end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mts.error.invoking_state)
print(s3, s3:id())
assert(s1:name() == "s1")
assert(s2:name() == "s2")
assert(s3:name() == nil)
local s1a = mts.state("s1")
print(s1a)
assert(s1a:id()   == s1:id())
assert(s1a:name() == s1:name())
local s2a = mts.state(s2:id())
print(s2a)
assert(s2a:id()   == s2:id())
assert(s2a:name() == s2:name())
local _, err = pcall(function() mts.state("foo") end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mts.error.unknown_object)

PRINT("==================================================================================")
do
    local s = mts.newstate(function()
                            local function m2(x)
                                error("error xxx")
                            end
                            local function m(x)
                                return m2(x) + 5
                            end
                            return function(x)
                                return m(x) + 3
                            end
                        end)
    local function a2()
        return s:call(42)
    end
    local function a()
        return a2() + 1
    end
    local _, err = pcall(a)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    print("-- Expected error name:")
    print(err:name())
    print("-------------------------------------")
    print("-- Expected error details:")
    print(err:details())
    print("-------------------------------------")
    print("-- Expected Traceback:")
    print(err:traceback())
    print("-------------------------------------")
    assert(err == mts.error.invoking_state)
end
PRINT("==================================================================================")
do
    local function invokeNew()
        mts.newstate(function(x)
                        local function m2(x)
                            error("error yyy")
                        end
                        local function m(x)
                            return m2(x) + 5
                        end
                        return m(x) + 3
                    end)
    end
    local _, err = pcall(function() invokeNew() end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    print("-- Expected error name:")
    print(err:name())
    print("-------------------------------------")
    print("-- Expected error details:")
    print(err:details())
    print("-------------------------------------")
    print("-- Expected Traceback:")
    print(err:traceback())
    print("-------------------------------------")
    assert(err == mts.error.invoking_state)
end
PRINT("==================================================================================")
do
    local x1 = 0
    local _, err = pcall(function ()
        mts.newstate(function() return x1 end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
end
do
    local x2 = 0
    local _, err = pcall(function ()
        mts.newstate(function() print(x2) return x2 end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
end
PRINT("==================================================================================")
do
    local _, err = pcall(function()
        mts.newstate(function()
            local e = {}
            error(e)
        end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mts.error.invoking_state)
end
PRINT("==================================================================================")
do
    local _, err = pcall(function()
        mts.newstate(function()
            local e = {}
            local s = tostring(e)
            setmetatable(e, { 
                __tostring = 
                    function(self) return "XXXX {"..s.."}" end})
            error(e)
        end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mts.error.invoking_state)
end
PRINT("==================================================================================")
do
    GLOBAL_VAR = 1
    local _, err = pcall(function()
        mts.newstate(function()
            local x = GLOBAL_VAR + 1
            return function() return x end
        end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err == mts.error.invoking_state)
end
PRINT("==================================================================================")
do
    local function testcase()
        local _, err = pcall(function()
            mts.newstate(function()
                return function() end, "", {}
            end)
        end)
        print("-------------------------------------")
        print("-- Expected error:")
        print(err)
        print("-------------------------------------")
        assert(mtstates.error.state_result == err)
    end
    testcase()
end
PRINT("==================================================================================")
do
    local _, err = pcall(function()
        mts.newstate(function()
            return require"mtstates".newstate(function() return {} end)
        end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(mtstates.error.invoking_state == err)
end
PRINT("==================================================================================")
do
    local _, err = pcall(function()
        mts.newstate(function()
            return 1
        end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(mtstates.error.state_result == err)
end
PRINT("==================================================================================")
do
    local s = mts.newstate(function()
        return function() end, "a", "b"
    end)
    local _, err = pcall(function()
        s:call(1, 2, function() end)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
end
PRINT("==================================================================================")
do
    local s, a, b = mts.newstate(function()
        local mts = require"mtstates"
        local s = mts.newstate(function() return (function() end) end)
        return function() return s:id(), s; end, "a", "b"
    end)
    assert(a == "a" and b == "b")
    local _, err = pcall(function()
        s:call(1)
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(mtstates.error.state_result == err)
end
PRINT("==================================================================================")
do
    local s = mts.newstate(function()
        return function() return 42; end
    end)
    assert(s:call() == 42)
    s:close()
    local _, err = pcall(function()
        s:call()
    end)
    print("-------------------------------------")
    print("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(mtstates.error.object_closed == err)
end
PRINT("==================================================================================")
print("Done.")
