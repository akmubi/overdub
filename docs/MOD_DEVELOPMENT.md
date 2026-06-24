# Overdub Mod Development Guide

You do not need to have read Unreal Engine source code before starting this guide. To read code examples you need some basic **C/C++** knowledge.

The guide covers:
- main parts of an Unreal Engine game
- the game loop
- the game thread and render thread
- worlds, levels, actors, and components
- UObject system and reflection
- names, classes, properties, and functions
- assets and Blueprints
- how Overdub finds and hooks engine functions
- how the mod manager starts and stops mods
- how to package asset, Blueprint, and DLL mods
- how to write a native mod
- memory, object lifetime, cleanup, and DLL reload

The current mod SDK files are in the [`mod/`](../mod/) directory.

Important files:
```
mod
|--main.c
|--mod_api.h
|--mod_sdk.h
|--mod_sdk.c
|--mod_unreal.h
|--mod_unreal.c
|--mod_nuklear.h
|--mod_nuklear.c
+--mod.vcxproj
```

The most important loader files are:
```
src
|--main.c
|--signatures.c
|--mod_manager.c
|--mod_host.c
+--nk_d3d12.c
```

## 1. Unreal Engine Fundamentals

### Unreal Engine

Hi-Fi RUSH uses a modified build of Unreal Engine 4.27. The public UE 4.27 source code is very useful for understanding engine behavior, but the shipping game contains game-specific changes, compiler optimizations, and modified engine code. Always verify addresses, layouts, and assumptions against the actual game executable.

An Unreal Engine game has two main parts:
1. Unreal Engine itself
2. The game-specific code and content built on top of it

Unreal Engine provides:
- object management
- worlds and levels
- actors and components
- input
- rendering
- audio
- animation
- physics
- asset loading
- Blueprints
- etc.

Hi-Fi RUSH adds its own classes, assets, gameplay rules, UI, music systems, combat systems, and other game-specific code.

A mod can change either side.

Examples:
- replacing a texture -> changes cooked game content
- spawning a Blueprint actor -> adds new Unreal gameplay logic
- hooking a native function -> changes compiled game or engine code
- editing a UObject property -> changes live runtime data
- intercepting `ProcessEvent` -> changes reflected function calls

#### Simple mental model

The game can be thought of as a large machine:
```
Windows starts the process
|
v
Unreal Engine starts
|
+-- systems are loaded
+-- the object system is created
+-- assets are loaded
+-- the first world is created
+-- input and rendering are started
|
v
the game begins running frames
```

Overdub is loaded into the same process. Its code becomes part of the game process. This means a native mod can directly access the game's memory, but it also means a bad address can crash the whole game.

### The Game Loop

A game runs in a loop. A simple game loop looks like this:
```c
while (is_game_running) {
  read_input();
  update_game();
  render_frame();
}
```

Unreal Engine is much more complicated, but the same idea still applies.

A simplified Unreal Engine game loop looks like this:
```
read controller and Windows input
|
v
update the engine (engine tick)
|
+-- update worlds
+-- update actors
+-- update components
+-- run Blueprint code
+-- process timers
+-- update animation
+-- update audio
+-- prepare rendering work
|
v
render the frame
|
v
present the final image
```

Each frame, the engine calculates a value called "delta time". Delta time tells the engine how much time passed since the previous frame. Example:
```
60 FPS  -> ~0.0167 seconds per frame
120 FPS -> ~0.0083 seconds per frame
```

This value is used for movement, physics, animation, and other time-based updates:
```c
position += speed * delta;
```

In this example, without delta time, movement speed would depend on frame rate.

#### Where Overdub Runs

Overdub hooks the engine tick function, which gives it a place to run once per game frame. It exposes this through the mod tick callback:
```c
void tick(mod_t mod, float delta);
```

Overdub allows mods to run inside engine tick as well using a tick callback. Inside a tick callback mods can:
- find new game objects
- react to level changes
- change gameplay values
- update mod state
- run work that **must** happen on the game thread

A tick callback should be fast. Slow work inside it slows down the whole game.

### Threads

A thread is one path of code execution inside the process. Modern games use many threads. In Unreal Engine, these threads can be used for:
- gameplay
- rendering
- asset loading
- audio
- physics
- file IO
- GPU command submission
- shader work
- etc.

For modding, the most important thread is the **game thread**.

#### The Game Thread

The game thread controls most high-level gameplay state. It normally owns or manages:
- world (`UWorld`)
- actors
- components
- player controllers
- pawns
- HUD objects
- widgets
- most Blueprint execution
- most reflected function calls
- most UObject creation and destruction
- etc.

Overdub records the game thread during the first hooked engine tick.

The following Overdub callbacks run on the game thread:

- mod initialization
- mod tick
- most input callbacks
- mod manager UI
- mod custom UI
- game-thread `ProcessEvent` callbacks
- game-thread `UFunction::Invoke` callbacks

This is important because most Unreal gameplay APIs should be used on the game thread.

#### The Render Thread

The render thread prepares rendering work. It does not normally own gameplay objects.

A mod should not change UObject properties or actors from a render thread unless the engine API is known to allow it.

Because of that Overdub separates *UI creation* from *UI rendering*.
```
game thread:
  build UI
  convert it into draw data
  publish a render snapshot

render thread:
  read the latest render snapshot
  draw it over the game
```

This means mod UI code runs on the game thread, not inside the thread that calls the D3D12 `Present` hook.

#### Worker Threads

A mod may create worker threads for slow work. Good worker-thread tasks:
- reading files
- parsing large data
- network requests
- compression
- long searches over copied data

Overdub does not provide worker threads for mods. A mod that creates its own worker thread is responsible for synchronization and cleanup. Overdub itself also runs code from engine, input, and rendering hooks on the threads that call those functions.

#### Hooks Can Run on Any Thread

A custom native hook runs on the thread that called the hooked function. Overdub hook API does not move the call to the game thread. Before writing a hook, you should find out:
- which thread calls the function
- whether several threads can call it at once
- whether it can call itself again
- whether it can run while the game is shutting down
- whether the original function must be called immediately

You can often find answers to these questions by reading Unreal Engine 4.27 source code, but remember that Hi-Fi RUSH uses a modified build.

### Worlds, Levels, Actors, and Components

These are the main building blocks of the gameplay.

#### UWorld

A `UWorld` represents a running world. It owns or connects systems such as:
- loaded levels
- actors
- game state
- timers
- physics scenes
- player controllers
- world settings
- etc.

A game can replace its current world during:
- loading screens
- map changes
- level transitions
- restarting from checkpoints
- etc.

**A pointer to the old world must not be treated as permanent.**

#### Levels

A world can contain one or more levels. The persistent level is the main level. Other levels can be streamed in and out. Example:
```
World
+-- PersistentLevel
|   |-- Player actor
|   |-- Camera actor
|   +-- Game manager actor
|
|-- StreamedLevel_A
+-- StreamedLevel_B
```

An actor may disappear because its level was unloaded even when the game is still running.

#### Actors

An actor is a UObject that can exist in a world. Examples:
- player character
- enemy
- camera
- trigger
- item
- level script actor
- manager actor
- etc.

Actors have a gameplay lifetime. A rough actor lifecycle looks like this:
```
class is loaded
|
v
actor is created
|
v
actor is initialized
|
v
BeginPlay
|
v
actor updates and receives events
|
v
EndPlay
|
v
actor is destroyed
```

The exact native lifecycle has more steps, but this is a useful starting model.

#### Components

Components add behavior or data to actors. Examples:
- scene component
- mesh component
- audio component
- movement component
- collision component
- camera component
- etc.

A simple actor tree might look like this:
```
PlayerActor
|-- RootSceneComponent
|-- SkeletalMeshComponent
|-- MovementComponent
|-- AudioComponent
+-- CameraComponent
```

Some components are attached in a scene tree. Other components are only logical helpers.

#### Common Gameplay Objects

These objects are often useful to mods:
```
GameInstance     - lives across world changes
World            - current gameplay world
PlayerController - controls a player
Pawn             - object controlled by a controller
HUD              - player HUD logic
Actor            - world object
ActorComponent   - reusable actor behavior
Widget           - Unreal UI object
```

**Do not assume all of them exist at startup**. For example, the player controller may not exist yet while the title screen or loading screen is active.

### UObject

`UObject` is the base of Unreal's reflected object system. Many important Unreal types are UObjects:
- classes
- packages
- functions
- enums
- actors
- components
- worlds
- widgets
- data assets
- data tables
- **Blueprint-generated objects**

Not every native Unreal type is a UObject. Low-level renderer and platform objects often are not.

#### Basic layout

The current SDK exposes the following layout:
```c
struct uobject_s {
  void        *vftable;
  eobj_flags_t obj_flags;
  int32_t      internal_idx;
  uclass_t    *cls;
  fname_t      name;
  uobject_t   *outer;
};
```

The important fields are:
| Field          | Meaning                              |
|----------------|--------------------------------------|
| `vftable`      | Native virtual function table        |
| `obj_flags`    | Object state flags                   |
| `internal_idx` | Position in the global UObject array |
| `cls`          | Runtime class of this object         |
| `name`         | Object name stored as an `FName`     |
| `outer`        | Object that contains this object     |

#### Example UObject

Imagine an actor called `Enemy_12`.
```
object pointer -> Enemy_12 instance
class          -> BP_Enemy_C
name           -> Enemy_12
outer          -> CurrentLevel
```

The *class* says what kind of object it is.
The *name* says what this instance is called.
The *outer* says where it belongs.

#### UObject is not the same as Actor

Every actor is a UObject. **Not every UObject is an actor**. Examples of UObjects that are not actors:
- class objects
- functions
- packages
- data assets
- textures
- widgets
- animation objects

### The Global UObject Array

Unreal keeps live UObjects in **a global array**. Overdub exposes this array through the SDK. A simplified picture:
```
Global UObject Array

[0]    Package /Script/CoreUObject
[1]    Class /Script/CoreUObject.Object
[2]    Class /Script/Engine.Actor
...
[5000] PlayerController instance
[5001] Player pawn
[5002] Enemy actor
...
```

The array is useful for:
- finding loaded classes
- finding live object instances
- inspecting game systems
- watching new objects
- watching object destruction
- validating object pointers

#### Object Slots Can Be Reused

An object can be destroyed and removed from the array. Later, another object can use the same slot. This means a cached pointer or index is not automatically safe forever. Example of a bad assumption:
```c
static uobject_t *player;

player = find_player();

/* many seconds later ... */

player->something = value;
```

The player may have been destroyed during a level change. Much safer pattern:
```c
if (!player || !unreal_uobject_is_valid(player)) {
  player = find_player();
}

if (player) {
  /* use it now */
}
```

Even validation only proves that the object is valid at that moment.

### Object Lifetime and Garbage Collection

Unreal uses garbage collection for many UObjects. A raw pointer stored by a DLL does not keep an object alive. The engine may destroy an object because:
- its world changed
- its level unloaded
- gameplay destroyed it
- no Unreal reference keeps it alive
- the game is shutting down
- garbage collection removed it
- etc.

#### Safe Mental Model

Treat a `UObject *` like **a borrowed pointer**.
```
find object
|
v
validate object
|
v
use object for a short time
|
v
do not assume it remains valid later
```

#### Better Ways to Track Objects

A mod can:
- search for the object when needed
- validate a cached pointer before use
- listen for object creation/deletion
- detect world changes
- clear cached pointers during `deinit`

#### UObject Listeners

Overdub lets a mod register *create and delete listeners*. Example:
```c
static uobject_t *g_player = NULL;

static void
on_object_created(uobject_t *obj, int32_t idx, void *user)
{
  if (has_name(obj, "MainPlayer")) {
    g_player = obj;
  }
}

static void
on_object_deleted(uobject_t *obj, int32_t idx, void *user)
{
  if (obj == g_player) {
    g_player = NULL;
  }
}
```

Listeners should be fast. Unreal may create or destroy many objects.

### FName

Unreal uses `FName` for many names/identifiers. Examples:
- object names
- function names
- property names
- class names
- gameplay tags
- socket names

An `FName` is not a normal string pointer. The current target uses:
```c
struct fname_s {
  uint32_t cmp_idx; // comparison index into the global name pool
  uint32_t num;     // internal instance number
};
```

`num` uses Unreal's internal representation:

```
internal num = 0  -> no suffix
internal num = 1  -> _0
internal num = 2  -> _1
internal num = 12 -> _11
```

For numbered names, the displayed suffix is `num - 1`.

Example:

```
Base name:    Enemy
Internal num: 12
Displayed:    Enemy_11
```

#### Why Unreal Uses FName

Names are used constantly. Comparing two integer IDs is much faster than comparing two full strings every time. Instead of:
```c
strcmp(a, b);
```

Unreal can often compare:
```c
a.cmp_idx == b.cmp_idx
```

#### Finding and Creating Names

Use `FNAME_FIND` when you only want to find an existing name. Creating new names changes the global name pool, so it should only be done when needed and when the engine allows it.

### Outer and Full Names

Every UObject has an `outer`. The outer describes *containment*. Example:

```
Package /Script/Engine
+-- Class Actor
    +-- Function K2_DestroyActor
```

The class is inside the package. The function is inside the class.

#### Outer Is Not Inheritance

These are different relationships:
```
outer chain:      where the object belongs
class:            what type the object is
super class:      what class it inherits from
actor attachment: what scene object it is attached to
```

**Do not use the outer chain as a class hierarchy**.

#### Short Names Can Repeat

Two objects can have the same short name. Example:

```
PackageA.SomeObject
PackageB.SomeObject
```

Searching only for `SomeObject` may return the wrong one. A full name includes more context. Examples:

```
Class /Script/Engine.Actor
Function /Script/Engine.Actor.K2_DestroyActor
```

Use full names when you know the exact object.

### UClass and Inheritance

Every UObject has a class. The class is represented by `UClass`. Example:
```
EnemyActor instance
|
v
BP_Enemy_C class
|
v
EnemyBase class
|
v
Character class
|
v
Pawn class
|
v
Actor class
|
v
Object class
```

This is the inheritance chain.

#### Checking a Type

Comparing only the direct class pointer answers: *Is this object exactly this class?*
Walking the inheritance chain answers: *Is this object this class or a child class?*
Example:
```c
if (unreal_uobject_is_a(obj, actor_class)) {
  /* obj is Actor or a child of Actor */
}
```

#### The Class Default Object

Each class has a class default object, usually called the **CDO**. The CDO stores *default values* for the class. Example:
```
BP_Enemy_C
+-- Default__BP_Enemy_C
```

New instances often copy initial values from the CDO. Changing the CDO may affect future instances, but it does not guarantee that existing instances change. CDO changes are global and should be made carefully.

### Reflection

Reflection is the system that lets Unreal describe its own classes, properties, and functions at runtime. Without reflection, native code only knows data through compiled C++ types and offsets.

With reflection, code can ask:
- *what class is this object?*
- *what properties does the class have?*
- *what functions does it expose?*
- *where is this property stored?*
- *how large is the function parameter buffer?*

Reflection powers many Unreal systems:
- Blueprints
- serialization
- editor property panels
- networking
- garbage collection
- reflected function calls
- save-game data

Overdub uses reflection for UObject Search, property inspection, function lookup, Blueprint spawning, and function calls.

### Properties

A reflected property is described by an `FProperty`. The property does not normally contain the value itself. It tells you where the value is inside an object or parameter buffer.

#### Simple Mental Model

Imagine this object memory:
```
Player object
+0x000 UObject base
+0x100 Health
+0x104 MaxHealth
+0x108 IsInvincible
```

Reflection may describe:
```
Health:
  type   = FloatProperty
  offset = 0x100

MaxHealth:
  type   = FloatProperty
  offset = 0x104

IsInvincible:
  type   = BoolProperty
  offset = 0x108
```

The address of a normal property is:
```c
void *address = (uint8_t *)object + property->offset_internal;
```

The SDK helper does this:
```c
void *address = unreal_uprop_ptr(property, object);
```

#### Property Type Matters

A property may be:
- bool
- byte
- integer
- float
- double
- name
- string
- text
- object reference
- class reference
- struct
- array
- map
- set
- enum
- delegate
- etc.

**Do not treat all properties as the same size**. For example:
```c
*(uint64_t *)address = value;
```

That can corrupt memory when the property is smaller or uses a special layout.

#### Boolean Properties

Unreal booleans may share bits inside one byte. Example:
```
byte:
bit 0 = IsVisible
bit 1 = IsEnabled
bit 2 = IsLocked
```

The property contains masks that describe which bit belongs to it. Use the bool helpers instead of writing the whole byte.

#### Complex Properties

Some types own memory or require constructors and destructors:
- `FString`
- `FText`
- `TArray`
- `TMap`
- `TSet`
- delegates
- non-trivial structs

Copying raw bytes may leak memory, double-free memory, or leave invalid pointers. Start with plain properties such as integers, floats, names, and object pointers before working with complex types.

### UFunction

A reflected function is represented by `UFunction`. A UFunction stores information such as:
- function flags
- parameter count
- total parameter-buffer size
- return-value offset
- reflected parameter properties
- native function pointer or Blueprint bytecode

#### Function Parameter Buffer

Unreal reflected calls normally use one block of memory for all parameters. Example function:
```
SetHealth(float NewHealth, bool ClampValue) -> bool
```

The parameter buffer might look like:
```
+0x00 NewHealth
+0x04 ClampValue
+0x05 padding
+0x08 ReturnValue
```

The real offsets come from reflection. **Do not guess them from the function declaration**.

#### ProcessEvent

`ProcessEvent` is Unreal's main reflected function dispatcher. Conceptually:
```c
object->ProcessEvent(function, params);
```

The SDK exposes:
```c
unreal_process_event(object, function, params);
```

A typical call has these steps:
```
find target object
|
v
find target UFunction
|
v
allocate params_size bytes
|
v
fill parameters using reflected offsets
|
v
call ProcessEvent
|
v
read output and return properties
```

#### Example

This example only works when the parameter is really a plain `int32_t`. Error handling is omitted:
```c
static void
call_example(uobject_t *obj)
{
  if (!obj || !unreal_uobject_is_valid(obj)) {
    return;
  }

  /* find function named "SetCount" */
  ufunc_t *func = unreal_ustruct_find_func((ustruct_t *)obj->cls, STR_LIT("SetCount"));

  /* create an arena for temporary allocations */
  tmp_arena_t tmp = mod_scratch_begin(0);
  {
    /* allocate space for function parameters (zeroed out) */
    uint8_t *params = MOD_ARENA_PUSH_ARRAY_ZERO(tmp.arena, uint8_t, func->params_size);

    /* find function parameter called "Count" */
    fprop_t *count_prop = unreal_ustruct_find_prop((ustruct_t *)func, STR_LIT("Count"));

    /* get the "Count" parameter location and set its value to 5 */
    int32_t *count_ptr = (int32_t *)unreal_uprop_ptr(count_prop, params);
    *count_ptr = 5;

    /* call the function with provided parameters */
    unreal_process_event(obj, func, params);
  }
  mod_scratch_end(tmp);
}
```

For strings, arrays, structs, and other complex values, the real Unreal construction rules must be followed.

### ProcessEvent Hooks

Overdub hooks `ProcessEvent`. A native mod can receive callbacks before and after reflected calls.

```c
bool pe_pre(mod_t mod, uobject_t *obj, ufunc_t *func, void *params);
void pe_post(mod_t mod, uobject_t *obj, ufunc_t *func, void *params, bool consumed);
```

#### Normal Flow

```
mod pre callbacks
|
v
original ProcessEvent
|
v
mod post callbacks
```

#### Consumed Flow

A pre callback can return `true`. If any active mod returns `true`, Overdub skips the original call.
```
mod pre callbacks
|
+-- one returned true
|
v
skip original ProcessEvent
|
v
mod post callbacks receive consumed = true
```

All pre callbacks still run. Overdub **combines their results**.

#### Why This Is Can be Dangerous

Skipping the original call may skip:
- gameplay logic
- Blueprint events
- return-value creation
- output parameters
- internal state changes
- delegate broadcasts

#### ProcessEvent Can Recurse

A `ProcessEvent` callback may call another reflected function. That can enter the callback again. Example:
```
pe_pre for FunctionA
  |
  +-- mod calls ProcessEvent for FunctionB
        |
        +-- pe_pre for FunctionB
```

A hook may need a recursion guard.

### UFunction::Invoke and Blueprint Execution

`UFunction::Invoke` is a lower-level execution path. It works with `FFrame`, which represents the current script execution frame. An `FFrame` contains information such as:
- current function
- current object
- bytecode position
- local variables
- output parameters
- previous frame

Overdub exposes pre and post callbacks for this path.
```c
bool func_invoke_pre(mod_t mod, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result);
```

This is more fragile than `ProcessEvent`. The stack and result pointers belong to the current call. Never save them for later. Use this hook only when `ProcessEvent` is not enough.

**Skipping it most of the time makes the game crash or freeze**.

### Assets and Packages

Unreal stores cooked content in packages. Examples of content:
- textures
- materials
- sounds
- meshes
- animations
- Blueprints
- data tables
- data assets
- maps
- etc.

A package has an Unreal path. Example:
```
/Game/Mods/Example/BP_Example
```

This is not a Windows file path.

Package types:
- PAK
- IoStore

A `.pak` file stores packaged game files. IoStore normally uses:
```
example.utoc
example.ucas
```

The `.utoc` contains the table of contents. The `.ucas` contains package data.

#### Mounting

Mounting tells Unreal that another package container exists. After mounting, Unreal can load content from it. Mounting does not automatically run a Blueprint or create an actor.

#### Replacement Mods (dummying)

An asset replacement uses the same package path as an original game asset. Example:
```
original:
  /Game/UI/Textures/T_Logo

mod:
  /Game/UI/Textures/T_Logo
```

**The package with higher mount priority normally wins**.

#### New Content Mods

A mod can also add new content under its own path. Example:
```
/Game/Mods/Example/BP_ExampleMod
```

Native code or another Blueprint must load or use it.

### Blueprints

Blueprints are Unreal classes and scripts created in the Unreal Editor. When cooked, a Blueprint normally creates a generated class ending in `_C`. Example asset:
```
/Game/Mods/Example/BP_ExampleMod
```

Generated class:
```
/Game/Mods/Example/BP_ExampleMod.BP_ExampleMod_C
```

The `_C` class path is normally used when spawning the Blueprint actor.

#### Blueprint Actor Mental Model

```
Blueprint asset
|
v
generated UClass
|
v
spawn actor instance
|
v
BeginPlay and Blueprint events
```

#### Overdub Blueprint Mods

Overdub can:
- load the generated class
- find a world context
- spawn the actor
- track the actor
- destroy the actor
- expose spawn settings in the manager

Possible spawn contexts:
- player_controller
- local_player
- pawn
- hud
- world
- game_instance
- custom_class_path

The context must exist when spawning happens. For example, the pawn may not exist on the title screen.

#### Travel and Destruction

A spawned Blueprint actor may be destroyed during level travel. Overdub can notice that its stored pointer is no longer valid, but a persistent mod may need its own respawn logic.

## 2. How Overdub Connects to the Game

### Types of Overdub Mods

A mod package can contain several parts.

#### Asset Mod

Best for:
- replacing textures
- replacing music or sound
- replacing materials
- changing cooked data
- adding cooked content

Asset mods are mounted during startup. They normally need a restart after:
- updating files
- disabling the mod
- removing the mod
- changing order

#### Blueprint Actor Mod

Best for:
- gameplay logic made in Unreal Editor
- event-driven logic
- using existing Blueprint nodes
- features that do not need native hooks

**It requires cooked asset files**.

#### Native DLL Mod

Best for:
- native hooks
- reflection
- custom input
- custom UI
- external libraries
- direct memory work
- features that cannot be made with assets or Blueprints alone

**Native mods are the most powerful and the easiest to crash**.

#### Mixed Mod

A mod can contain:
- assets
- one or more Blueprint actors
- native DLL code

Example:
```
example-mod/
|-- assets/
|   |-- example.pak
|   |-- example.utoc
|   +-- example.ucas
|-- example.dll
+-- mod.ini
```

### What a Hook Is

A hook changes the path of a function call so custom code runs before, after, or instead of the original function. A normal call looks like this:
```
caller
|
v
original function
|
v
return to caller
```

After installing a hook, the call is redirected to a detour:
```
caller
|
v
detour function
|
+-- inspect or change parameters
+-- run custom code before the original
|
v
original function through a trampoline
|
+-- run custom code after the original
|
v
return to caller
```

The main terms are:
- **target** - the original function being hooked
- **detour** - the replacement function written by the loader or mod
- **trampoline** - a callable path to the original function
- **original function pointer** - the pointer a detour uses to call the original behavior

A detour does not always have to call the original function. It may completely replace it, but skipping the original is dangerous when the caller expects side effects, output parameters, or a return value.

Hooks are useful because a shipping game does not provide callbacks for every internal event. Overdub uses hooks to observe the engine tick, input, package mounting, reflected function calls, and rendering.

A hook must exactly match the original function's calling convention, parameters, and return type. A wrong declaration can corrupt registers or the stack and crash the game.

Hooks can also introduce:
- recursion when the detour reaches the same hooked function again
- thread-safety problems
- conflicts between mods
- crashes during DLL unload
- version-specific failures after a game update

The later [Custom Native Hooks](#custom-native-hooks) section explains how a mod installs its own managed hooks through the SDK.

### How Overdub Enters the Game

Overdub is loaded by Hibiki Bootstrap. The bootstrap is a proxy DLL placed beside the game executable. Windows loads the proxy DLL because the game expects a system DLL with that name. The proxy forwards the original system DLL functions and loads Overdub.

Simplified flow:
```
game starts
|
v
Windows loads proxy DLL
|
v
proxy forwards original DLL functions
|
v
proxy loads overdub.dll
|
v
Overdub initializes inside the game process
```

The bootstrap only loads Overdub. Overdub itself handles:
- logging
- signature scanning
- engine hooks
- input hooks
- renderer hooks
- mod discovery
- mod lifecycle
- UI
- asset mounting
- Blueprint spawning

### Signature Scanning

The game executable does not provide a stable public API for all engine internals. Overdub must find functions and global variables inside compiled machine code. It uses:
1. known RVAs for supported game versions
2. byte-pattern scanning when needed

#### RVA

An RVA is *an offset from the module base*. Example:
```
game module base: 0x00007FF700000000
function RVA:     0x0000000001234000
function address: 0x00007FF701234000
```

An RVA is fast, **but it usually changes when the executable changes**.

#### Signature

A signature is *a byte pattern*. Example:
```
48 89 5C 24 ?? 57 48 83 EC ??
```

`??` means any byte.

**A good signature matches one intended function**.

#### Why Both Are Used

Known RVAs are fast and exact for known builds. Signatures can still find a function after smaller executable changes. Neither method guarantees that every structure offset and function parameter is still correct.

A game update can change:
- code addresses
- instruction patterns
- structure layouts
- vtable indexes
- function parameters
- calling conventions

### Overdub Engine Hooks

Overdub currently hooks or patches several engine systems.

1. Engine Tick
- detect the game thread
- finish Unreal-dependent initialization
- start native mods
- start Blueprint mods
- update Overdub once per frame
- dispatch mod tick callbacks
- build UI

2. ProcessEvent
- observe reflected calls
- let mods change parameters
- let mods replace calls
- run post-call callbacks

3. UFunction::Invoke
- observe lower-level function execution
- inspect Blueprint VM frames
- support tracing and advanced hooks

4. Input
Overdub hooks Unreal and Slate input functions for:
- key down
- key up
- character input
- mouse movement
- mouse buttons
- mouse wheel
- analog input
- application activation

These events are passed to the UI and active mods.

5. Package Mounting
Overdub hooks package mounting so it can capture the live PAK and IoStore systems and mount mod assets at the correct time.

6. Rendering
Overdub hooks:
- D3D12 viewport initialization
- DXGI `Present`
- DXGI `ResizeBuffers`

It uses them to draw the manager and tools over the game.

7. Cursor Functions
Overdub also hooks cursor functions so the mouse cursor can remain visible while its UI is open.

### Overdub Startup Timeline

A simple startup timeline looks like this:
1. bootstrap loads `overdub.dll`
2. Overdub initializes logging and memory
3. Overdub scans engine signatures
4. Overdub installs engine hooks
5. Overdub loads its main configuration
6. Overdub discovers mod directories
7. Overdub parses mod.ini files
8. Overdub loads mod config.ini files
9. Unreal package systems become available
10. Overdub mounts enabled mod assets
11. first game-engine tick happens
12. Overdub records the game thread
13. Overdub collects common Unreal objects
14. Overdub starts enabled DLL mods
15. Overdub starts enabled Blueprint actors
16. normal per-frame mod callbacks begin

The exact engine startup is more complicated, but this order explains why native mods are not initialized directly from `DllMain`.

### Asset Mount Timing

Overdub waits until the game uses its own package mount path. It does not mount asset files directly from DLL process attach. This gives it valid engine objects for:
- PAK mounting
- IoStore mounting

For enabled mods, asset priority is based on runtime mod order. Simplified rule:
```
lower mod in the manager list
|
v
higher numeric asset priority
|
v
normally wins an asset conflict
```

Changing asset order requires a restart because the live mount order is not rebuilt safely.

### First Engine Tick

On the first hooked engine tick, Overdub:
- stores the current thread as the game thread
- finds the Unreal game window
- collects common Unreal classes and functions
- starts enabled native DLL mods
- starts enabled Blueprint entries
- marks the engine as initialized

This is when native `init` callbacks begin. At this point, many core Unreal systems exist, but a gameplay world, pawn, HUD, or player controller may still be missing. A mod should still handle missing runtime objects.

### Per-Frame Overdub Flow

A simplified frame looks like this:
```
Overdub pre-tick
|
+-- initialize engine state on first frame
+-- begin UI frame
+-- update cursor state
+-- call native mod tick callbacks
|
v
original Unreal engine tick
|
v
Overdub post-tick
|
+-- finish manager UI
+-- finish mod UI
+-- update previous key state
+-- convert UI draw commands
+-- publish render snapshot
+-- reset temporary memory
+-- increase frame counter
```

The final overlay is drawn later from the `Present` hook.

### Input Flow

Input is processed in this order:
```
Overdub UI pre-processing
|
+-- consumed?
|
v
native mod input callbacks
|
+-- first true result consumes input
|
v
Overdub UI post-processing
|
+-- consumed?
|
v
original Unreal input function
```

When input is consumed, the original game input handler does not receive it. **A mod should only consume an event when it really wants to block the game action**.

### ProcessEvent Flow in Overdub

For game-thread calls:
```
all active pe_pre callbacks
|
+-- combine true results
|
+-- consumed = false
|   |
|   +-- call original ProcessEvent
|
+-- consumed = true
    |
    +-- skip original ProcessEvent
    |
    v
all active pe_post callbacks
```

Every active pre callback runs, even when an earlier mod already returned `true`. This means two mods can conflict when both replace the same call.

## 3. Mod Manager and Package Format

### Mod Manager Architecture

The mod manager keeps information about every discovered mod. Each mod has two main kinds of data:
1. Manifest data
2. Runtime data

#### Manifest Data

Manifest data comes from `mod.ini`. It includes:
- ID
- name
- author
- version
- kind
- description
- DLL path
- asset paths
- Blueprint definitions
- option definitions

This data is loaded at startup. Reloading a DLL does not reload the manifest.

#### Runtime Data

Runtime data changes while the game runs. It includes:
- enabled state
- DLL loaded state
- DLL active state
- callback table
- registered commands
- managed hooks
- UObject listeners
- arenas
- asset mount state
- asset priority
- Blueprint actor pointers
- option values
- error messages

### Mod Discovery

At startup, the manager does the following:
```
load overdub-config.ini
|
v
scan the mods directory
|
v
find mod.ini in each mod directory
|
v
parse and validate manifests
|
v
apply saved mod order
|
v
apply disabled mod list
|
v
load each mod config.ini
```

The mod list is discovered once. Adding a new mod directory while the game is running does not register it. Restart the game after adding or removing a mod.

### Mod Order

The manager has:
- wanted order
- runtime order

Wanted order is the order saved by the UI. Runtime order is the order used by the current game process. Changing wanted order does not rewrite every live system. Restart the game to apply it. 
Runtime order affects:
- DLL startup
- tick callbacks
- input callbacks
- ProcessEvent callbacks
- UFunction invoke callbacks
- command lookup
- Blueprint startup
- asset priority

### Mod Handles

The public API uses opaque handles. Examples:
```c
typedef uint64_t mod_t;
typedef uint64_t mod_cfg_t;
typedef uint64_t mod_arena_t;
```

Treat them as IDs owned by Overdub. **Do not**:
- cast them to internal pointers
- decode their bits
- store internal manager pointers
- depend on their current representation

The manager uses the mod handle to connect resources to the correct mod.

### Native DLL Lifecycle

A native mod has two separate states:
1. Is the DLL loaded?
2. Is the mod active?

The lifecycle is:
```
UNLOADED
|
| Load
v
LOADED, inactive
|
| Start
v
LOADED, active
|
| Stop
v
LOADED, inactive
|
| Unload
v
UNLOADED
```

#### Load

Overdub:
1. checks the DLL path
2. checks that the file exists
3. calls `LoadLibraryW`
4. finds `mod_entry`
5. calls `mod_entry`
6. reads the callback table
7. checks ABI compatibility

Loading does not call `init`.

#### Start

Overdub:
1. loads the DLL if needed
2. creates managed runtime tables
3. creates the mod permanent arena
4. calls `init`
5. marks the mod active when `init` returns true

An active mod receives callbacks.

#### Stop

Overdub:

1. calls optional `deinit`
2. removes managed hooks
3. removes managed UObject listeners
4. destroys managed arenas
5. removes managed commands
6. clears per-run arena memory
7. marks the mod inactive

The DLL remains loaded.

#### Restart

- Stop
- Start

The DLL is not unloaded. Static DLL variables still exist.

#### Reload

- Stop
- Unload
- Load
- Start

This loads the new DLL file from disk. Reload is useful during development, but it is only safe when cleanup is complete.

### Managed Resources

Overdub tracks some resources created through the mod API. These are cleaned after `deinit`:
- hooks created through `mod_hook_create`
- UObject listeners registered through the SDK
- console commands
- permanent arena memory
- dynamic arenas created through the SDK

The mod should still clean them explicitly when that makes the code clearer.

#### Resources Overdub Cannot Clean

The mod must clean these itself:
- threads
- Win32 handles
- sockets
- files
- COM objects
- D3D12 objects
- `malloc` memory
- `VirtualAlloc` memory
- hooks created directly with another library
- manual code patches
- engine delegates registered directly
- timers created outside the host API
- callbacks stored by another library

A DLL cannot be safely unloaded while another thread or callback can still execute its code.

### Package Layout

Every mod has its own directory
```
mods/
+-- example-mod/
    |-- mod.ini
    |-- config.ini
    |-- example.dll
    |-- example.pak
    |-- example.utoc
    +-- example.ucas
```

Only `mod.ini` is required. A mod can contain only the files it needs. Examples:

Asset-only mod:
```
example-mod/
|-- mod.ini
+-- example.pak
```

DLL-only mod:
```
example-mod/
|-- mod.ini
+-- example.dll
```

Mixed mod:
```
example-mod/
|-- mod.ini
|-- example.dll
|-- example.utoc
+-- example.ucas
```

### mod.ini

A complete example:

```ini
[info]
id = author.example
name = Example Mod
author = Author Name
version = 1.0.0
kind = mod

[description]
Example mod used by this guide.
It contains code, assets, a Blueprint actor, and options.

[code]
path = example.dll

[assets]
path_without_ext = example

[blueprint]
id = example_actor
mod_actor_class_path = /Game/Mods/Example/BP_ExampleMod.BP_ExampleMod_C
attach_to = world
auto_spawn = true
default_spawn_keybind = F6

[option.bool]
id = feature_enabled
label = Enable feature
description = Enables the main feature.
default_value = true

[option.int]
id = count
label = Count
description = Number of items.
default_value = 3
min_value = 0
max_value = 100
step = 1

[option.float]
id = scale
label = Scale
description = Runtime scale value.
default_value = 1.0
min_value = 0.1
max_value = 5.0
step = 0.1

[option.enum]
id = mode
label = Mode
description = Selects the mode.
enum_values = Off|Normal|Aggressive
default_value = Normal

[option.string]
id = label
label = Label
description = Example text value.
default_value = Example

[option.keybind]
id = action_key
label = Action key
description = Runs the example action.
default_value = F7

[option.color]
id = display_color
label = Display color
description = RGBA display color.
default_value = 255.128.32.255
```

Section names and keys are case-insensitive. A semicolon starts a comment.

```ini
; This is a comment.
```

### Manifest Sections

#### `[info]`

```ini
[info]
id = author.example
name = Example Mod
author = Author Name
version = 1.0.0
kind = mod
```

| Key       | Meaning                          |
|-----------|----------------------------------|
| `id`      | Stable unique mod ID. Required.  |
| `name`    | Display name. Required.          |
| `author`  | Author shown in the manager.     |
| `version` | Mod version. `0.0.0` is invalid. |
| `kind`    | `mod` or `tool`.                 |

**Do not change the ID between normal releases**. The ID is used in saved order and configuration.

#### `[description]`

```ini
[description]
First line.
Second line.
```

The full section body becomes the displayed description.

#### `[code]`

```ini
[code]
path = example.dll
```

The path is normally relative to the mod directory. The DLL must be x64.

#### `[assets]`

```ini
[assets]
path_without_ext = example
```

Overdub looks for:
```
example.pak
example.utoc
example.ucas
```

The `.utoc` and `.ucas` files must be used together.

#### `[blueprint]`

A mod can have several Blueprint sections.
```ini
[blueprint]
id = example_actor
mod_actor_class_path = /Game/Mods/Example/BP_ExampleMod.BP_ExampleMod_C
attach_to = world
auto_spawn = true
default_spawn_keybind = F6
```

Accepted `attach_to` values:
- `none`
- `player_controller`
- `local_player`
- `pawn`
- `hud`
- `world`
- `game_instance`
- `custom_class_path`

For a custom class:
```ini
attach_to = custom_class_path
custom_attach_class_path = Class /Script/GameModule.SomeClass
```

#### Option Sections

Supported option types:
```
[option.bool]
[option.int]
[option.float]
[option.enum]
[option.string]
[option.keybind]
[option.color]
```

Common keys:
```
id
label
description
default_value
```

Integer and float options also use:
```
min_value
max_value
step
```

Enum values use `|`:
```ini
enum_values = First|Second|Third
```

Colors use decimal RGBA:
```ini
default_value = 255.128.32.255
```

## 4. Native DLL Development

### Building a Native Mod

The included Visual Studio project uses:

- Visual Studio 2022
- x64
- C11 or C++20
- static MSVC runtime

Build Debug:

```batch
msbuild mod\mod.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:ModName=example
```

Build Release:

```batch
msbuild mod\mod.vcxproj /p:Configuration=Release /p:Platform=x64 /p:ModName=example
```

Output:

```
mod/build/<Configuration>/example.dll
```

A separate mod project should use the public SDK files in `mod/`. Do not depend on private loader structures from `src/` or `include/`.

### Minimal Native Mod

```c
#include "mod_sdk.h"

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  MOD_LOG_INFO("Example mod started");
  return true;
}

static void
example_deinit(mod_t mod)
{
  (void)mod;
  MOD_LOG_INFO("Example mod stopped");
}

MOD_ENTRY()
{
  static const mod_api_t api = {
    .struct_size = sizeof(mod_api_t),
    .abi_version = MOD_ABI_VERSION,
    .init        = example_init,
    .deinit      = example_deinit,
  };

  return &api;
}
```

`mod_entry` only returns the callback table. **Do not perform real initialization inside `mod_entry`**. Overdub calls `init` later when the engine is ready.

### Callback Table

The current callback table contains:
```c
struct mod_api_s {
  uint32_t                  struct_size;
  mod_version_t             abi_version;
  mod_init_fn_t             init;
  mod_deinit_fn_t           deinit;
  mod_tick_fn_t             tick;
  mod_input_fn_t            input;
  mod_pe_pre_fn_t           pe_pre;
  mod_pe_post_fn_t          pe_post;
  mod_func_invoke_pre_fn_t  func_invoke_pre;
  mod_func_invoke_post_fn_t func_invoke_post;
  mod_draw_panel_fn_t       draw_panel;
  mod_draw_config_fn_t      draw_config;
};
```

**`init` is required**. The other callbacks are optional.

#### init

```c
bool init(const mod_host_api_t *host, mod_t mod);
```

Use it to:
- initialize the SDK
- get config handles
- allocate state
- register commands
- register UObject listeners
- scan signatures
- create hooks
- cache classes and functions

Return false when the mod cannot start safely.

#### deinit

```c
void deinit(mod_t mod);
```

Use it to:
- stop worker threads
- join threads
- remove unmanaged callbacks
- restore manual patches
- close handles
- release COM objects
- clear cached pointers
- undo game changes when possible

#### tick

```c
void tick(mod_t mod, float delta);
```

Called once per engine frame while the mod is active. Keep it fast.

#### input

```c
bool input(mod_t mod, input_event_t *event);
```

Return true to consume the event.

#### pe_pre and pe_post

Called around game-thread `ProcessEvent`. Do not keep the parameter pointer after the callback.

#### func_invoke_pre and func_invoke_post

Called around game-thread `UFunction::Invoke`. Do not keep the frame or result pointers.

#### draw_panel

Draws a custom section in the mod manager.

#### draw_config

Draws custom configuration UI.

### ABI Version

The callback table includes:

- `struct_size`
- `abi_version`

Always set:
```c
.struct_size = sizeof(mod_api_t),
.abi_version = MOD_ABI_VERSION,
```

The current compatibility rule is:
- major must match
- minor must match
- patch is ignored

### Static State and Restart

Static variables live as long as the DLL remains loaded. Example:
```c
static int g_counter;
```

A Stop followed by Start does not reload the DLL. So this happens:
```
DLL loads
  g_counter starts at 0

Start
  init runs

Stop
  deinit runs
  DLL stays loaded
  g_counter still exists

Start again
  init runs again
  g_counter still has its old value unless reset
```

Reset per-run state in `init`. Clear pointers in `deinit`.

### Managed Memory

#### Permanent Arena

```c
mod_arena_t arena = mod_get_perm();
```

Use it for memory that should live for one active run. Example:
```c
typedef struct {
  int counter;
} state_t;

static state_t *g_state;

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  g_state = MOD_ARENA_PUSH_ZERO(mod_get_perm(), state_t);

  return g_state != NULL;
}

static void
example_deinit(mod_t mod)
{
  (void)mod;
  g_state = NULL;
}
```

**The memory becomes invalid after the mod stops**.

#### Dynamic Arena

```c
mod_arena_t arena = mod_arena_create(64 * MB, 64 * KB);
```

Use a dynamic arena when one part of the mod needs its own reset or destroy point.

#### Scratch Arena

```c
tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);

/* temporary allocations */

mod_scratch_end(tmp);
```

Scratch memory becomes invalid at `mod_scratch_end`. **Do not store scratch pointers**.

#### External Memory

Memory from `malloc`, `VirtualAlloc`, or another library is not managed by Overdub. Free it in `deinit`.

### Configuration

Get a config handle:
```c
static mod_cfg_t g_enabled_cfg;

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  g_enabled_cfg = mod_get_cfg_by_id(STR_LIT("feature_enabled"));

  return g_enabled_cfg != MOD_CFG_INVALID;
}
```

Read the value:
```c
bool enabled = mod_cfg_get_bool(g_enabled_cfg);
```

There is no special config-changed callback. A simple pattern is to compare the value during tick:
```c
static bool g_last_enabled;

static void
example_tick(mod_t mod, float delta)
{
  (void)mod;
  (void)delta;

  bool enabled = mod_cfg_get_bool(g_enabled_cfg);

  if (enabled != g_last_enabled) {
    g_last_enabled = enabled;

    if (enabled) {
      /* enable feature */
    } else {
      /* disable feature */
    }
  }
}
```

### Logging

```c
MOD_LOG_DEBUG("debug value: %d", value);
MOD_LOG_INFO("mod started");
MOD_LOG_WARN("object not found");
MOD_LOG_ERROR("initialization failed");
```

Do not log every frame or every `ProcessEvent` call in normal use. Hot-path logging can cause large slowdowns.

### Console Commands

Register commands during `init`.
```c
static void
example_command(mod_t mod, str_t name, str_t args, void *user)
{
  (void)mod;
  (void)name;
  (void)user;

  MOD_LOG_INFO("args: " STR_FMT, STR_ARG(args));
}

static bool
example_init(const mod_host_api_t *host, mod_t mod)
{
  if (!mod_sdk_init(host, mod)) {
    return false;
  }

  return mod_register_cmd(STR_LIT("example.run"), STR_LIT("Runs the example command"), example_command, NULL);
}
```

Use a prefix such as `example.` to avoid command name conflicts.

### Input and Keybinds

A direct input callback:

```c
static bool
example_input(mod_t mod, input_event_t *event)
{
  (void)mod;

  if (event->kind == INPUT_EVENT_KEY_DOWN && event->key == INPUT_KEY_F8 && !event->is_repeat) {
    MOD_LOG_INFO("F8 pressed");
    return true;
  }

  return false;
}
```

Returning true blocks later mod input callbacks and the game input handler.

#### Keybind Polling

For option keybinds, polling from tick is often easier.

```c
static mod_cfg_t g_action_key_cfg;

static void
example_tick(mod_t mod, float delta)
{
  (void)mod;
  (void)delta;

  keybind_t bind = mod_cfg_get_keybind(g_action_key_cfg);
  if (mod_keybind_is_pressed(bind)) {
    /* run once */
  }
}
```

Use:
```
is_down      held now
was_down     held last frame
is_pressed   changed from up to down
is_released  changed from down to up
```

### Finding UObjects

Include:
```c
#include "mod_unreal.h"
```

Cache common Unreal objects during initialization:
```c
unreal_cache_objects();
```

#### Find a Class by Full Name

```c
uclass_t *actor_class = unreal_uobject_find_class_by_full_name(STR_LIT("Class /Script/Engine.Actor"));
```

#### Find a Live Instance

```c
uobject_t *actor = unreal_uobject_find_first_of(actor_class);
```

The first instance may not be the instance you want. Filter by:
- full name
- outer chain
- world
- class
- object flags
- known owner

#### Validate It

```c
if (!actor || !unreal_uobject_is_valid(actor)) {
  actor = NULL;
}
```

#### Check Its Type

```c
if (!unreal_uobject_is_a(actor, actor_class)) {
  actor = NULL;
}
```

### UObject Listeners

Register listeners during `init`.

```c
static uobject_t *g_tracked = NULL;

static void
on_created(uobject_t *obj, int32_t idx, void *user)
{
  (void)obj;
  (void)idx;
  (void)user;

  /* keep this fast */
}

static void
on_deleted(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  if (obj == g_tracked) {
    g_tracked = NULL;
  }
}
```

Register:
```c
if (!mod_register_uobject_listener(UOBJECT_LISTENER_KIND_CREATE, on_created, NULL)) {
  return false;
}

if (!mod_register_uobject_listener(UOBJECT_LISTENER_KIND_DELETE, on_deleted, NULL)) {
  return false;
}
```

Overdub removes managed listeners after `deinit`.

### Custom Native Hooks

Use native hooks when reflection is not enough. The SDK provides:
- signature scanning
- hook creation
- hook enable
- hook disable
- hook removal

Example shape:
```c
typedef void (__fastcall *target_fn_t)(void *self);

static target_fn_t g_target_real;

static void __fastcall
target_hook(void *self)
{
  /* before */

  g_target_real(self);

  /* after */
}
```

Resolve and install:
```c
static bool
install_target_hook(void)
{
  void *target = NULL;

  sigscan_entry_t entry = {
    .name    = "target",
    .pattern = "48 89 5C 24 ?? 57 48 83 EC ??",
    .pp      = &target,
    .op_off  = 0,
    .deref   = 0,
  };

  if (mod_sigscan(&entry) != SIGSCAN_ERR_OK || !target) {
    return false;
  }

  if (!mod_hook_create(target, target_hook, (void **)&g_target_real)) {
    return false;
  }

  return mod_hook_enable(target);
}
```

The pattern is only an example.

#### Hook Rules

A detour must:
- use the correct calling convention
- use the correct parameter types
- call the original when required
- handle recursion
- avoid slow work
- handle the real calling thread
- stop using mod state during shutdown

Hooks created through the SDK are removed after `deinit`.

### Custom UI

Include:
```c
#include "mod_nuklear.h"
```

Example panel:
```c
static void
example_draw_panel(mod_t mod, struct nk_context *ctx)
{
  (void)mod;

  nk_layout_row_dynamic(ctx, 24.0f, 1);
  nk_label(ctx, "Example panel", NK_TEXT_LEFT);

  if (nk_button_label(ctx, "Run")) {
    /* action */
  }
}
```

Overdub uses a custom version of Nuklear rather than the unmodified upstream library. Mods should use `mod_nuklear.h` and the matching SDK files shipped with their Overdub version. Do not assume that code written for another Nuklear version has the same declarations or behavior.

Nuklear is immediate mode. The controls are rebuilt every frame. Rules:
- do not store the context pointer
- do not call D3D12 directly
- do not block
- keep UI state in your mod
- keep stable labels in valid memory
- expect window resize and resolution changes

## 5. Workflows and Debugging

### Asset Mod Workflow

A simple asset workflow is:
```
create or edit assets in Unreal Editor
|
v
cook for the correct Windows target
|
v
package as PAK or IoStore
|
v
place files beside mod.ini
|
v
declare path_without_ext
|
v
restart the game
```

Common problems:
- wrong Unreal Engine version
- wrong cooked platform
- wrong package path
- missing dependency
- editor-only asset
- wrong Blueprint class path
- missing `_C`
- asset conflict with another mod
- wrong mount order

### Blueprint Mod Workflow

A simple Blueprint actor workflow is:
```
create Blueprint actor
|
v
add mod logic
|
v
cook and package it
|
v
add [assets]
|
v
add [blueprint]
|
v
start the game
|
v
Overdub loads class and spawns actor
```

A useful entry actor should:
- own its event bindings
- clean up on EndPlay
- avoid assuming one permanent world
- recreate state after travel when needed
- avoid depending on editor-only objects

### Reload-Safe Design

DLL reload only works when the old DLL can be removed safely.

#### init Checklist

- reset all per-run globals
- initialize the SDK
- get required config handles
- allocate state
- resolve required classes and functions
- create managed hooks
- register managed listeners
- register commands
- start threads only after state is ready
- fail cleanly

#### Runtime Checklist

- validate UObject pointers
- handle world changes
- avoid slow tick work
- avoid hot-path logging
- use recursion guards
- copy data before sending it to workers
- never keep parameter buffers or FFrame pointers

#### deinit Checklist

- mark the mod as shutting down
- stop creating new jobs
- disable unmanaged callbacks
- signal threads
- join every thread
- restore manual patches
- remove engine delegates
- release handles
- release COM objects
- free external memory
- clear UObject pointers
- clear arena pointers
- return only when no code can call the DLL again

### Debugging

#### Use the Log

Log important steps:
```c
MOD_LOG_INFO("searching for player class");
MOD_LOG_WARN("player was not found");
MOD_LOG_ERROR("failed to install hook");
```

Include enough information to understand what failed.

#### Use UObject Search

UObject Search helps find:
- loaded classes
- live instances
- object names
- outer chains
- properties
- functions
- class inheritance
- Kismet bytecode

Selecting a search result opens it in a preview tab. The preview tab is reused when another result is selected. Click the tab itself to pin it. Linked objects in the details panel can also be opened from their context menu with `Open in new tab`.

The details panel has these shortcuts:

| Shortcut   | Action                  |
|------------|-------------------------|
| `Tab`      | Select the next tab     |
| `Ctrl+Tab` | Select the previous tab |
| `Ctrl+W`   | Close the selected tab  |

The shortcuts are handled only while the details panel accepts input.

#### Use UFunction Tracer

The tracer records calls seen through the `ProcessEvent` and `UFunction::Invoke` hooks. Start with a narrow capture filter. Capturing every function can fill the call limit quickly and can noticeably slow the game.

**Rule syntax**

Write one rule per line or separate rules with semicolons:

```
+target:text
-target:text
```

Every valid rule starts with a sign:

- `+` is an include rule
- `-` is an exclude rule

The target and text are separated by `:`. When the target is omitted, the tracer uses `func`.

```
+OnBeat
```

is the same as:

```
+func:OnBeat
```

Whitespace around rules, targets, and text is ignored.

**Filter targets**

The table uses the shortest recommended target names. The other names are accepted aliases.

| Target          | Accepted aliases                                       | Matches                               |
|-----------------|--------------------------------------------------------|---------------------------------------|
| `self`          | `self_name`, `obj`, `object`                           | Name of the object receiving the call |
| `self_outer`    | `self_outer_name`, `obj_outer`, `object_outer`         | Names in the object's outer chain     |
| `class`         | `class_name`, `self_class`                             | Name of the object's class            |
| `class_outer`   | `class_outer_name`, `self_class_outer`                 | Names in the class outer chain        |
| `class_inherit` | `class_super`, `self_super`                            | Class and inheritance-chain names     |
| `func`          | `func_name`, `function`                                | Function name                         |
| `func_outer`    | `func_name_outer`, `func_outer_name`, `function_outer` | Names in the function outer chain     |

An outer-chain or inheritance-chain target checks the individual names in that chain. It does not compare one assembled full path.

Rules with no sign, an unknown target, empty text, or invalid syntax are ignored. The current implementation stores up to 128 parsed rules from a filter input buffer of up to 4 KiB.

**How several rules are combined**

Include rules use OR.

```
+func:OnBeat
+func:OnDamage
```

A call passes the include stage when either function rule matches. Two include rules do not mean that both conditions must match.

Exclude rules are checked after includes. Any matching exclusion rejects the call.

```
+class_inherit:Actor
-func:Tick
-self:Default__
```

This captures calls where the receiving object's class is `Actor` or inherits from it, except functions whose names match `Tick` and objects whose names match `Default__`.

When a filter contains only exclude rules, every call starts included and only matching exclusions are removed:

```
-func:Tick
-func:ReceiveTick
```

When there are no valid rules, every call passes.

**Match options**

Matching is a substring search by default.

- `Ignore case` applies case-insensitive matching to every rule
- `Exact match` requires an individual target name to equal the rule text
- both options apply to the whole filter, not to one rule

**Nested calls**

Normally every root call must pass the capture filter.

When `Capture nested calls` is enabled, a call made inside an already captured call is recorded without applying the filter to that nested call. This is useful for seeing the call tree below one narrow entry point, but it can produce many results.

The tracer currently does not save parameter or return values. Captured object pointers can also become invalid after their objects are destroyed.

#### Use a Debugger

Hi-Fi RUSH is protected by VMProtect, so normal debugger attachment is blocked. For the current game builds, TitanHide is the only known working bypass used for normal native debugging. Without it, rely on logging, crash stack traces, static analysis, and Overdub's built-in tools.

Build the mod with symbols and attach a debugger. Useful choices:
- x64dbg
- IDA debugger
- another x64 debugger

A crash may happen later than the original memory corruption. Check the first invalid write, not only the final crash.

#### Save Crash Information

Keep:
- `overdub.log`
- stack trace
- game version
- store version
- Overdub version
- mod version
- installed mod list
- exact reproduction steps
- PDB files for native mods

### Source Map

When this guide and the implementation disagree, check the code.

| Area                              | Source                                   |
|-----------------------------------|------------------------------------------|
| Loader entry and main callbacks   | `src/main.c`                             |
| Signatures and core engine hooks  | `src/signatures.c`                       |
| Manifest parser and mod lifecycle | `src/mod_manager.c`                      |
| Host API                          | `src/mod_host.c`, `include/mod_host.h`   |
| Public mod ABI                    | `mod/mod_api.h`                          |
| SDK wrappers                      | `mod/mod_sdk.h`, `mod/mod_sdk.c`         |
| Unreal helpers                    | `mod/mod_unreal.h`, `mod/mod_unreal.c`   |
| Nuklear helpers                   | `mod/mod_nuklear.h`, `mod/mod_nuklear.c` |
| D3D12 overlay                     | `src/nk_d3d12.c`                         |
| Built-in tool examples            | `src/builtin/`                           |

### Topics for Later Drafts

The following subjects need their own detailed examples:
- creating cooked PAK files
- creating IoStore files
- Unreal project setup for Hi-Fi RUSH
- reflected property type checking
- safe `FString` and `FText` handling
- arrays, maps, and sets
- delegates
- sparse multicast delegates
- data tables
- data assets
- stable world tracking
- real Hi-Fi RUSH hook examples
- C++ mod examples
