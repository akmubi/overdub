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

---Keybinds are strings such as "F8", "Ctrl+F8" or "F8 | Select+RB" (keyboard part | gamepad part).
---@param bind string
---@return boolean
function input.KeybindDown(bind) end

---@param bind string
---@return boolean
function input.KeybindPressed(bind) end

---@param bind string
---@return boolean
function input.KeybindReleased(bind) end

---True when the event is the press of the bind.
---@param bind string
---@param event OverdubInputEvent
---@return boolean
function input.KeybindActivated(bind, event) end

---@param bind? string
---@return string? keyboard
---@return string? gamepad
function input.KeybindParts(bind) end

---@param keyboard? string
---@param gamepad? string
---@return string? bind
function input.KeybindMerge(keyboard, gamepad) end

---@return number
function input.WheelDelta() end

---@return integer x
---@return integer y
function input.MousePosition() end

return input
