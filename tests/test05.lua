local mtstates  = require("mtstates")
local carray    = require("carray")

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
    local a = carray.new("int", 3)
    a:set(1, 101, 102, 103)
    local s,b  = mtstates.newstate(function(a)
                                       assert(a:len() == 3)
                                       local x,y,z = a:get(1,3)
                                       assert(x == 101)
                                       assert(y == 102)
                                       assert(z == 103)
                                       a:set(1, 201, 202, 203)
                                       return function()
                                       end, a
                                   end, a)
    local x,y,z = a:get(1,3)
    assert(x == 101)
    assert(y == 102)
    assert(z == 103)

    local x,y,z = b:get(1,3)
    assert(x == 201)
    assert(y == 202)
    assert(z == 203)
end
PRINT("==================================================================================")
do
    local s  = mtstates.newstate(function()
                                     local i = 100
                                     return function(a)
                                         assert(a:len() == 3)
                                         local x,y,z = a:get(1,3)
                                         assert(x == i + 1)
                                         assert(y == i + 2)
                                         assert(z == i + 3)
                                         i = i + 100
                                         a:set(1, i + 1, i + 2, i + 3)
                                         return a
                                     end
                                 end)

    local a = carray.new("int", 3)
    a:set(1, 101, 102, 103)

    local b  = s:call(a)
    local b2 = s:call(b)

    local x,y,z = a:get(1,3)
    assert(x == 101)
    assert(y == 102)
    assert(z == 103)

    local x,y,z = b:get(1,3)
    assert(x == 201)
    assert(y == 202)
    assert(z == 203)

    local x,y,z = b2:get(1,3)
    assert(x == 301)
    assert(y == 302)
    assert(z == 303)
end
PRINT("==================================================================================")
print("OK.")
