# Plans
This file will become less relevant as we actually execute on these plans.

## Package-Based Model
I'm redesigning Hydra around a package-based model. The idea is that the Hydra core only contains the minimum actual functionality for the engine to operate, including a fixed-function application lifecycle
that cycles through stages (like RE Engine's module entry points). From here, packages can register hooks into these stages and run at those fixed steps.

The core engine should be lightweight, and ideally have zero opinion on the simulation itself - including probably forgoing any game object model entirely, maybe just offering Kohi-style primitives.
Packages can then add their own stuff as needed. However we should retain a Source Engine style separation of client and server. How we do that is TBD but we should do it because retrofitting that
later is a massive pain in the ass. Whether it's rollback or something simpler should also be pluggable.



























---

# MOSTLY GARBAGE BELOW

---

## New Layout
New layout is being optimized around each of the directories under `Source/` housing a module, with each module being a separate Git repository, then all of them smashed together in this one repo.

## Module Shifts
- **Foundation** is the lowest-level and doubles as both a standard library and platform abstraction layer.
	- This includes OS abstraction (research `README-Platform.md`) and RHI abstraction (research `README-RHI.md` and `README-RHI-Addendum.md`).
	- This is in comparison to the responsibilities held by Foundation previously, where it was meant to be mostly platform-agnostic but in practice needed to acknowledge the existing of those concepts anyway.
	- In practice I think it's going to prove far easier for Foundation to hold all the big stuff, and then for individual libraries to accept relevant things as (preferably decomposed) input parameters, instead of what we were trying to do previously which if I remember correctly was almost the other way around.

- **Memory** is being moved to its own module with all its bells and whistles.
	- I don't really want it to depend on Foundation's virtual memory interface directly if possible, preferably accepting whatever it needs via function parameters instead.
	- That is to say that accepting `void *virtualMemory` and other things as parameters is fine, but directly including Foundation headers in source files would be preferable to avoid, since I'm thinking about open-sourcing this module once I'm done with it (as educational and/or reference material).

## Include Paths
I'm thinking that instead of directories like:

```c
#include "RefractorSoftware/Foundation/Something.h"
```

I might just do this instead:

```c
#include "RefractorSoftware.Foundation.Something.h"
```

More dots, but flatter directory structure (just `include/` instead of `include/RE/Foundation/`), and ideally less redundant (no need for that `RE/Foundation/FoundationThing.h` bullshit I was doing earlier; should update the docs to reflect this of course).

## Remaining Considerations
Should I return to Java-style reverse-DNS naming, instead of this more C#-esque approach? Eh, probably not.

```c
// Java-Style (Reverse DNS):
Source/
	com.refractorsoftware.foundation
	com.refractorsoftware.memory
	com.refractorsoftware.engine.core
	com.refractorsoftware.engine.client
	com.refractorsoftware.engine.server
	com.refractorsoftware.engine.standalone
	more as needed...

#include "com.refractorsoftware.foundation.something.h"

// C#-Style (seems like a distant cousin):
Source/
	RefractorSoftware.Foundation
	RefractorSoftware.Memory
	RefractorSoftware.Engine.Core
	RefractorSoftware.Engine.Client
	RefractorSoftware.Engine.Server
	RefractorSoftware.Engine.Standalone
	more as needed...

#include "RefractorSoftware.Foundation.Something.h"

// or folders if that really bothers you, but it doesn't really for me
// might be something to consider for platform setup - NDA folders for specific platforms - but AFAIK that is more of a src/ concern which already probably won't follow the above pattern
#include "RefractorSoftware/Foundation/Something.h"

// middle-ground if needed
// actually now that I think about it, this might be preferable, less duplication in filenames... just remember to be careful when including
// files relative to each other, use the full path starting from include/ not relative ones or we'll cook ourselves later
#include "RefractorSoftware.Foundation/Something.h"
```

Java style would be more native-feeling on Unix-like systems *but* C# is a lot more terse and I'd argue probably more descriptive and readable too.

Yeah I think the C# style with the folders will work:
```c
#include "RefractorSoftware/Foundation/Something.h"

// pull in everything
#include "RefractorSoftware/Foundation.h"
```

More than the middle-ground one but clean enough. Could get unwieldy with large module names though:

```c
#include "RefractorSoftware/Plugins/Physics/Jolt/Something.h"

// Or
#include "RefractorSoftware.Plugins.Physics.Jolt/InternalThing.h"
```

But that's not a common case and regardless, the first one of those two is the more honest choice. Just own the depth.

**DEFINITELY** do not mix the two. Consistency beats everything else here.

## API Naming
Might rename API symbols from `RE`/`Re` to `RS`/`Rs`. I know last time I complained about `RsSint32` being harder to read than `ReSint32` but RS is a more suitable namespace anyway. Do something like `RS_int32` if it bothers
you that much. `RS_some_type` and `rsSomeFunction()`, or retaining `RS_Some_FunctionThatDoes()`, or `RsSomeFunction()`. Hmmm. That actually has precedent. `RS_int32_vec3`, `RS_float32_mat4x4`. Can't say it's unreadable.
Verbose, but not unreadable.

*Or you can be a dickhead and just drop the prefix completely.* I mean... it's probably less ugly to do that.

Naming things, just gonna list some options:

- `RsSomeType` / `RS_SomeSpace_SomeFunction()`
- `RS_some_type` / `RS_some_space_some_function()`

Trying some ideas that strip the Refrator Software prefix:
- `PLATFORM_WINDOW` / `platformCreateWindow()` / `platformCreateSurface()`
- `RHI_TEXTURE` / `rhiCreateTexture()`
- `RENDERGRAPH` / `rendergraphAddPass()`
- `INVENTORY_ITEM` / `inventoryItemCreate()`
- `AUDIO_BUFFER` / `audioPushBuffer()`
- `ENTITY_HANDLE` / `entityIsValid()`
- `F32` / `simdLoadF32()`
- `F32_MAT4x4` / `mat4x4ProjectF32()`
- `F64_VEC3` / `vec3AddF32()`
- `CPU_PROPS` / `cpuGetProps()` / `cpuHasProp()` (CPUID stuff)

Close, but no dice. The small inconsistencies set off my OCD. Maybe this:

- `platform_window` / `Platform_Window_Create()` / `Platform_Surface_Create()`
- `rhi_texture` / `RHI_Texture_Create()`
- `render_graph` / `Render_Graph_Add_Pass()`
- `inventory_item` / `Inventory_Item_Create()`
- `audio_buffer` / `Audio_Buffer_Push()`
- `entity_handle` / `Entity_Handle_Is_Valid()`
- `f32` / `Simd_Load_F32()`
- `f32mat4x4` / `Mat4x4_Project_F32()`
- `f64vec3` / `Vec3_Add_F32()`
- `cpu_props` / `CPU_Get_Props()` / `CPU_Has_Prop()`

Things I need the naming scheme and capitalization to do:

- Grow, grow, grow. Forever and ever and ever as we add more and more stuff - more APIs, more systems, more implementation.
- Not be a complete and utter pain in the ass to type.
- Never collide with anyone else's shit... ideally.
- Be very clear, legible, hard to misunderstand or misuse.

Godot has GD. Unreal Engine has UE. Unity has... Unity, I guess. SDL has, well, SDL. Nvidia has NV.

In *theory* we, Hydra, have HY, HR, HA, whatever. But in practice I want many of these libraries to not be locked to this engine, and even then, I don't want the engine's branding to bleed into the code either.
We want to build mountains of libraries, then build small houses of applications on top of them. So branding multiple libraries under the same name just restrains our ability to do that reasonably.

I think we'll probably end up with something like this in finality:

- Refractor Software primitive types: `Uint8/16/32/64`, `Sint8/16/32/64`, `Float32/64`, `Uptr`, `Sptr`, `Usize`, `Ssize`, `Byte`, `Bool`
- Refractor Software math types:  `Float32Vec3`, `Float32Mat4x4`, and similar vein
- Refractor Software composite types (option 1): `RefractPlatformWindow`, `RefractHandle`, `RefractRenderGraph`, `RefractAudioBuffer`, `RefractResult`, `RefractJobHandle`
- Refractor Software composite types (option 2): `RSPlatformWindow`, `RSHandle`, `RSRenderFrameGraph`, `RSAudioBuffer`, `RSResult`, `RSJobHandle`
- Refractor Software composite types (option 3): `PlatformWindow`, `Handle`, `RenderFrameGraph`, `AudioBuffer`, `Result`, `JobHandle` (and bank on not including a third-party library that defines these, which seems to not be common)
- Refractor Software functions (option 1): `PlatformWindowCreate()`, `HandleValid()`, `RenderFrameGraphAddPass()`, `AudioBufferReset()`, `ResultSucceeded()`, `JobCompleted()`
- Refractor Software functions (option 2): `platformWindowCreate()`, `handleValid()`, `renderFrameGraphAddPass()`, `audioBufferReset()`, `resultSucceeded()`, `jobCompleted()`
- Refractor Software functions (option 3): `rsPlatformWindowCreate()`, `rsHandleValid()`, `rsRenderFrameGraphAddPass()`, `rsAudioBufferReset()`, `rsResultSucceeded()`, `rsJobCompleted()`

Not quite as clean as something like id Software's `idEntity` and friends, but maybe good enough.

Raylib gets away with funny shit like `InitWindow()`, no `rl` prefix that I remember, impressive. Or maybe I'm horribly misremembering something.

I think cglm lets you set your own prefix somehow. Would be stupid for in-house libraries probably, maybe not, IDK. Certainly makes reading the header directly confusing and doesn't work with split declaration-definition (i.e., doesn't
work across header-source splits) unless you were to pass that to the command line, adding a command line parameter to switch a function prefix. Ridiculous for us.

So far the best option I can think of is to prefix based on a per-module codename, or just based on the subsystem, or just slap a big fuckin' `R`/`RS` in front of everything:

- `RSCreateWindow()`, `RSHandleValid()`, `RSRenderFrameGraphAddPass()`, `RSAudioBufferReset()`, `RSResultSucceeded()`, `RSJobCompleted()`, `RSPhysicsInit()`

Hey, look, `pthread` has that precedent. I get that POSIX is a standard and all so `p` is obvious like that, but `RPhysicsInit()` and `RPhysicsRigidbody` are way better than
`rsPhysicsInit()` or `RefractPhysicsRigidbody` to my eyes.

- `RSystemWindow`
- `RSystemWindowCreate()`
- `RRenderDeviceInit()`
- `RRenderDeviceCreateTexture()`
- `RRenderFrameGraph`
- `RRenderFrameGraphAddPass()`
- `RAudioBufferReset()`
- `RResultSucceeded()`
- `RJobCompleted()`
- `RPhysicsInit()`

Or fuck em. If we open source it is pure courtesy and we are not concerned with breaking other people's shit.
- `SystemWindow`
- `SystemWindowCreate()`
- `RenderDeviceInit()`
- `RenderDeviceCreateTexture()`
- `RenderFrameGraph`
- `RenderFrameGraphAddPass()`
- `AudioBufferReset()`
- `ResultSucceeded()`
- `JobCompleted()`
- `PhysicsInit()`
