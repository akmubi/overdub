---@meta

---@class OverdubInputEvent
---@field Kind string
---@field Key? string
---@field Pressed boolean
---@field Released boolean
---@field Repeat boolean
---@field Shift boolean
---@field Ctrl boolean
---@field Alt boolean
---@field Keyboard boolean
---@field Mouse boolean
---@field Gamepad boolean
---@field Character? integer Unicode codepoint for Character events.
---@field Activated? boolean Application activation state.
---@field ScreenX? integer
---@field ScreenY? integer
---@field ClientX? integer
---@field ClientY? integer
---@field DeltaX? number
---@field DeltaY? number
---@field WheelDelta? number
---@field AnalogValue? number

---@class OverdubInputModule
local input = {}

---@param key string
---@return boolean
function input.Down(key) end

---@param key string
---@return boolean
function input.Pressed(key) end

---@param key string
---@return boolean
function input.Released(key) end

---@param key string
---@return number
function input.Analog(key) end

---@return number
function input.WheelDelta() end

---@return integer x
---@return integer y
function input.MousePosition() end

return input
