  local mtstates = require("mtstates")
  local _, err = pcall(function() 
      mtstates.newstate(function() end) 
  end)
  assert(err:match(mtstates.error.state_result))
