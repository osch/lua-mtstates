local mtmsg    = require("mtmsg")
local mtstates = require("mtstates")

local function printf(...)
    print(string.format(...))
end

local N        = 5000
local withName = false

local function printTime(title, startTime)
    local t = mtmsg.time() - startTime
    printf("%-15s %10.3f kop/sec, %6.3f msecs/op", title, N / t / 1000, t / N * 1000)
end

for run = 1, 2 do
    
    collectgarbage() 

    printf("N = %d, withName = %s", N, withName)
    
    local idlist = {}
    local stlist = {}
    
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    do
        local function stateSetup(i)
            return function(x)
                return x + i
            end
        end
        for i = 1, N do
            local name = withName and "foo"..i or nil
            local s = mtstates.newstate(name, stateSetup, i)
            stlist[i]  = s
            idlist[i] = s:id()
        end
    end
    printTime("Creation:", startTime)
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    for i = 1, N do
        assert(stlist[i]:call(1000) == 1000 + i)
    end
    printTime("Direct call:", startTime)
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    for i = 1, N do
        local id = idlist[i]
        assert(mtstates.state(id):id() == id)
    end
    printTime("Search by id:", startTime)
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    for i = 1, N do
        assert(mtstates.state(idlist[i]):call(1000) == 1000 + i)
    end
    printTime("Call by id:", startTime)
    --------------------------------------------------------------
    
end    
    
print("OK.")
