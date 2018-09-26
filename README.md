# mtstates 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Build Status](https://travis-ci.org/osch/lua-mtstates.svg?branch=master)](https://travis-ci.org/osch/lua-mtstates)
[![Build status](https://ci.appveyor.com/api/projects/status/v8t6rsf45dwt60pl/branch/master?svg=true)](https://ci.appveyor.com/project/osch/lua-mtstates/branch/master)
[![Install](https://img.shields.io/badge/Install-LuaRocks-brightgreen.svg)](https://luarocks.org/modules/osch/mtstates)

<!-- ---------------------------------------------------------------------------------------- -->

Invoking interpreter states from multiple threads for the [Lua] scripting language.

This package provides a way to create new Lua states from within Lua for using
them in arbitrary threads. The implementation is independent from the
underlying threading library (e.g. [Lanes] or [lua-llthreads2]).

The general principle is to prepare a state by running a setup function within
this state that returns a callback function which afterwards can be called from
different threads. This can be useful in a thread-pool scenario when the number
of states exceeds the number of available threads.

This package is also available via LuaRocks, see https://luarocks.org/modules/osch/mtstates.

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2

See below for full [reference documentation](#documentation).

<!-- ---------------------------------------------------------------------------------------- -->

#### Requirements

   * Tested operating systems: Linux, Windows, MacOS
   * Other Unix variants: could work, but untested, required are:
      * gcc atomic builtins or C11 `stdatomic.h`
      * `pthread.h` or C11 `threads.h`
   * Tested Lua versions: 5.1, 5.2, 5.3, luajit 2.0 & 2.1

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

It's not possible for concurrently runnings threads to call a state's callback function
simultaneously, therefore each call waits until the previous call is finished. It is 
possible to give a timeout for calling a state callback (see [*state:tcall()*](#tcall)).

The following example uses  [mtmsg]( https://github.com/osch/lua-mtmsg#mtmsg) to
synchronize access to a state:

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
                                     local cmd, stateId, arg = threadIn:nextmsg()
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

threadIn:addmsg("finish")
assert(threadOut:nextmsg() == "finished")
assert(thread:join())
```



<!-- ---------------------------------------------------------------------------------------- -->

## Documentation

   * [Module Functions](#module-functions)
       * mtstates.newstate()
       * mtstates.state()
       * mtstates.singleton()
       * mtstates.id()
       * mtstates.type()
   * [State Methods](#state-methods)
       * state:id()
       * state:name()
       * state:call()
       * state:tcall()
       * state:interrupt()
       * state:isowner()
       * state:close()
   * [Errors](#errors)
       * mtstates.error.ambiguous_name
       * mtstates.error.concurrent_access
       * mtstates.error.interrupted
       * mtstates.error.invoking_state
       * mtstates.error.object_closed
       * mtstates.error.out_of_memory
       * mtstates.error.state_result
       * mtstates.error.unknown_object

<!-- ---------------------------------------------------------------------------------------- -->

### Module Functions

* <a id="newstate">**`mtstates.newstate([name,][libs,]setup[,...)`**</a>

  Creates a new state. The given setup function is executed in the new state. The
  setup function must return a state callback function which can be called
  using the method *state:call()*. Additional values that are returned from 
  the setup function are given back as additional results by *mtstates.newstate()*.
  
    * *name*  - optional string, the name of the new state, can be *nil* or 
                omitted to create a state without name.
    * *libs*  - optional boolean, if *true* all standard libraries
                are opened in the new state, if *false* only the basic lua functions 
                and the module "package" are loaded, other standard libraries 
                are preloaded and can be loaded by *require*, defaults to *true*,
    * *setup* - state setup function, can be a function without upvalues or
                a string containing lua code. The setup function must return
                a function that is used as state callback function for the
                new created state.
    * *...*   - additional parameters, are transfered to the new state and
                are given as arguments to the setup function. Arguments can be
                simple data types (string, number, boolean, nil, light user data).

  This function returns a state referencing lua object with *state:isowner() == true*.

  Possible errors: *mtstates.error.invoking_state*,
                   *mtstates.error.state_result*

* **`mtstates.state(id|name)`**

  Creates a lua object for referencing an existing state. The state must
  be referenced by its *id* or *name*. Referencing the state by *id* is
  much faster than referencing by *name* if the number of states 
  increases.

    * *id* - integer, the unique state id that can be obtained by
           *state:id()*.

    * *name* - string, the optional name that was given when the
               state was created with *mtstates.newstate()*. To
               find a state by name the name must be unique for
               the whole process.

  This function returns a state referencing lua object with *state:isowner() == false*.

  Possible errors: *mtstates.error.ambiguous_name*,
                   *mtstates.error.unknown_object*


* **`mtstates.singleton(name,[libs,]setup[,...)`**
    
  Creates a lua object for referencing an existing state by name. If the
  state referenced by the name does not exist, a new state is created using
  the supplied parameters. This invocation can be used to atomically construct 
  a globally named state securely from different threads.
    
  If *mtstates.singleton()* is called with the same *name* parameter from
  concurrently running threads the second invocation waits until the first
  invocation is finished or fails.
  
  
  * *name* - mandatory string, name for finding an existing state. If the state is
             not found the following parameters are used to create a new state
             with the given name.
  
  * *libs*, *setup*, *...*  - same parameters as in [*mtstates.newstate()*](#newstate).
  
  This function returns a state referencing lua object with *state:isowner() == true*.

  This function should only by used for very special use cases: Because references 
  to the same *mtstates*'s state from different lua states are reference counted
  special care has to be taken that no reference cycle is constructed using 
  *mtstates.singleton()*.

  Possible errors: *mtstates.error.ambiguous_name*,
                   *mtstates.error.invoking_state*,
                   *mtstates.error.state_result*    
  
* **`mtstates.id()`**

  Gives the state id of the currently running state invoking this function.
  Returns `nil` if the current state was not constructed via 
  *mtstates.newstate()* or *mtstates.singleton()*.
  
  
* **`mtstates.type(arg)`**

  Returns the type of *arg* as string. Same as *type(arg)* for builtin types.
  For *userdata* objects it tries to determine the type from the *__name* field in 
  the metatable and checks if the metatable can be found in the lua registry for this key
  as created by [luaL_newmetatable](https://www.lua.org/manual/5.3/manual.html#luaL_newmetatable).

  Returns *"userdata"* for userdata where the *__name* field in the metatable is missing
  or does not have a corresponding entry in the lua registry.
  
  Returns *"mtstates.state"* if the arg is the state userdata type provided by this package.


<!-- ---------------------------------------------------------------------------------------- -->

### State Methods

* **`state:id()`**
  
  Returns the state's id as integer. This id is unique among all states 
  for the whole process.

* **`state:name()`**

  Returns the state's name that was given to *mtstates.newstate()*.
  

* **`state:call(...)`**

  Invokes the state callback function that was returned by the state setup function
  when the state was created with *mtstates.newstate()*.
  
  * *...* - All argument parameters are transfered to the state and given to the state
            callback function. Arguments can be simple data types (string, number,
            boolean, nil, light user data).

  If the state callback function is processed in a concurrently running thread the 
  *state:call()* method waits for the other call to complete before the state callback
  function is invoked. See next method *state:tcall()* for calling a state with 
  timeout parameter.

  Returns the results of the state callback function. Results can be simple data types 
  (string, number, boolean, nil, light user data).

  Possible errors: *mtstates.error.interrupted*,
                   *mtstates.error.invoking_state*,
                   *mtstates.error.object_closed*,
                   *mtstates.error.state_result*


* <a id="tcall">**`state:tcall(timeout, ...)`**</a>

  Invokes the state callback function that was returned by the state setup function
  when the state was created with *mtstates.newstate()*.
  
  * *timeout* float, maximal time in seconds for waiting for a concurrently running
              call of the state callback function to complete.
              
  * *...* - additional argument parameters are transfered to the state and given to the state
            callback function. Arguments can be simple data types (string, number,
            boolean, nil, light user data).

  If the state could be accessed within the timeout *state:tcall()* returns the boolean
  value *true* and all results from the state callback function. Results can be simple 
  data types (string, number, boolean, nil, light user data).
  
  Returns *false* if the state could not be accessed during the timeout.

  Possible errors: *mtstates.error.interrupted*,
                   *mtstates.error.invoking_state*,
                   *mtstates.error.object_closed*,
                   *mtstates.error.state_result*


* **`state:interrupt([flag])`**

  Interrupts the state by installing a debug hook that triggers an error
  *mtstates.error.interrupted*. Therefore the interrupt only occurs when lua
  code is invoked. If the state is within a c function the interrupt occurs
  when the c function returns.

  * *flag* - optional boolean, if not specified or *nil* the state
             is only interrupted once, if *true* the state is interrupted
             at every operation again, if *false* the state is no
             longer interrupted.

             
* **`state:isowner()`**

  Returns `true` if the state referencing lua object owns the referenced
  state. If the last owning lua object is garbage collected the underlying 
  state is closed. 
  
  State referencing objects constructed via *mtstates.newstate()* or
  *mtstates.singleton()* are owning the state.
  
  State referencing objects constructed via *mtstates.state()* are 
  not owning the state.
  

* **`state:close()`**

  Closes the underlying state and frees the memory. Every operation from any
  referencing object raises a *mtstates.error.object_closed*. 
  
  A closed state cannot be found via its name or id using the function
  *mtstates.state()*.
  
  A closed state cannot be reactivated.

  Possible errors: *mtstates.error.concurrent_access*

<!-- ---------------------------------------------------------------------------------------- -->

### Errors

* All errors raised by this module are string values. Special error strings are
  available in the table `mtstates.error`, example:

  ```lua
  local mtstates = require("mtstates")
  assert(mtstates.error.state_result == "mtstates.error.state_result")
  ```
  
  These can be used for error evaluation purposes, example:
  
  ```lua
  local mtstates = require("mtstates")
  local _, err = pcall(function() 
      mtstates.newstate(function() end) 
  end)
  assert(err:match(mtstates.error.state_result))
  ```

* **`mtstates.error.ambiguous_name`**

  More than one state was found for the given name to *mtstates.state()*.
  To find a state by name, the state name must be unique among all states
  in the whole process 

* **`mtstates.error.concurrent_access`**

  Raised if *state:close()* is called while the state is processing a call 
  on a parallel running thread.

* **`mtstates.error.interrupted`**

  The state was interrupted by invoking the method *state:interrupt()*.

* **`mtstates.error.invoking_state`**

  An error ocurred when running code within another state, e.g. when the setup function
  given to *mtstates.newstate()* is executed during state creation or when the
  state callback function is called in *state:call()*.
  
  This error contains the traceback from within the state and the outer traceback from 
  context that called *mtstates.newstate()* or *state:call()*.
  

* **`mtstates.error.object_closed`**

  An operation is performed on a closed state, i.e. the method
  *state:close()* has been called.

* **`mtstates.error.out_of_memory`**

  State memory cannot be allocated.


* **`mtstates.error.state_result`**

  A result value from a function invoked in another state is invalid, e.g. the
  setup function given to *mtstates.newstate()* returns no state callback or
  a state callback or state setup function returns a value that cannot be 
  transferred back to the invoking state.

* **`mtstates.error.unknown_object`**

  A reference to an existing state (via *mtstates.state()*)
  cannot be created because the object cannot be found by
  the given id or name. 
  
  All mtstates objects are subject to garbage collection and therefore a owning
  reference to a created object is needed to keep it alive, example:

  ```lua
  local mtstates  = require("mtstates")
  local s1 = mtstates.newstate("return function() return 3 end")
  local id = s1:id()
  local s2 = mtstates.state(id)
  local s3 = mtstates.state(id)
  assert(s1:isowner() == true)
  assert(s2:isowner() == false)
  assert(s3:isowner() == false)
  s2 = nil
  collectgarbage()
  assert(s3:call() == 3)
  assert(mtstates.state(id):id() == id)
  s1 = nil
  collectgarbage()
  local _, err = pcall(function()
      s3:call()
  end)
  assert(err:match(mtstates.error.object_closed))
  local _, err = pcall(function()
      mtstates.state(id)
  end)
  assert(err:match(mtstates.error.unknown_object))
  ```
  

End of document.

<!-- ---------------------------------------------------------------------------------------- -->
