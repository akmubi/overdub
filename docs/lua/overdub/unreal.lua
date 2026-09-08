---@meta

---@class UObject
local UObject = {}

---@return boolean
function UObject:IsValid() end

---@return string
function UObject:GetName() end

---@return string
function UObject:GetFullName() end

---@return string
function UObject:GetPathName() end

---@return FName
function UObject:GetFName() end

---@return UObject?
function UObject:GetOuter() end

---@return UClass
function UObject:GetClass() end

---@param class UClass|string
---@return boolean
function UObject:IsA(class) end

---@return integer
function UObject:GetInternalIndex() end

---@return integer
function UObject:GetAddress() end

---@return integer
function UObject:GetFlags() end

---@return integer
function UObject:GetInternalFlags() end

---@param flags integer
---@return boolean
function UObject:HasAnyFlags(flags) end

---@param flags integer
---@return boolean
function UObject:HasAllFlags(flags) end

---@param flags integer
---@return boolean
function UObject:HasAnyInternalFlags(flags) end

---@return boolean
function UObject:IsRooted() end

---@return boolean claimed_now
function UObject:AddToRoot() end

---@return boolean released_now
function UObject:RemoveFromRoot() end

---@return boolean
function UObject:IsAnyClass() end

---@return boolean
function UObject:IsClass() end

---@return UObject?
function UObject:GetWorld() end

---@param function_object UFunction
---@param ... any
---@return ... any
function UObject:CallFunction(function_object, ...) end

---@return UObjectReflection
function UObject:Reflection() end

---@param name string
---@return any
function UObject:GetPropertyValue(name) end

---@param name string
---@param value any
function UObject:SetPropertyValue(name, value) end

---@param property_name string
---@param target UObject
---@param function_object UFunction|string
---@return UnrealDelegateHandle
function UObject:BindDelegate(property_name, target, function_object) end

---@return string
function UObject:Type() end

---@class UStruct: UObject
local UStruct = {}

---@return UStruct?
function UStruct:GetSuperStruct() end

---@param callback fun(property: UnrealProperty): boolean?
function UStruct:ForEachProperty(callback) end

---@param callback fun(function_object: UFunction): boolean?
function UStruct:ForEachFunction(callback) end

---@return integer
function UStruct:GetStructureSize() end

---@return integer
function UStruct:GetMinAlignment() end

---@class UnrealImplementedInterface
---@field Class UClass?
---@field PointerOffset integer
---@field ImplementedByK2 boolean

---@class UClass: UStruct
local UClass = {}

---@return UObject?
function UClass:GetCDO() end

---@param parent UClass
---@return boolean
function UClass:IsChildOf(parent) end

---@return integer
function UClass:GetClassFlags() end

---@param flags integer
---@return boolean
function UClass:HasAnyClassFlags(flags) end

---@param flags integer
---@return boolean
function UClass:HasAllClassFlags(flags) end

---@return UClass?
function UClass:GetWithinClass() end

---@return UObject?
function UClass:GetGeneratedBy() end

---@return UnrealImplementedInterface[]
function UClass:GetInterfaces() end

---@param outer? UObject
---@param name? string
---@param flags? integer
---@param template? UObject
---@param copy_transients_from_class_defaults? boolean
---@return UObject?
function UClass:ConstructObject(outer, name, flags, template, copy_transients_from_class_defaults) end

---@param world_context? UObject
---@return UObject?
function UClass:SpawnActor(world_context) end

---@class UScriptStruct: UStruct

---@class UFunction: UStruct
local UFunction = {}

---@return integer
function UFunction:GetFunctionFlags() end

---@param flags integer
function UFunction:SetFunctionFlags(flags) end

---@return integer
function UFunction:GetParameterCount() end

---@return integer
function UFunction:GetParameterSize() end

---@return integer?
function UFunction:GetReturnValueOffset() end

---@class UEnum: UObject
local UEnum = {}

---@param value integer
---@return FName?
function UEnum:GetNameByValue(value) end

---@param index integer Zero-based index.
---@return FName? name
---@return integer? value
function UEnum:GetEnumNameByIndex(index) end

---@param callback fun(name: FName, value: integer): boolean?
function UEnum:ForEachName(callback) end

---@return integer
function UEnum:NumEnums() end

---@param index integer Zero-based index.
---@param name FName|string
function UEnum:EditNameAt(index, name) end

---@param index integer Zero-based index.
---@param value integer
function UEnum:EditValueAt(index, value) end

---@class UnrealDataTableEntry
---@field Name string
---@field Data UnrealDataTableRow

---@class UDataTable: UObject
local UDataTable = {}

---@return UScriptStruct?
function UDataTable:GetRowStruct() end

---@return table<string, UnrealDataTableRow>
function UDataTable:GetRowMap() end

---@param row_name string|FName
---@return UnrealDataTableRow?
function UDataTable:FindRow(row_name) end

---@param row_name string|FName
---@param value table|UnrealValue|UnrealDataTableRow
function UDataTable:AddRow(row_name, value) end

---@param row_name string|FName
function UDataTable:RemoveRow(row_name) end

function UDataTable:EmptyTable() end

---@return string[]
function UDataTable:GetRowNames() end

---@return UnrealDataTableEntry[]
function UDataTable:GetAllRows() end

---@param callback fun(name: string, row: UnrealDataTableRow): boolean?
function UDataTable:ForEachRow(callback) end

---@class FName
local FName = {}

---@return string
function FName:ToString() end

---@return integer
function FName:GetComparisonIndex() end

---@param other FName|string
---@return boolean
function FName:Equals(other) end

---@return string
function FName:Type() end

---@class FText
local FText = {}

---@return string
function FText:ToString() end

---@return string
function FText:Type() end

---@class UnrealValue
local UnrealValue = {}

---@return boolean
function UnrealValue:IsValid() end

---@return string
function UnrealValue:Type() end

---@return integer
function UnrealValue:Num() end

---@return integer
function UnrealValue:GetArrayNum() end

---@return integer
function UnrealValue:GetArrayAddress() end

---@return integer
function UnrealValue:GetArrayMax() end

---@return integer
function UnrealValue:GetArrayDataAddress() end

---@param callback fun(key: any, value: any): boolean?
function UnrealValue:ForEach(callback) end

---@return table
function UnrealValue:ToTable() end

function UnrealValue:Clear() end

function UnrealValue:Empty() end

---@overload fun(self: UnrealValue, key: any, value: any): boolean
---@param value any
---@return integer|boolean
function UnrealValue:Add(value) end

---@param index integer
---@param value any
function UnrealValue:Insert(index, value) end

---@param value any
---@return boolean
function UnrealValue:Contains(value) end

---@param value any
---@return integer|any
function UnrealValue:Find(value) end

---@param value any
---@return boolean
function UnrealValue:Remove(value) end

---@return string
function UnrealValue:ToString() end

---@return integer
function UnrealValue:Len() end

---@return boolean
function UnrealValue:IsEmpty() end

---@param value string
function UnrealValue:Append(value) end

---@param value string
---@return boolean
function UnrealValue:StartsWith(value) end

---@param value string
---@return boolean
function UnrealValue:EndsWith(value) end

---@return UnrealValue
function UnrealValue:ToUpper() end

---@return UnrealValue
function UnrealValue:ToLower() end

---@class UnrealDataTableRow
local UnrealDataTableRow = {}

---@return boolean
function UnrealDataTableRow:IsValid() end

---@param name string
---@return any
function UnrealDataTableRow:GetPropertyValue(name) end

---@param name string
---@param value any
function UnrealDataTableRow:SetPropertyValue(name, value) end

---@return string
function UnrealDataTableRow:Type() end

---@return string
function UnrealDataTableRow:ToString() end

---@class UnrealProperty
local UnrealProperty = {}

---@return boolean
function UnrealProperty:IsValid() end

---@return string
function UnrealProperty:GetFullName() end

---@return FName
function UnrealProperty:GetFName() end

---@param class UnrealFieldClass|string|FName
---@return boolean
function UnrealProperty:IsA(class) end

---@return UnrealFieldClass
function UnrealProperty:GetClass() end

---@return integer
function UnrealProperty:GetOffset_Internal() end

---@return integer
function UnrealProperty:GetArrayDim() end

---@return integer
function UnrealProperty:GetElementSize() end

---@return UClass
function UnrealProperty:GetPropertyClass() end

---@return integer
function UnrealProperty:GetByteMask() end

---@return integer
function UnrealProperty:GetByteOffset() end

---@return integer
function UnrealProperty:GetFieldMask() end

---@return integer
function UnrealProperty:GetFieldSize() end

---@return UScriptStruct?
function UnrealProperty:GetStruct() end

---@return UnrealProperty?
function UnrealProperty:GetInner() end

---@return UnrealProperty?
function UnrealProperty:GetElementProperty() end

---@return UnrealProperty?
function UnrealProperty:GetKeyProperty() end

---@return UnrealProperty?
function UnrealProperty:GetValueProperty() end

---@return UnrealProperty?
function UnrealProperty:GetUnderlyingProperty() end

---@return UEnum?
function UnrealProperty:GetEnum() end

---@return string
function UnrealProperty:Type() end

---@class UnrealFieldClass
local UnrealFieldClass = {}

---@return boolean
function UnrealFieldClass:IsValid() end

---@return FName
function UnrealFieldClass:GetFName() end

---@return string
function UnrealFieldClass:Type() end

---@class UObjectReflection
local UObjectReflection = {}

---@return boolean
function UObjectReflection:IsValid() end

---@param name string|FName
---@return UnrealProperty?
function UObjectReflection:GetProperty(name) end

---@return string
function UObjectReflection:Type() end

---@class UnrealCall
local UnrealCall = {}

---@return boolean
function UnrealCall:IsValid() end

---@return UObject
function UnrealCall:GetObject() end

---@return UFunction
function UnrealCall:GetFunction() end

---@return "Before"|"After"
function UnrealCall:GetPhase() end

---@return boolean
function UnrealCall:IsDeferred() end

---@return boolean
function UnrealCall:IsSkipped() end

---@param name string
---@return any
function UnrealCall:Get(name) end

---@param name string
---@param value any
function UnrealCall:Set(name, value) end

---@return any
function UnrealCall:GetReturnValue() end

---@param value any
function UnrealCall:SetReturnValue(value) end

---@return boolean changed
function UnrealCall:Skip() end

---@class UnrealHookHandle
local UnrealHookHandle = {}

---@return boolean removed_now
function UnrealHookHandle:Remove() end

---@return boolean
function UnrealHookHandle:IsActive() end

---@class UnrealDelegateHandle
local UnrealDelegateHandle = {}

---@return boolean removed_now
function UnrealDelegateHandle:Remove() end

---@return boolean
function UnrealDelegateHandle:IsActive() end

---@class UnrealNotificationHandle
local UnrealNotificationHandle = {}

---@return boolean removed_now
function UnrealNotificationHandle:Remove() end

---@return boolean
function UnrealNotificationHandle:IsActive() end

---@class UnrealHookCallbacks
---@field Before? fun(call: UnrealCall)
---@field After? fun(call: UnrealCall)

---@class UnrealPostLoadCallbacks
---@field Before? fun(object: UObject)
---@field After? fun(object: UObject)

---@alias OverdubUnrealPropertyTypeName "Bool"|"Byte"|"Int8"|"Int16"|"Int32"|"Int64"|"UInt16"|"UInt32"|"UInt64"|"Float"|"Double"|"Name"|"String"|"Text"|"Object"|"Class"|"SoftObject"|"SoftClass"|"WeakObject"|"LazyObject"|"Interface"|"Struct"|"Enum"|"Array"|"Set"|"Map"

---@class OverdubUnrealPropertyType
---@field Type OverdubUnrealPropertyTypeName
---@field Class? UClass
---@field Struct? UScriptStruct
---@field Enum? UEnum
---@field Underlying? OverdubUnrealPropertyTypeName|OverdubUnrealPropertyType
---@field Inner? OverdubUnrealPropertyTypeName|OverdubUnrealPropertyType
---@field Key? OverdubUnrealPropertyTypeName|OverdubUnrealPropertyType
---@field Value? OverdubUnrealPropertyTypeName|OverdubUnrealPropertyType

---@class OverdubUnrealPropertyDefinition: OverdubUnrealPropertyType
---@field Name string
---@field Flags? integer
---@field Default? any
---@field Const? boolean

---@class OverdubUnrealFunctionDefinition
---@field Name string
---@field Flags? integer
---@field Parameters? OverdubUnrealPropertyDefinition[]
---@field Return? OverdubUnrealPropertyTypeName|OverdubUnrealPropertyType
---@field Implementation fun(self: UObject, ...: any): any

---@class OverdubUnrealClassDefinition
---@field Parent UClass
---@field Properties? OverdubUnrealPropertyDefinition[]
---@field Functions? OverdubUnrealFunctionDefinition[]

---@class OverdubUnrealModule
local unreal = {}

---@param value string|integer|FName
---@param find_type? integer
---@return FName
function unreal.FName(value, find_type) end

---@param value string
---@return FText
function unreal.FText(value) end

---@type FName
unreal.NAME_None = nil

---@type table<string, integer>
unreal.EObjectFlags = {}

---@type table<string, integer>
unreal.EInternalObjectFlags = {}

---@type table<string, integer>
unreal.EClassFlags = {}

---@type table<string, integer>
unreal.EPropertyFlags = {}

---@type table<string, integer>
unreal.EFunctionFlags = {}

---@type table<string, string>
unreal.PropertyTypes = {}

---@type table<string, integer>
unreal.EFindName = {}

---@param path string
---@param class? UClass
---@return UObject?
function unreal.FindObject(path, class) end

---@return integer
function unreal.GetObjectCount() end

---@param index integer Zero-based index.
---@return UObject?
function unreal.GetObjectByIndex(index) end

---@return fun(): integer?, UObject?
function unreal.Objects() end

---@param path string
---@param class? UClass
---@param outer? UObject
---@return UObject?
function unreal.LoadObject(path, class, outer) end

---@param class UClass
---@param outer? UObject
---@param name? string
---@param flags? integer
---@param template? UObject
---@param copy_transients_from_class_defaults? boolean
---@return UObject?
function unreal.ConstructObject(class, outer, name, flags, template, copy_transients_from_class_defaults) end

---@param class UClass
---@param world_context? UObject
---@return UObject?
function unreal.SpawnActor(class, world_context) end

---@param name string
---@param definition OverdubUnrealClassDefinition
---@return UClass
function unreal.DefineClass(name, definition) end

---@param source UObject
---@param property_name string
---@param target UObject
---@param function_object UFunction|string
---@return UnrealDelegateHandle
function unreal.BindDelegate(source, property_name, target, function_object) end

---@param class UClass|string
---@param callback fun(object: UObject)
---@return UnrealNotificationHandle
function unreal.NotifyOnNewObject(class, callback) end

---@param object UObject
---@param callback fun()
---@return UnrealNotificationHandle
function unreal.NotifyOnDeleteObject(object, callback) end

---@param function_object UFunction|string
---@param callbacks UnrealHookCallbacks
---@return UnrealHookHandle
function unreal.Hook(function_object, callbacks) end

---@param class UClass|string
---@param callbacks UnrealPostLoadCallbacks
---@return UnrealHookHandle
function unreal.HookPostLoad(class, callbacks) end

---@return UObject?
function unreal.GetCurrentWorld() end

---@return UObject?
function unreal.GetTransientPackage() end

return unreal
