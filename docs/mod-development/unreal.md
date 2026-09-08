# Unreal API

Hi-Fi RUSH uses a modified Unreal Engine 4.27 build. Public UE 4.27 source is useful for understanding types, but Overdub's layouts and addresses are specific to the supported game executable.

The C API is declared in [`unreal.h`](../../include/unreal.h), [`unreal_prop.h`](../../include/unreal_prop.h), and [`unreal_reflect.h`](../../include/unreal_reflect.h). Lua uses the same low-level property codec through `overdub.unreal`.

## Objects and names

A UObject has a class, an outer, a name, flags, and an entry in the global object array. The outer chain describes containment. It is not the class inheritance chain.

Object paths and full names are different strings:

```
/Script/Engine.Actor
Class /Script/Engine.Actor
```

Use a path lookup when you already have the expected class. Use a full-name lookup when the type prefix is part of the identifier.

```c
uclass_t  *actor_class = unreal_uobject_find_class_by_full_name(STR_LIT("Class /Script/Engine.Actor"));
uobject_t *actor       = unreal_uobject_find_by_path_name(actor_class, STR_LIT("/Game/Maps/Test.Test:PersistentLevel.MyActor"));
```

`unreal_uobject_find_by_path_name` follows the path spelling produced by `unreal_uobject_push_ue_path_name`. A colon separates a package object path from subobjects such as a persistent level actor.

## Object lifetime

Treat `uobject_t *` as a borrowed pointer. An actor or asset can disappear during destruction, garbage collection, level streaming, or travel. An object-array slot can later hold another object.

```c
if (target && !unreal_uobject_is_valid(target)) {
  target = NULL;
}
```

Validation only describes that moment. When a mod needs to keep track of a particular runtime object, search once for existing instances and maintain the pointer with create and delete listeners.

```c
static uclass_t  *g_wanted_class;
static uobject_t *g_current_object;

static void
on_object_created(uobject_t *object, int32_t index, void *user)
{
  (void)index;
  (void)user;

  if (unreal_uobject_is_a(object, g_wanted_class)) {
    g_current_object = object;
  }
}

static void
on_object_deleted(uobject_t *object, int32_t index, void *user)
{
  (void)index;
  (void)user;

  if (object == g_current_object) {
    g_current_object = NULL;
  }
}
```

Register those callbacks with `mod_register_uobject_listener`. Registrations made through the mod handle are removed automatically after `deinit`.

## Properties

Find a property through its owning class or struct, then use the property codec to locate and read its value.

```c
fprop_t *prop  = unreal_ustruct_find_prop((ustruct_t *)object->cls, STR_LIT("Count"));
void    *value = unreal_fprop_value_in_container(prop, object, 0);

unreal_prop_integer_t count = {0};
if (prop && value && unreal_fprop_read_integer(prop, value, &count)) {
  count.value += 1;
  unreal_fprop_write_integer(prop, value, count);
}
```

Do not assign property storage with `memcpy` unless the property is known to be plain data. Strings, text, structs, object references, arrays, sets, and maps need Unreal's initialization, copy, and destruction rules. `unreal_prop.h` exposes those operations in one place.

The property codec supports scalar numbers, booleans, names, strings, text, object and class references, soft references, weak and lazy references, interfaces, structs, arrays, sets, maps, enums, and delegate inspection or binding. Custom class definitions currently support all of those except delegate property declarations.

Array, set, and map element types are recursive descriptions. Their children are not limited to scalar properties. A supported struct, enum, reference, or another supported container can be used when Unreal allows the resulting layout and hashing rules.

## UFunctions

Find a function on its owning `UStruct`. Build a parameter buffer using the function's reflected layout, initialize every property, write inputs through the codec, call the function, read outputs, and destroy every initialized value.

`unreal_process_event_observed` uses the virtual `ProcessEvent` entry so Overdub hooks can observe the call. `unreal_process_event` calls the captured engine implementation directly. Prefer the observed form for ordinary mod behavior.

Never retain a parameter buffer, `FFrame`, output pointer, or reflected temporary after the call returns.

## Object construction and actors

Use `unreal_construct_object` for a non-actor UObject and give it a suitable outer. `unreal_get_transient_package` is useful for temporary objects.

```c
uobject_t *object = unreal_construct_object(custom_class, unreal_get_transient_package());
```

Actors must be spawned through the world path instead of plain object construction:

```c
uobject_t *actor = unreal_spawn_actor(world_context, actor_class);
```

The world context can be null when Overdub can resolve the current world. An explicit valid context is clearer when the mod already has one.

## Custom classes from C

`mod_define_class` creates a process-lifetime class under a name scoped to the mod ID. The class inherits its parent's native constructor and virtual table. It can add reflected properties and native-backed UFunctions. It cannot add new C++ virtual methods.

```c
static uclass_t *g_inventory_class;

static void MOD_CALL
inventory_add(unreal_func_call_t *call)
{
  fprop_t *amount_prop = unreal_ustruct_find_prop((ustruct_t *)call->function, STR_LIT("Amount"));
  fprop_t *count_prop  = unreal_ustruct_find_prop((ustruct_t *)call->object->cls, STR_LIT("Count"));
  fprop_t *return_prop = unreal_ustruct_find_prop((ustruct_t *)call->function, STR_LIT("ReturnValue"));

  void *amount_value = unreal_fprop_value_in_container(amount_prop, call->params, 0);
  void *count_value  = unreal_fprop_value_in_container(count_prop, call->object, 0);

  unreal_prop_integer_t amount = {0};
  unreal_prop_integer_t count  = {0};

  bool ready = unreal_fprop_read_integer(amount_prop, amount_value, &amount);
  ready = ready && unreal_fprop_read_integer(count_prop, count_value, &count);

  if (ready) {
    count.value += amount.value;
    unreal_fprop_write_integer(count_prop, count_value, count);
    unreal_fprop_write_integer(return_prop, call->return_value, count);
  }
}

static bool
define_inventory_class(mod_handle_t mod)
{
  uclass_t *object_class = unreal_uobject_find_class_by_full_name(STR_LIT("Class /Script/CoreUObject.Object"));
  uclass_t *actor_class  = unreal_uobject_find_class_by_full_name(STR_LIT("Class /Script/Engine.Actor"));

  str_t tag_defaults[] = {
    STR_LIT("Default"),
    STR_LIT("Inventory"),
  };

  unreal_prop_def_t props[] = {
    {
      .name                = STR_LIT("Count"),
      .type                = UNREAL_PROP_KIND_INT32,
      .integer.default_val = 10,
    },
    {
      .name = STR_LIT("Owner"),
      .type = UNREAL_PROP_KIND_OBJECT,
      .cls  = actor_class,
    },
    {
      .name                    = STR_LIT("Tags"),
      .type                    = UNREAL_PROP_KIND_ARRAY,
      .inner.type              = UNREAL_PROP_KIND_NAME,
      .array.default_val.items = tag_defaults,
      .array.default_val.count = COUNTOF(tag_defaults),
    },
  };

  unreal_prop_def_t add_params[] = {
    {
      .name = STR_LIT("Amount"),
      .type = UNREAL_PROP_KIND_INT32,
    },
  };

  unreal_func_def_t funcs[] = {
    {
      .name       = STR_LIT("Add"),
      .params     = add_params,
      .num_params = COUNTOF(add_params),
      .ret.type   = UNREAL_PROP_KIND_INT32,
      .impl       = inventory_add,
    },
  };

  unreal_class_def_t def = {
    .struct_size = sizeof(def),
    .parent      = object_class,
    .props       = props,
    .num_props   = COUNTOF(props),
    .funcs       = funcs,
    .num_funcs   = COUNTOF(funcs),
  };

  g_inventory_class = mod_define_class(mod, STR_LIT("InventoryState"), &def);
  return g_inventory_class != NULL;
}
```

The definition is copied during the call, including property defaults. The class and its layout remain registered until process exit. When a mod stops, Overdub disables its function implementations but does not destroy the class. Starting the same mod again with the same definition reuses the class and enables the new implementations.

Static UFunctions are rejected. A custom implementation receives the target object, UFunction, parameter storage, return-value storage, and its `user` pointer through `unreal_func_call_t`.

Class default values are written to the new class default object. New instances receive independent copies, so dynamic containers do not share their backing storage with the CDO.

The supported custom property definitions are bool, signed and unsigned integers, float, double, name, string, text, object, class, soft object, soft class, weak object, lazy object, interface, struct, enum, array, set, and map. A set element or map key must have a usable Unreal hash function.

## Rooting

Rooting prevents Unreal garbage collection from removing an object. Use it only when the mod truly owns a long-lived object.

Overdub tracks root claims across mods. The first Overdub claim roots the object, and the last claim removes the root unless the object was already rooted outside Overdub. This prevents one mod from undoing another mod's claim.

Rooting does not make a destroyed actor usable and does not replace validity checks.

## PostLoad timing

Calling `PostLoad` yourself is not a general notification mechanism. A mod that must change an object while the engine is processing its load should hook the object's real `PostLoad` call and run before or after it as required.

This timing matters for data such as a UDataTable that another system consumes immediately after load. Changing the table later may not update the derived state that the game already built from it.

Native mods can use a version-specific native hook when no higher-level callback exists. Native Overdub Lua provides `HookPostLoad`; see [Lua Unreal](../lua/unreal.md).

## Debugging reflection

Use UObject Search to verify names, paths, classes, properties, and functions before hard-coding them. UFunction Tracer can reveal which reflected calls occur during an action. Both tools operate on the live game and are more reliable than guessing from generated names alone.
