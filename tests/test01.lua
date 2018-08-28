local load = loadstring and loadstring or load
local dump = string.dump
local mts = require("mtstates")
local function f1()
    local a = 1
    local b = 2
    return function(x) return x + 1 end
end
local s1 = mts.newstate("s1", f1)
print(s1, s1:id())
assert(s1:name() == "s1")
local _, err = pcall(function() mts.newstate("s1", "") end)
print("-------------------------------------")
print("-- Expected error:")
print(err)
print("-------------------------------------")
assert(err == mts.error.object_exists)
s1 = nil
collectgarbage()
local s1 = mts.newstate("s1", function()
    x = s1
    y = 2
    return function(x) return x + 2 end
end)
local s2 = mts.newstate("s2", [[return function(x) return x + 3 end]])
local s3, x, y = mts.newstate([[return function(x) return x + 2 end, 4, 5]])
assert(x == 4 and y == 5)
print(s1, s1:id())
print(s2, s2:id())
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
print("Done.")
