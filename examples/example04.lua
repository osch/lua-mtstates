  local mtstates = require("mtstates")
  local _, err = pcall(function() 
      mtstates.newstate(function() end) 
  end)
  assert(err == mtstates.error.state_result)