# Lua Unreal

`overdub.unreal` exposes reflected Unreal objects directly on the game thread. It uses one UObject userdata representation. UClass, UStruct, UFunction, UEnum, and UDataTable add specialized methods while remaining UObjects.

Unreal property, function, and type names keep their original CamelCase spelling. Overdub module names and ordinary Lua variable names use snake_case.

## Find and load objects

`FindObject` searches objects that are already loaded. It accepts the path spelling returned by `GetPathName`.

```lua
local unreal = require("overdub.unreal")

local actor_class = assert(unreal.FindObject("/Script/Engine.Actor"))
local actor       = unreal.FindObject("/Game/Maps/Test.Test:PersistentLevel.MyActor", actor_class)
```

The colon in the actor path is normal Unreal object-path syntax for a subobject. It is not an Overdub-specific format.

`LoadObject` is separate because it may load an asset and perform file access:

```lua
local table_class = assert(unreal.FindObject("/Script/Engine.DataTable"))
local item_table  = assert(unreal.LoadObject("/Game/Data/Items.Items", table_class))
```

Use `GetObjectCount` and `GetObjectByIndex` for indexed access. `Objects` is the convenient iterator:

```lua
for index, object in unreal.Objects() do
    if object and object:IsA(actor_class) then
        inspect_actor(index, object)
    end
end
```

A complete object-array scan can be expensive. Put it in a cooperative task and call `task.ShouldYield` inside the loop.

## Common object methods

Every object supports validity, identity, class, outer, and flag operations.

```lua
if actor and actor:IsValid() then
    print(actor:GetName())
    print(actor:GetFullName())
    print(actor:GetPathName())
    print(actor:GetOuter())
    print(actor:GetClass())
    print(actor:GetFlags())
    print(actor:GetInternalFlags())
end
```

`IsA` accepts a UClass or a supported class identifier. `HasAnyFlags`, `HasAllFlags`, and `HasAnyInternalFlags` test bit masks.

UClass adds CDO, inheritance, interface, construction, and class-flag methods. UStruct adds property and function iteration plus layout information. UFunction and UEnum add their own metadata methods. See [`unreal.lua`](overdub/unreal.lua) for exact signatures.

## Properties and UFunctions

Indexing a UObject by an Unreal property name reads the reflected value. Assignment writes it through the shared property codec.

```lua
actor.bHidden = true

local location = actor:K2_GetActorLocation()
location.Z = location.Z + 100.0
actor:K2_SetActorLocation(location, false, true)
```

Indexing a reflected UFunction produces a bound Lua function. Inputs are marshaled from Lua values. Return and output parameters are returned in reflected order.

```lua
local strings = assert(unreal.FindObject("/Script/Engine.Default__KismetStringLibrary"))
local parts = strings:ParseIntoArray("alpha,beta", ",", true)

print(parts[1]:ToString())
print(parts[2]:ToString())
```

Structs and containers are live reflected values. Arrays use Lua's one-based indexing. Sets and maps provide operations such as `Add`, `Contains`, `Find`, `Remove`, `Clear`, `Num`, and `ToTable` where they apply.

Reflected values retain their owner information and validate it on access. A row or nested value becomes invalid after its owner is destroyed or the row is removed.

## Data tables

UDataTable has direct row helpers. Returned rows are live views.

```lua
local row = item_table:FindRow("Item001")
if row and row:IsValid() then
    row.Price = 0
    row.DisplayName = unreal.FText("Free item")
end

item_table:ForEachRow(function(name, value)
    print(name, value.Price)
    return false
end)
```

The remaining methods include `GetRowStruct`, `GetRowMap`, `AddRow`, `RemoveRow`, `EmptyTable`, `GetRowNames`, and `GetAllRows`.

Changing a table after another game system has consumed it may be too late. Use a PostLoad hook when the change must happen during the engine's load sequence.

## Track object creation and deletion

`NotifyOnNewObject` observes future instances of one class. It does not scan existing objects. Perform one targeted initial search when an instance may already exist.

```lua
local controller_class   = assert(unreal.FindObject("/Script/Engine.PlayerController"))
local current_controller = find_existing_controller()

local create_handle = unreal.NotifyOnNewObject(controller_class, function(object)
    current_controller = object
end)
```

`NotifyOnDeleteObject` watches one particular object. Its callback has no object argument because the UObject is already being removed.

```lua
local delete_handle

if current_controller then
    delete_handle = unreal.NotifyOnDeleteObject(current_controller, function()
        current_controller = nil
    end)
end
```

Notification callbacks are delivered to the owning game-thread Lua state. Their handles support `Remove` and `IsActive`. Delete notifications remove themselves after firing.

## UFunction hooks

`Hook` registers optional Before and After callbacks for one UFunction. Both run through one API because they are two phases of the same intercepted call.

```lua
local set_location = assert(unreal.FindObject("/Script/Engine.Actor:K2_SetActorLocation"))

local hook = unreal.Hook(set_location, {
    Before = function(call)
        local location = call:Get("NewLocation")
        if location.Z < 0 then
            location.Z = 0
            call:Set("NewLocation", location)
        end
    end,

    After = function(call)
        if not call:GetReturnValue() then
            print("move failed")
        end
    end,
})
```

A synchronous Before callback can change parameters, set the return value, or call `Skip` to prevent the original invocation. After sees the final outputs and can replace the return value.

The call object is borrowed for the callback. `IsValid` becomes false afterward.

Overdub never recursively enters the same Lua state. If Lua causes another hooked call while it is already running, Overdub copies the values and delivers read-only Before and After snapshots later. `IsDeferred` reports this case. A deferred callback cannot alter a call that already finished.

There is no separate public deferred-hook type. Deferral is an internal response to nested re-entry, while ordinary calls remain synchronous interceptors.

## PostLoad hooks

`HookPostLoad` matches a class and receives real UObject PostLoad calls. Before runs immediately before the engine implementation and After runs immediately after it.

```lua
local table_class = assert(unreal.FindObject("/Script/Engine.DataTable"))

local post_load_hook = unreal.HookPostLoad(table_class, {
    Before = function(object)
        prepare_table(object)
    end,

    After = function(object)
        apply_table_changes(object)
    end,
})
```

This API observes engine timing. It is not the same as calling `PostLoad` from Lua. Use the phase required by the game system that consumes the object.

## Construct objects and spawn actors

Construct a non-actor object through the module or its UClass view:

```lua
local object_class = assert(unreal.FindObject("/Script/CoreUObject.Object"))
local object       = assert(object_class:ConstructObject(
    unreal.GetTransientPackage(),
    nil,
    unreal.EObjectFlags.RF_Transient
))
```

Actor classes must use `SpawnActor`:

```lua
local actor = assert(custom_actor_class:SpawnActor(unreal.GetCurrentWorld()))
```

Construction rejects abstract classes, invalid outers, engine-managed flags, and actor classes passed to `ConstructObject`.

## Root claims

`AddToRoot` gives the current mod a root claim. Repeating it in the same mod returns false because the claim already exists. `RemoveFromRoot` releases that claim.

Claims are counted across mods. The last Overdub claim restores the object's original root state. Shutdown releases all claims held by the mod.

Rooting is not a substitute for validity checks and should not be used to keep world-owned actors alive without understanding their lifecycle.

## Custom classes

`DefineClass` creates a process-lifetime UClass scoped to the mod ID. It can inherit an existing class, add properties, change CDO defaults, and add Lua-backed UFunctions.

```lua
local object_class  = assert(unreal.FindObject("/Script/CoreUObject.Object"))
local actor_class   = assert(unreal.FindObject("/Script/Engine.Actor"))
local vector_struct = assert(unreal.FindObject("/Script/CoreUObject.Vector"))

local inventory_class = unreal.DefineClass("InventoryState", {
    Parent = object_class,

    Properties = {
        {
            Name    = "Count",
            Type    = "Int32",
            Default = 10,
        },
        {
            Name  = "Owner",
            Type  = "Object",
            Class = actor_class,
        },
        {
            Name    = "Origin",
            Type    = "Struct",
            Struct  = vector_struct,
            Default = { X = 0, Y = 0, Z = 0 },
        },
        {
            Name    = "Tags",
            Type    = "Array",
            Inner   = "Name",
            Default = { "Default", "Inventory" },
        },
        {
            Name    = "Scores",
            Type    = "Map",
            Key     = "Name",
            Value   = "Int32",
            Default = { Default = 4 },
        },
    },

    Functions = {
        {
            Name = "Add",
            Flags = unreal.EFunctionFlags.FUNC_BlueprintCallable,
            Parameters = {
                {
                    Name = "Amount",
                    Type = "Int32",
                },
            },
            Return = "Int32",
            Implementation = function(self, amount)
                self.Count = self.Count + amount
                return self.Count
            end,
        },
    },
})

local inventory = assert(inventory_class:ConstructObject())
print(inventory:Add(5))
```

Property descriptions can nest. Array and set use `Inner`. Map uses `Key` and `Value`. Object types use `Class`, structs use `Struct`, and enums use `Enum` plus an integer `Underlying` type.

Supported declarations are Bool, Byte, Int8, Int16, Int32, Int64, UInt16, UInt32, UInt64, Float, Double, Name, String, Text, Object, Class, SoftObject, SoftClass, WeakObject, LazyObject, Interface, Struct, Enum, Array, Set, and Map.

Static custom UFunctions are rejected. Classes remain registered until process exit. Stopping the mod makes their Lua implementations inactive but does not remove their layouts.

## Delegate binding

A custom UFunction can be bound to a compatible reflected delegate property.

```lua
local handle = actor:BindDelegate("OnDestroyed", inventory, "HandleDestroyed")

if handle:IsActive() then
    -- handle:Remove() can unbind it early.
end
```

`unreal.BindDelegate(actor, "OnDestroyed", inventory, "HandleDestroyed")` is the equivalent module form. The target function must be non-static and match the delegate signature. Bindings are removed when the mod stops.
