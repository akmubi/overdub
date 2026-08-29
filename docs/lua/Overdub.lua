---@meta

---@class OverdubLog
---@field Debug fun(format: string, ...: any)
---@field Info fun(format: string, ...: any)
---@field Warn fun(format: string, ...: any)
---@field Error fun(format: string, ...: any)

---@class OverdubCommandHandle
local Command = {}

---@return boolean removed_now
function Command:Remove() end

---@return boolean
function Command:IsActive() end

---@class OverdubDirectoryEntry
---@field Name string
---@field Path string
---@field IsDirectory boolean
---@field Size integer

---@class OverdubIniSection
---@field Name string
---@field Argument string?
---@field Lines string[]
---@field Values table<string, string>

---@class OverdubModule
---@field Id string Stable ID from mod.ini.
---@field Name string Display name from mod.ini.
---@field GameDir string Game installation directory.
---@field RootDir string Root directory containing installed mods.
---@field ModDir string Directory of the current mod.
---@field Log OverdubLog
local overdub = {}

---@return integer
function overdub.Frame() end

---@return integer
function overdub.NowUs() end

---@param name string
---@param callback fun(args: string)
---@return OverdubCommandHandle
function overdub.RegisterCommand(name, callback) end

---@param path string
---@param pattern? string
---@return OverdubDirectoryEntry[]? entries
---@return string? error
function overdub.ListDirectory(path, pattern) end

---@param path string
---@return OverdubIniSection[]? sections
---@return string? error
function overdub.ReadIni(path) end

---@class OverdubLifecycle
---@field Startup? fun()
---@field Tick? fun(delta_seconds: number)
---@field Input? fun(event: OverdubInputEvent): boolean?
---@field DrawPanel? fun(context: OverdubUIContext)
---@field DrawConfig? fun(context: OverdubUIContext)
---@field Shutdown? fun()

return overdub
