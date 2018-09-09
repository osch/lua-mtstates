local load = loadstring and loadstring or load
local dump = string.dump
local mtstates = require("mtstates")
local mts      = mtstates
print("test01 started")
print("mtstates._VERSION", mts._VERSION)
print("mtstates._INFO", mts._INFO)
local function line()
    return debug.getinfo(2).currentline
end
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
    local _, err = pcall(function()
        mts.newstate("foo")
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match("bad argument #1"))
    
    local _, err = pcall(function()
        mts.newstate("foo", [[xxx]])
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match("bad argument #2"))

    local _, err = pcall(function()
        mts.newstate("foo", true, [[xxx]])
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match("bad argument #3"))

    local _, err = pcall(function()
        mts.newstate("foo", true, [[xxx]], "foo2")
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match("bad argument #3"))

    local _, err = pcall(function()
        mts.newstate("foo", true, 2, "foo2")
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match("bad argument #3"))

    local _, err = pcall(function()
        mts.newstate("foo", true, mts.newstate, "foo2")
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match("bad argument #3"))
end
PRINT("==================================================================================")
do
    local function f1()
        local a = 1
        local b = 2
        return function(x) return x + 1 end
    end
    local s1 = mts.newstate("s1", f1)
    print(s1, s1:id())
    assert(s1:name() == "s1")
    assert(s1:call(10) == 11)
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.invoking_state))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.unknown_object))
    local s1a = mts.newstate("s1", f1)
    local _, err = pcall(function() mts.state("s1") end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.ambiguous_name))
end
PRINT("==================================================================================")
do
    local s = mts.newstate("return function(x) return x + 3 end")
    assert(s:call(10) == 13)
    assert(s:name() == nil)
    
    local n = "foo"..line()
    local s = mts.newstate(n, "return function(x) return x + 4 end")
    assert(s:call(10) == 14)
    assert(s:name() == n)
    
    local s = mts.newstate(nil, "return function(x) return x + 5 end")
    assert(s:call(10) == 15)
    assert(s:name() == nil)
end
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.invoking_state))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.invoking_state))
end
PRINT("==================================================================================")
do
    local x1 = 0
    local _, err = pcall(function ()
        mts.newstate(function() return x1 end)
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
end
do
    local x2 = 0
    local _, err = pcall(function ()
        mts.newstate(function() print(x2) return x2 end)
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.invoking_state))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.invoking_state))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mts.error.invoking_state))
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
        PRINT("-- Expected error:")
        print(err)
        print("-------------------------------------")
        assert(err:match(mtstates.error.state_result))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.invoking_state))
    assert(err:match(mtstates.error.state_result))
end
PRINT("==================================================================================")
do
    local _, err = pcall(function()
        mts.newstate(function()
            return 1
        end)
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(    err:match(mtstates.error.state_result))
    assert(not err:match(mtstates.error.invoking_state))
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
    PRINT("-- Expected error:")
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.state_result))
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
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.object_closed))
end
PRINT("==================================================================================")
do
    local s = mts.newstate(function()
        assert(type(table) == "table")
        assert(type(debug) == "table")
        return function() end
    end)
    assert(s:name() == nil)

    local s = mts.newstate(true, function()
        assert(type(table) == "table")
        assert(type(debug) == "table")
        return function() end
    end)
    assert(s:name() == nil)

    local n = "foo"..line()
    local s = mts.newstate(n, true, function()
        assert(type(table)  == "table")
        assert(type(string) == "table")
        assert(type(debug)  == "table")
        return function() end
    end)
    assert(s:name() == n)
    
    local n = "foo"..line()
    local s = mts.newstate(n, false, function()
        assert(type(package) == "table")
        assert(type(string)  == "nil")
        assert(type(debug)   == "nil")
        return function() end
    end)
    assert(s:name() == n)

    local s = mts.newstate(nil, false, function()
        assert(type(package) == "table")
        assert(type(string)  == "nil")
        assert(type(debug)   == "nil")
        return function() end
    end)
    assert(s:name() == nil)

    local s = mts.newstate(false, function()
        assert(type(package) == "table")
        assert(type(table)   == "nil")
        assert(type(string)  == "nil")
        table = require("table")
        assert("a b" ==  table.concat({"a", "b"}, " "))
        string = require("string")
        assert(string.len("123") == 3)
        assert(debug == nil)
        debug = require("debug")
        assert(type(debug.traceback) == "function")
        if _VERSION == "Lua 5.2" then
            assert(bit32 == nil)
            local bit32 = require("bit32")
            assert(type(bit32.bnot) == "function")
        end
        if _VERSION ~= "Lua 5.1" then
            assert(coroutine == nil)
            coroutine = require("coroutine")
        end
        assert(type(coroutine) == "table")
        assert(type(coroutine.yield) == "function")
        assert(type(require("mtstates").newstate) == "function")
        return function() end
    end)
    assert(s:name() == nil)
end
PRINT("==================================================================================")
do
    local s = mts.newstate(function() 
        return function(arg)
            return arg + 5 
        end
    end)
    
    local _, err = pcall(function()
        s:interrupt("")
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")

    s:interrupt(nil)
    local _, err = pcall(function()
        s:call(100)
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(err:match(mtstates.error.invoking_state))
    assert(err:match(mtstates.error.interrupted))
    
    assert(s:call(200) == 205)
    
    s:interrupt()
    local _, err = pcall(function()
        s:call(100)
    end)
    assert(err:match(mtstates.error.invoking_state))
    assert(err:match(mtstates.error.interrupted))
    
    assert(s:call(300) == 305)

    s:interrupt(true)
    for i = 1, 4 do
        local _, err = pcall(function()
            s:call(100)
        end)
        print("-- Expected error:", err)
        assert(err:match(mtstates.error.invoking_state))
        assert(err:match(mtstates.error.interrupted))
    end
    
    s:interrupt(false)
    assert(s:call(400) == 405)
    
end
PRINT("==================================================================================")
do
    local ok, err = pcall(function()
        mtstates.newstate(mtstates.newstate)
    end)
    print("-------------------------------------")
    PRINT("-- Expected error:")
    print(err)
    print("-------------------------------------")
    assert(not ok and err:match("lua function expected"))
end
PRINT("==================================================================================")
do
    local s = mtstates.newstate(function() 
        local x = 0
        return function(y)
            x = x + 1
            return x + (y and y or 0), y
        end
    end)
    assert(s:call() == 1)

    local ok , rslt1, rslt2 = s:tcall(0)
    assert(ok and rslt1 == 2 and rslt2 == nil)
    print(ok, rslt1, rslt2)

    local ok , rslt1, rslt2 = s:tcall(1, 100)
    print(ok, rslt1, rslt2)
    assert(ok and rslt1 == 103 and rslt2 == 100)
end
PRINT("==================================================================================")
print("Done.")
