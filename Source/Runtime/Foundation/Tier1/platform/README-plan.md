# Platform Plans

## Unreal Engine Refrence
Unreal Engine features a file in Core/Microsoft, MinimalWindowsApi.h, that does this:

```
// Implementation of a minimal subset of the Windows API required for inline function definitions and platform-specific
// interfaces in Core. Use of this file allows avoiding including large platform headers in the public engine API.
//
// Win32 API functions are declared in the "Windows" namespace to avoid conflicts if the real Windows.h is included 
// later, but are mapped to the same imported symbols by the linker due to C-style linkage.
```

We may want to consider this if we need to inline platform calls into our public API (which we might for things like critical sections,
atomic operations, etc.)

Only problem of course is that we are working in C... no C++ namespaces available... gonna have to be very careful doing this.
Of course, if the source code never includes such a minimal API header alongside platform SDK headers (which is a controllable portion,
because we just need to care about the source code to the platform-specific implementation, not the entire engine), we might be able
to wrangle it into submission...

In shipping non-shared-library builds it's likely that everything would get optimized out with LTO so again this could end up being not
necessary, but keep in mind that anyone who wants to use our library as a shared library will then end up incurring a cost on every function
call. And *then again* inlining platform SDK API calls (what this mechanism would do) will later result in mismatches between what our public API
does and what we implemented in the shared library (i.e., user uses our passthrough public API from one version, then we change something in
the implementation that also involves changing the passthrough public API to use a different OS API, and now everbody who compiled and shipped calls
to the passthrough API from the last version is fucked). And *then again* that is what versioning is for. I mean I could go back and forth all day
on this shit right?

I mean I guess Valve did ship a very modular engine (Source) back in the day without issues so maybe DLL call overhead isn't as disastrous as I make
it out to be. At which point regular static-linked function overhead definitely isn't.

## The More Realistic Path
What we're probably actually going to do is implement the platform and RHI abstraction as a series of wrappers on top of SDL3 and the vendor-specific
APIs, likely starting with Vulkan to get that sweet sweet cross-platform one-shot for Windows and Linux. This will get rid of the existing Win32 backend
and create a dependency on SDL3 (which may or may not be dynamically linked depending on environment - my thinking is static linking for Windows, dynamic
linking for Linux), but it is so hilariously massively worth it due to how much time it saves us at the platform level.

We still end up needing to wrap SDL3, that part doesn't change. And in fact we may even want to go a step further and customize our SDL3 install to bypass
their front-end API and call straight into the dynapi jump table to save ourselves an extra indirection. Of course, if we *don't*, then each platform API
call will be roughly the cost of a virtual function call, which at the frequency we're talking is a complete non-issue for probably 90% of method calls, and
we'd get to use whatever stock SDL3 build ships with the platform or Steamworks.

We also will probably still want our own compiler predef and primitive types, because we don't want to directly include SDL in the public API. We also will
not disable dynapi because we care about preservation (that's half the reason we're rolling our own engine).

### SDL GPU API
Probably won't use this one. It doesn't have the expressiveness we need (barriers, etc.) and we'd rather just roll a custom RHI that *can* express those concepts
because we know we're going to need all of them if we're looking to maximize performance (again just look at our performance targets, those cannot be reached
easily without the extra depth of a proper RHI). That said we have no need to support legacy OpenGL and other such APIs, we only need to work with D3D12/Vulkan/Metal
style APIs which is basically all of our targets.
