# mtstates 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Build Status](https://travis-ci.org/osch/lua-mtstates.svg?branch=master)](https://travis-ci.org/osch/lua-mtstates)
[![Build status](https://ci.appveyor.com/api/projects/status/v8t6rsf45dwt60pl/branch/master?svg=true)](https://ci.appveyor.com/project/osch/lua-mtstates/branch/master)

<!-- ---------------------------------------------------------------------------------------- -->

Invoking interpreter states from multiple threads for the [Lua] scripting language.

This package provides a way to create new Lua states from within Lua for using
them in arbitrary threads. The implementation is independent from the
underlying threading library (e.g. [Lanes] or [lua-llthreads2]).

The general principle is to prepare a state that returns a callback function that can be
called from different threads. This can be useful in a thread-pool scenario when the
number of states exceeds the number of available threads.

This package is also available via LuaRocks, see https://luarocks.org/modules/osch/mtstates.

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2

See below for full [reference documentation](#documentation).

<!-- ---------------------------------------------------------------------------------------- -->

## Examples

For the examples [llthreads2](https://luarocks.org/modules/moteus/lua-llthreads2)
is used as low level multi-threading implementation.

First example: a state is constructed whose callback function is called from a
parallel thread and from the main thread. The state is passed by integer id to the
created thread. 

```lua
local llthreads = require("llthreads2.ex")
local mtstates  = require("mtstates")

local state = mtstates.newstate(function(init)
                                    local list = { init }
                                    local cmds = {
                                        add = function(arg) list[#list + 1] = arg end,
                                        get = function() return table.concat(list, " ") end
                                    }
                                    return function(cmd, ...)
                                        return cmds[cmd](...)
                                    end
                                end,
                                "Hello")

local thread = llthreads.new(function(stateId)
                                 local mtstates = require("mtstates")
                                 local state    = mtstates.state(stateId)
                                 return state:call("add", "World")
                             end,
                             state:id())
thread:start()
thread:join()
assert("Hello World" == state:call("get"))
```

Concurrent access to the same Lua state from different threads is prohibited and
leads to an error. Therefore state access has to be synchronised by the caller.
The following example uses  [mtmsg]( https://github.com/osch/lua-mtmsg#mtmsg) to
synchronise acces to the state:

```lua
local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local mtstates  = require("mtstates")

local state = mtstates.newstate(function()
                                    local list = {}
                                    local cmds = {
                                        add = function(arg) list[#list + 1] = arg end,
                                        get = function() return table.concat(list, " ") end
                                    }
                                    return function(cmd, ...)
                                        return cmds[cmd](...)
                                    end
                                end)

local threadIn  = mtmsg.newbuffer()
local threadOut = mtmsg.newbuffer()

local thread = llthreads.new(function(threadInId, threadOutId)
                                 local mtstates  = require("mtstates")
                                 local mtmsg     = require("mtmsg")
                                 local threadIn  = mtmsg.buffer(threadInId)
                                 local threadOut = mtmsg.buffer(threadOutId)
                                 while true do
                                     local cmd, stateId, arg = buffer:nextmsg()
                                     if cmd == "finish" then
                                         threadOut:addmsg("finished")
                                         break
                                     else
                                        local state = mtstates.state(stateId)
                                        state:call(cmd, arg)
                                        threadOut:addmsg("done")
                                    end
                                 end
                             end,
                             threadIn:id(), threadOut:id())
thread:start()

threadIn:addmsg("add", state:id(), "Hello")
threadIn:addmsg("add", state:id(), "World")

assert(threadOut:nextmsg() == "done")
assert(threadOut:nextmsg() == "done")

assert("Hello World" == state:call("get"))

threadIn:addmsg(state:id(), "finished")
assert(threadOut:nextmsg() == "finished")
assert(thread:join())
```



<!-- ---------------------------------------------------------------------------------------- -->

## Documentation

   * [Module Functions](#module-functions)
       * mtstates.newstate()
       * mtstates.state()
       * mtstates.type()
   * [State Methods](#state-methods)
       * state:id()
       * state:name()
       * state:call()
       * state:close()
   * [Error Methods](#error-methods)
       * error:name()
       * error:details()
       * error:traceback()
       * error:message()
       * err1 == err2
   * [Errors](#errors)
       * mtstates.error.invoking_state
       * mtstates.error.object_closed
       * mtstates.error.object_exists
       * mtstates.error.out_of_memory
       * mtstates.error.state_result
       * mtstates.error.unknown_object

<!-- ---------------------------------------------------------------------------------------- -->

### Module Functions

* **`mtstates.newstate([name,][libs,]setup[,...)`**

  TODO...

  Possible errors: *mtstates.error.object_exists*,
                   *mtstates.error.invoking_state*,
                   *mtstates.error.state_result*

* **`mtstates.state(id|name)`**

  Creates a lua object for referencing an existing state. The state must
  be referenced by its *id* or *name*.

    * *id* - integer, the unique state id that can be obtained by
           *state:id()*.

    * *name* - string, the optional name that was given when the
               state was created with *mtstates.newstate()*

  Possible errors: *mtstates.error.unknown_object*


* **`mtstates.type(arg)`**

  Returns the type of *arg* as string. Same as *type(arg)* for builtin types.
  For *userdata* objects it tries to determine the type from the *__name* field in 
  the metatable and checks if the metatable can be found in the lua registry for this key
  as created by [luaL_newmetatable](https://www.lua.org/manual/5.3/manual.html#luaL_newmetatable).

  Returns *"userdata"* for userdata where the *__name* field in the metatable is missing
  or does not have a corresponding entry in the lua registry.
  
  Returns *"mtstates.state"*, or *"mtstates.error"* if the arg is one
  of the userdata types provided by the mtstates package.


<!-- ---------------------------------------------------------------------------------------- -->

### State Methods

* **`state:id()`**
  
  Returns the state's id as integer. This id is unique among all states 
  for the whole process.

* **`state:name()`**

  Returns the state's name that was given to *mtstates.newstate()*.
  

* **`state:call(...)`**

  TODO...

  Possible errors: *mtstates.error.object_closed*,
                   *mtstates.error.invoking_state*,
                   *mtstates.error.state_result*


* **`state:close()`**

  Closes the underlying state and frees the memory. Every operation from any
  referencing object raises a *mtstates.error.object_closed*. A closed state
  cannot be reactivated.


<!-- ---------------------------------------------------------------------------------------- -->

### Error Methods

* **`error:name()`**

  Name of the error as string, example:
  
  ```lua
  local mtstates = require("mtstates")
  assert(mtstates.error.object_closed:name() == "mtstates.error.object_closed")
  ```
  
* **`error:details()`**

  Additional details as string regarding this error.

* **`error:traceback()`**

  Stacktrace as string where the error occured.

* **`error:message()`**

  Full message as string containing name, details and traceback.
  Same as *tostring(error)*.
  
* **`err1 == err2`**
  
  Two errors are considered equal if they have the same name. This can be used
  for error evaluation, example:

  ```lua
  local mtstates = require("mtstates")
  local _, err = pcall(function() 
      mtstates.newstate(function() end) 
  end)
  assert(err == mtstates.error.state_result)
  ```

<!-- ---------------------------------------------------------------------------------------- -->

### Errors

* **`mtstates.error.invoking_state`**

  TODO....

* **`mtstates.error.object_closed`**

  An operation is performed on a closed state, i.e. the method
  *state:close()* has been called.

* **`mtstates.error.object_exists`**

  A new state is to be created via *mtstates.newstate()*
  with a name that refers to an already existing state.


* **`mtstates.error.out_of_memory`**

  State memory cannot be allocated.


* **`mtstates.error.state_result`**

  TODO....

* **`mtstates.error.unknown_object`**

  A reference to an existing state (via *mtstates.state()*)
  cannot be created because the object cannot be found by
  the given id or name. 
  
  All mtstates objects are subject to garbage collection and therefore a reference to a 
  created object is needed to keep it alive, i.e. if you want to pass an object
  to another thread via name or id, a reference to this object should be kept in the
  thread that created the object, until the receiving thread signaled that a reference
  to the object has been constructed in the receiving thread.
  

End of document.

<!-- ---------------------------------------------------------------------------------------- -->
