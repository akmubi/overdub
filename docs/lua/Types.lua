---@meta
---@diagnostic disable: missing-return, duplicate-set-field, lowercase-global

-- Overdub Lua API declarations.
--
-- This is a LuaLS/EmmyLua metadata file, modeled after UE4SS's Types.lua.
-- It documents the API implemented by this Overdub revision and must not be
-- required or executed by a mod. This file describes the UE4SS compatibility
-- runtime. Reflected UFunction calls are automatically bridged to the game
-- thread; use ExecuteInGameThread for multi-step game-state operations.

---@alias UnrealName string|FName
---@alias UnrealClassFilter string|FName|UClass
---@alias UnrealKey integer|string
---@alias ScheduledActionHandle integer
---@alias PropertyType string

---@class EObjectFlagsTable
---@field RF_NoFlags integer
---@field RF_Public integer
---@field RF_Standalone integer
---@field RF_MarkAsNative integer
---@field RF_Transactional integer
---@field RF_ClassDefaultObject integer
---@field RF_ArchetypeObject integer
---@field RF_Transient integer
---@field RF_MarkAsRootSet integer
---@field RF_TagGarbageTemp integer
---@field RF_NeedInitialization integer
---@field RF_NeedLoad integer
---@field RF_KeepForCooker integer
---@field RF_NeedPostLoad integer
---@field RF_NeedPostLoadSubobjects integer
---@field RF_NewerVersionExists integer
---@field RF_BeginDestroyed integer
---@field RF_FinishDestroyed integer
---@field RF_BeingRegenerated integer
---@field RF_DefaultSubObject integer
---@field RF_WasLoaded integer
---@field RF_TextExportTransient integer
---@field RF_LoadCompleted integer
---@field RF_InheritableComponentTemplate integer
---@field RF_DuplicateTransient integer
---@field RF_StrongRefOnFrame integer
---@field RF_NonPIEDuplicateTransient integer
---@field RF_Dynamic integer
---@field RF_WillBeLoaded integer
---@field RF_HasExternalPackage integer

---@type EObjectFlagsTable
EObjectFlags = {}

---@class EInternalObjectFlagsTable
---@field ReachableInCluster integer
---@field ClusterRoot integer
---@field Native integer
---@field Async integer
---@field AsyncLoading integer
---@field Unreachable integer
---@field PendingKill integer
---@field RootSet integer
---@field GarbageCollectionKeepFlags integer
---@field AllFlags integer

---@type EInternalObjectFlagsTable
EInternalObjectFlags = {}

---@class EFindNameTable
---@field FNAME_Find integer Do not create a missing name.
---@field FNAME_Add integer Find or add the name.

---@type EFindNameTable
EFindName = {}

---@class PropertyTypesTable
---@field ObjectProperty PropertyType
---@field ObjectPtrProperty PropertyType Alias of ObjectProperty on UE 4.27.
---@field Int8Property PropertyType
---@field Int16Property PropertyType
---@field IntProperty PropertyType
---@field Int32Property PropertyType
---@field Int64Property PropertyType
---@field NameProperty PropertyType
---@field FloatProperty PropertyType
---@field DoubleProperty PropertyType
---@field StrProperty PropertyType
---@field ByteProperty PropertyType
---@field UInt16Property PropertyType
---@field UIntProperty PropertyType Alias of UInt32Property.
---@field UInt32Property PropertyType
---@field UInt64Property PropertyType
---@field BoolProperty PropertyType
---@field ArrayProperty PropertyType
---@field SetProperty PropertyType
---@field MapProperty PropertyType
---@field StructProperty PropertyType
---@field ClassProperty PropertyType
---@field SoftObjectProperty PropertyType
---@field SoftClassProperty PropertyType
---@field WeakObjectProperty PropertyType
---@field LazyObjectProperty PropertyType
---@field EnumProperty PropertyType
---@field TextProperty PropertyType
---@field InterfaceProperty PropertyType
---@field DelegateProperty PropertyType
---@field MulticastDelegateProperty PropertyType

---@type PropertyTypesTable
PropertyTypes = {}

---@class FName
local FNameType = {}

---@return string
function FNameType:ToString() end

---@return integer
function FNameType:GetComparisonIndex() end

---@param other FName
---@return boolean
function FNameType:Equals(other) end

---@return 'FName'
function FNameType:type() end

---@param value string|integer|FName
---@param find_type? integer EFindName.FNAME_Find or EFindName.FNAME_Add.
---@return FName
function FName(value, find_type) end

---@type FName
NAME_None = nil

---@class FText
local FTextType = {}

---@return string
function FTextType:ToString() end

---@return 'FText'
function FTextType:type() end

---@param value string
---@return FText
function FText(value) end

---@class FString
local FString = {}

---@return string
function FString:ToString() end

function FString:Empty() end
function FString:Clear() end

---@return integer
function FString:Len() end

---@return boolean
function FString:IsEmpty() end

---@param value string|FString
function FString:Append(value) end

---@param value string
---@return integer? index One-based byte index, or nil when absent.
function FString:Find(value) end

---@param prefix string
---@return boolean
function FString:StartsWith(prefix) end

---@param suffix string
---@return boolean
function FString:EndsWith(suffix) end

---@return FString
function FString:ToUpper() end

---@return FString
function FString:ToLower() end

---@return boolean
function FString:IsValid() end

---@return 'FString'
function FString:type() end

---@class RemoteUnrealParam<T>
local RemoteUnrealParam = {}

---@generic T
---@return T
function RemoteUnrealParam:Get() end

---@generic T
---@return T
function RemoteUnrealParam:get() end

---@generic T
---@param value T
function RemoteUnrealParam:Set(value) end

---@generic T
---@param value T
function RemoteUnrealParam:set(value) end

---@return boolean
function RemoteUnrealParam:IsValid() end

---@return 'RemoteUnrealParam'|'LocalUnrealParam'
function RemoteUnrealParam:type() end

---@class LocalUnrealParam<T>: RemoteUnrealParam<T>

---@class TArray<T>: { [integer]: T }
local TArray = {}

---@return integer
function TArray:Num() end

---@return integer
function TArray:GetArrayNum() end

---@return integer
function TArray:GetArrayAddress() end

---@return integer
function TArray:GetArrayMax() end

---@return integer
function TArray:GetArrayDataAddress() end

---@generic T
---@param callback fun(index: integer, element: LocalUnrealParam<T>): boolean?
function TArray:ForEach(callback) end

---@generic T
---@return T[]
function TArray:ToTable() end

function TArray:Empty() end
function TArray:Clear() end

---@generic T
---@param value T
---@return integer index One-based index.
function TArray:Add(value) end

---@generic T
---@param index integer One-based insertion index.
---@param value T
function TArray:Insert(index, value) end

---@generic T
---@param value T
---@return boolean
function TArray:Contains(value) end

---@param index integer One-based removal index.
---@return boolean
function TArray:Remove(index) end

---@return boolean
function TArray:IsValid() end

---@return 'TArray'
function TArray:type() end

---@class TSet<T>
local TSet = {}

---@return integer
function TSet:Num() end

---@generic T
---@param value T
---@return integer
function TSet:Add(value) end

---@generic T
---@param value T
---@return boolean
function TSet:Contains(value) end

---@generic T
---@param value T
---@return boolean
function TSet:Remove(value) end

function TSet:Empty() end
function TSet:Clear() end

---@generic T
---@param callback fun(element: T): boolean?
function TSet:ForEach(callback) end

---@generic T
---@return T[]
function TSet:ToTable() end

---@return boolean
function TSet:IsValid() end

---@return 'TSet'
function TSet:type() end

---@class TMap<K, V>: { [K]: V }
local TMap = {}

---@return integer
function TMap:Num() end

---@generic K, V
---@param key K
---@return V
function TMap:Find(key) end

---@generic K, V
---@param key K
---@param value V
function TMap:Add(key, value) end

---@generic K
---@param key K
---@return boolean
function TMap:Contains(key) end

---@generic K
---@param key K
---@return boolean
function TMap:Remove(key) end

function TMap:Empty() end
function TMap:Clear() end

---@generic K, V
---@param callback fun(key: RemoteUnrealParam<K>, value: RemoteUnrealParam<V>): boolean?
function TMap:ForEach(callback) end

---@generic K, V
---@return table<K, V>
function TMap:ToTable() end

---@return boolean
function TMap:IsValid() end

---@return 'TMap'
function TMap:type() end

---@class UScriptStructValue
local UScriptStructValue = {}

---@return boolean
function UScriptStructValue:IsValid() end

---@return 'UScriptStruct'
function UScriptStructValue:type() end

---@class UDataTableRow: UScriptStructValue
local UDataTableRow = {}

---@return string
function UDataTableRow:ToString() end

---@class FieldClass
local FieldClass = {}

---@return boolean
function FieldClass:IsValid() end

---@return FName
function FieldClass:GetFName() end

---@return 'FieldClass'
function FieldClass:type() end

---@class Property
local Property = {}

---@return boolean
function Property:IsValid() end

---@return string
function Property:GetFullName() end

---@return FName
function Property:GetFName() end

---@param property_type PropertyType|FName|FieldClass
---@return boolean
function Property:IsA(property_type) end

---@return FieldClass
function Property:GetClass() end

---@return integer
function Property:GetOffset_Internal() end

---@return integer
function Property:GetArrayDim() end

---@return integer
function Property:GetElementSize() end

---@return UClass
function Property:GetPropertyClass() end

---@return integer
function Property:GetByteMask() end

---@return integer
function Property:GetByteOffset() end

---@return integer
function Property:GetFieldMask() end

---@return integer
function Property:GetFieldSize() end

---@return UScriptStruct
function Property:GetStruct() end

---@return Property
function Property:GetInner() end

---@return Property
function Property:GetElementProperty() end

---@return Property
function Property:GetKeyProperty() end

---@return Property
function Property:GetValueProperty() end

---@return Property
function Property:GetUnderlyingProperty() end

---@return UEnum?
function Property:GetEnum() end

---@return 'Property'
function Property:type() end

---@class UObjectReflection
local UObjectReflection = {}

---@return boolean
function UObjectReflection:IsValid() end

---Returns an invalid Property handle, rather than nil, when the property is absent.
---@param property_name UnrealName
---@return Property
function UObjectReflection:GetProperty(property_name) end

---@return 'UObjectReflection'
function UObjectReflection:type() end

---@class UObject
local UObject = {}

---@return boolean
function UObject:IsValid() end

---@return string
function UObject:GetFullName() end

---@return FName
function UObject:GetFName() end

---@return integer
function UObject:GetAddress() end

---@return UClass
function UObject:GetClass() end

---@return UObject
function UObject:GetOuter() end

---@return UWorld?
function UObject:GetWorld() end

---@return boolean
function UObject:IsAnyClass() end

---@return boolean
function UObject:IsClass() end

---@param class UClass|string
---@return boolean
function UObject:IsA(class) end

---@param flags integer
---@return boolean
function UObject:HasAllFlags(flags) end

---@param flags integer
---@return boolean
function UObject:HasAnyFlags(flags) end

---@param flags integer
---@return boolean
function UObject:HasAnyInternalFlags(flags) end

---@return UObjectReflection
function UObject:Reflection() end

---@param property_name string
---@return any
function UObject:GetPropertyValue(property_name) end

---@param property_name string
---@param value any
function UObject:SetPropertyValue(property_name, value) end

---@param func UFunction
---@param ... any
---@return ... any
function UObject:CallFunction(func, ...) end

---@return string
function UObject:type() end

---@class UWorld: UObject

---@class UStruct: UObject
local UStruct = {}

---@return UStruct?
function UStruct:GetSuperStruct() end

---@param callback fun(func: UFunction): boolean?
function UStruct:ForEachFunction(callback) end

---Iterates only properties declared directly by this struct; walk GetSuperStruct for inherited fields.
---@param callback fun(property: Property): boolean?
function UStruct:ForEachProperty(callback) end

---@class UScriptStruct: UStruct

---@class UClass: UStruct
local UClass = {}

---@return UObject
function UClass:GetCDO() end

---@param parent UClass
---@return boolean
function UClass:IsChildOf(parent) end

---@class UFunction: UStruct
local UFunction = {}

---@return integer
function UFunction:GetFunctionFlags() end

---@param flags integer
function UFunction:SetFunctionFlags(flags) end

---@param receiver UObject
---@param ... any
---@return ... any
function UFunction:__call(receiver, ...) end

---@class UEnum: UObject
local UEnum = {}

---@param value integer
---@return FName?
function UEnum:GetNameByValue(value) end

---@param callback fun(name: FName, value: integer): boolean?
function UEnum:ForEachName(callback) end

---@param index integer Zero-based index.
---@return FName?, integer?
function UEnum:GetEnumNameByIndex(index) end

---@param index integer Zero-based index.
---@param name string|FName
function UEnum:EditNameAt(index, name) end

---@param index integer Zero-based index.
---@param value integer
function UEnum:EditValueAt(index, value) end

---@class UDataTable: UObject
local UDataTable = {}

---@return UScriptStruct
function UDataTable:GetRowStruct() end

---@return table<string, UDataTableRow>
function UDataTable:GetRowMap() end

---@param row_name UnrealName
---@return UDataTableRow?
function UDataTable:FindRow(row_name) end

---@param row_name UnrealName
---@param row UDataTableRow|UScriptStructValue|table
function UDataTable:AddRow(row_name, row) end

---@param row_name UnrealName
function UDataTable:RemoveRow(row_name) end

function UDataTable:EmptyTable() end

---@return string[]
function UDataTable:GetRowNames() end

---@class UDataTableNamedRow
---@field Name string
---@field Data UDataTableRow

---@return UDataTableNamedRow[]
function UDataTable:GetAllRows() end

---@param callback fun(name: string, row: UDataTableRow): boolean?
function UDataTable:ForEachRow(callback) end

---@class UnrealAPI
---@field EObjectFlags EObjectFlagsTable
---@field EInternalObjectFlags EInternalObjectFlagsTable
---@field EFindName EFindNameTable
---@field PropertyTypes PropertyTypesTable
---@field NAME_None FName
local UnrealAPI = {}

---@overload fun(name: string): UObject
---@overload fun(class: UClass?, outer: UObject?, name: string, exact_class?: boolean): UObject
---@param class_or_name UnrealClassFilter?
---@param object_name UnrealName?
---@param required_flags? integer
---@param banned_flags? integer
---@return UObject An invalid UObject handle is returned when no object matches.
function UnrealAPI.StaticFindObject(class_or_name, object_name, required_flags, banned_flags) end

---@overload fun(name: string): UObject
---@overload fun(class: UClass?, outer: UObject?, name: string, exact_class?: boolean): UObject
---@param class_or_name UnrealClassFilter?
---@param object_name UnrealName?
---@param required_flags? integer
---@param banned_flags? integer
---@return UObject An invalid UObject handle is returned when no object matches.
function UnrealAPI.FindObject(class_or_name, object_name, required_flags, banned_flags) end

---@param limit integer Zero means unlimited.
---@param class_or_name UnrealClassFilter?
---@param object_name UnrealName?
---@param required_flags? integer
---@param banned_flags? integer
---@param exact_class? boolean
---@return UObject[]
function UnrealAPI.FindObjects(limit, class_or_name, object_name, required_flags, banned_flags, exact_class) end

---@param class_name string
---@return UObject An invalid UObject handle is returned when no object matches.
function UnrealAPI.FindFirstOf(class_name) end

---@param class_name string
---@return UObject[]?
function UnrealAPI.FindAllOf(class_name) end

---@param path string
---@return UObject?
function UnrealAPI.LoadAsset(path) end

---@param value string|integer|FName
---@param find_type? integer
---@return FName
function UnrealAPI.FName(value, find_type) end

---@param value string
---@return FText
function UnrealAPI.FText(value) end

---@return UObject
function UnrealAPI.CreateInvalidObject() end

---@type UnrealAPI
Unreal = {}

---@overload fun(name: string): UObject
---@overload fun(class: UClass?, outer: UObject?, name: string, exact_class?: boolean): UObject
---@param class_or_name UnrealClassFilter?
---@param object_name UnrealName?
---@param required_flags? integer
---@param banned_flags? integer
---@return UObject An invalid UObject handle is returned when no object matches.
function StaticFindObject(class_or_name, object_name, required_flags, banned_flags) end

---@overload fun(name: string): UObject
---@overload fun(class: UClass?, outer: UObject?, name: string, exact_class?: boolean): UObject
---@param class_or_name UnrealClassFilter?
---@param object_name UnrealName?
---@param required_flags? integer
---@param banned_flags? integer
---@return UObject An invalid UObject handle is returned when no object matches.
function FindObject(class_or_name, object_name, required_flags, banned_flags) end

---@param limit integer
---@param class_or_name UnrealClassFilter?
---@param object_name UnrealName?
---@param required_flags? integer
---@param banned_flags? integer
---@param exact_class? boolean
---@return UObject[]
function FindObjects(limit, class_or_name, object_name, required_flags, banned_flags, exact_class) end

---@param class_name string
---@return UObject An invalid UObject handle is returned when no object matches.
function FindFirstOf(class_name) end

---@param class_name string
---@return UObject[]?
function FindAllOf(class_name) end

---@param path string
---@return UObject?
function LoadAsset(path) end

---@return UObject
function CreateInvalidObject() end

---@class ModRefTable
local ModRefTable = {}

---@param key any
---@return any
function ModRefTable:GetSharedVariable(key) end

---@param key any
---@param value any
function ModRefTable:SetSharedVariable(key, value) end

---@type ModRefTable
ModRef = {}

---@class GameDirectory: table<string, GameDirectory>
---@field __absolute_path string

---@return GameDirectory
function IterateGameDirectories() end

---@param name string
---@param callback fun(full_command: string, parameters: string[], output_device: nil): boolean?
function RegisterConsoleCommandHandler(name, callback) end

---@param name string
---@param callback fun(full_command: string, parameters: string[], output_device: nil): boolean?
function RegisterConsoleCommandGlobalHandler(name, callback) end

---@overload fun(key: UnrealKey, callback: fun()): boolean
---@param key UnrealKey
---@param modifiers integer[]
---@param callback fun()
---@return boolean
function RegisterKeyBind(key, modifiers, callback) end

RegisterKeyBindAsync = RegisterKeyBind

---@param key UnrealKey
---@param modifiers? integer[]
---@return boolean
function IsKeyBindRegistered(key, modifiers) end

---Runs on the compatibility worker with a deep snapshot of the reflected
---parameters. Setting a RemoteUnrealParam changes only that snapshot and does
---not alter the original Unreal call
---@param target string
---@param pre? fun(self: RemoteUnrealParam<UObject>, ...: RemoteUnrealParam)
---@param post? fun(self: RemoteUnrealParam<UObject>, ...: RemoteUnrealParam)
---@return integer pre_id, integer post_id
function RegisterHook(target, pre, post) end

---@param target string
---@param pre_id integer
---@param post_id integer
function UnregisterHook(target, pre_id, post_id) end

---Queues the callback to the compatibility worker
---@param class_path string
---@param callback fun(object: UObject)
function NotifyOnNewObject(class_path, callback) end

---@param callback fun()
function ExecuteInGameThread(callback) end

---@return boolean
function IsInGameThread() end

---@overload fun(handle: ScheduledActionHandle, delay_ms: integer, callback: fun()): ScheduledActionHandle
---@param delay_ms integer
---@param callback fun()
---@return ScheduledActionHandle
function ExecuteInGameThreadWithDelay(delay_ms, callback) end

---@param callback fun()
function ExecuteAsync(callback) end

---@param delay_ms integer
---@param callback fun()
function ExecuteWithDelay(delay_ms, callback) end

---@param delay_ms integer
---@param callback fun(): boolean?
function LoopAsync(delay_ms, callback) end

---@param frame_count integer
---@param callback fun(): boolean?
---@return ScheduledActionHandle
function LoopInGameThreadAfterFrames(frame_count, callback) end

---@param handle ScheduledActionHandle
---@return boolean
function CancelDelayedAction(handle) end

---@type table<string, integer>
Key = {}

---@class ModifierKeyTable
---@field SHIFT integer
---@field CONTROL integer
---@field ALT integer

---@type ModifierKeyTable
ModifierKey = {}

-- Intentional compatibility boundaries in this revision:
-- Property:ContainerPtrToValuePtr and Property:ImportText are not exposed;
-- direct UObject/row indexing and assignment provide lifetime-safe access.
-- UEnum insertion/removal and UObject:ProcessConsoleExec are also not exposed.
