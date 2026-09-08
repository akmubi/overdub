---@meta

---@alias OverdubUIAlignment "Left"|"Center"|"Right"

---@class OverdubUIColor
---@field R integer
---@field G integer
---@field B integer
---@field A? integer

---@class OverdubUIWindow
local Window = {}

---@return boolean removed_now
function Window:Remove() end

---@return boolean
function Window:IsActive() end

---@param open boolean
---@return boolean changed
function Window:SetOpen(open) end

---@return boolean
function Window:IsOpen() end

---@class OverdubUIContext
local Context = {}

---@param text string
---@param alignment? OverdubUIAlignment
function Context:Text(text, alignment) end

---@param text string
---@param color OverdubUIColor
---@param alignment? OverdubUIAlignment
function Context:TextColored(text, color, alignment) end

---@param text string
---@param height? number
function Context:WrappedText(text, height) end

---@param text string
---@param color OverdubUIColor
---@param height? number
function Context:WrappedTextColored(text, color, height) end

---@param label string
---@return boolean pressed
function Context:Button(label) end

---@param label string
---@param value boolean
---@return boolean value
function Context:Checkbox(label, value) end

---@param label string
---@param value integer
---@param minimum integer
---@param maximum integer
---@param step? integer
---@return integer value
function Context:SliderInt(label, value, minimum, maximum, step) end

---@param label string
---@param value number
---@param minimum number
---@param maximum number
---@param step? number
---@return number value
function Context:SliderFloat(label, value, minimum, maximum, step) end

---@param label string
---@param value integer
---@param minimum integer
---@param maximum integer
---@param step? integer
---@return integer value
---@return boolean changed
function Context:InputInt(label, value, minimum, maximum, step) end

---@param label string
---@param value number
---@param minimum number
---@param maximum number
---@param step? number
---@return number value
---@return boolean changed
function Context:InputFloat(label, value, minimum, maximum, step) end

---@param label string
---@param value string
---@param maximum_length? integer
---@return string value
---@return boolean changed
---@return boolean committed
function Context:InputText(label, value, maximum_length) end

---@param label string
---@param value string
---@param maximum_length? integer
---@return string value
---@return boolean changed
---@return boolean committed
function Context:TextInput(label, value, maximum_length) end

---@param label string
---@param items string[]
---@param selected_index integer
---@return integer selected_index
function Context:Dropdown(label, items, selected_index) end

---@param label string
---@param items string[]
---@param selected_index integer
---@return integer selected_index
function Context:Combo(label, items, selected_index) end

---@param label string
---@param color OverdubUIColor
---@return OverdubUIColor color
function Context:ColorEditor(label, color) end

---@param label string
---@param value string
---@return string value
function Context:Keybind(label, value) end

---@param id string
---@param height number
---@param callback fun(context: OverdubUIContext)
---@return integer scroll_x
---@return integer scroll_y
function Context:Group(id, height, callback) end

---@param id string
---@return integer scroll_x
---@return integer scroll_y
function Context:GroupScroll(id) end

---@param id string
---@param scroll_x integer
---@param scroll_y integer
function Context:SetGroupScroll(id, scroll_x, scroll_y) end

---@param title string
---@param expanded boolean
---@param callback fun(context: OverdubUIContext)
---@return boolean expanded
function Context:Tree(title, expanded, callback) end

---@param text string
function Context:Tooltip(text) end

function Context:Separator() end

---@param count? integer
function Context:Spacing(count) end

---@param columns integer
---@param height? number
function Context:Row(columns, height) end

---@param columns integer
---@param width number
---@param height? number
function Context:RowStatic(columns, width, height) end

---@param ratios number[]
---@param height? number
function Context:RowRatios(ratios, height) end

---@param widths number[]
---@param height? number
function Context:RowWidths(widths, height) end

---@alias OverdubUIGridColumn number|"Content"|"Flex"

---@overload fun(self: OverdubUIContext, id: string, columns: OverdubUIGridColumn[], height: number, callback: fun(context: OverdubUIContext))
---@param id string
---@param columns OverdubUIGridColumn[]
---@param callback fun(context: OverdubUIContext)
function Context:Grid(id, columns, callback) end

function Context:NextRow() end

---@return integer width
---@return integer height
function Context:ViewportSize() end

---@class OverdubUIModule
local ui = {}

---@overload fun(id: string, title: string, callback: fun(context: OverdubUIContext)): OverdubUIWindow
---@param id string
---@param callback fun(context: OverdubUIContext)
---@return OverdubUIWindow
function ui.RegisterWindow(id, callback) end

return ui
