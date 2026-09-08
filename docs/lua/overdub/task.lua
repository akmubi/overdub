---@meta

---@class OverdubTask
local Task = {}

---@return boolean cancelled_now
function Task:Cancel() end

---@return boolean
function Task:IsDone() end

---@return boolean
function Task:IsCancelled() end

---@class OverdubFuture
local Future = {}

---@return boolean
function Future:IsPending() end

---@return boolean
function Future:IsDone() end

---@return boolean
function Future:IsCancelled() end

---@return boolean cancelled_now
function Future:Cancel() end

---@param callback fun(result: any)
---@return OverdubFuture self
function Future:OnSuccess(callback) end

---@param callback fun(message: string)
---@return OverdubFuture self
function Future:OnError(callback) end

---@param callback fun(future: OverdubFuture)
---@return OverdubFuture self
function Future:OnComplete(callback) end

---Waits cooperatively and must be called from a task created by task.Spawn.
---@return any result
function Future:Await() end

---Returns the completed result or raises if pending, failed, or cancelled.
---@return any result
function Future:GetResult() end

---@return string?
function Future:GetError() end

---@class OverdubTaskModule
local task = {}

---@overload fun(name: string, callback: fun()): OverdubTask
---@param callback fun()
---@return OverdubTask
function task.Spawn(callback) end

function task.YieldFrame() end

---@param seconds number
function task.Sleep(seconds) end

---@return boolean
function task.ShouldYield() end

---Runs module[function_name](value) in this mod's serialized worker Lua state.
---@param module_name string
---@param function_name string
---@param value any
---@return OverdubFuture
function task.Offload(module_name, function_name, value) end

return task
