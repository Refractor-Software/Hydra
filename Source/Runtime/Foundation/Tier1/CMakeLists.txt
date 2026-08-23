## TODO(will) This is the old CMakeLists for Foundation. Needs to be updated to be:
##	- More robust (we now need this to build entirely standalone, in isolation); basically, it needs to start looking like a real independent library not guaranteed to be within Hydra's codebase.
##	- Include whatever system libraries we'll need (whatever Win32/Win64 Kernel links to, we will probably need it).
##	- Probably still going to do that shitty cheap little GLOB_RECURSE because I'm not about to sit here editing this damn thing for EVERY NEW FILE that we add (since that will amount to a lot),
##	unless of course we find a more productive way of adding source files that doesn't require editing this main one constantly... (Is that what add_subdirectory is also useful for?)
##	- Still not sure about whether to keep or throw out reverse-DNS naming. The all-lowercase convention it comes with is rather typical of Unix-like distros, which we want to be as native as possible with
##	despite the fact that we're starting on Windows. *But* the C# style of target naming is a bit more terse and rational, which is why I'm leaning in that direction.
##	- The new platform/ folder is likely where I'm going to want to store actual platform-specific stuff. That should have both include/ and src/ for each platform, so that both the public and private APIs of
##	Foundation can access them; for example when inlining platform- or compiler-specific calls into the main thing.
##	- When working with a platform and expecting to inline its stuff into the public API, rather than including entire system directories, just do a minimal redeclaration of its API,
##	that way we aren't dragging entire third-party includes into the public API and propagating them to users. This may be one place where a C++ compiler is required otherwise we will get a lot of
##	problems later.

add_library(com.refractor.core.foundation STATIC)

target_include_directories(com.refractor.core.foundation PUBLIC include)
target_include_directories(com.refractor.core.foundation PRIVATE src)

## NOTE(will) This will still be a C project, that hasn't changed.
file(GLOB_RECURSE foundation_headers CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE foundation_sources CONFIGURE_DEPENDS "src/*.h" "src/*.c")

target_sources(com.refractor.core.foundation PUBLIC ${foundation_headers})
target_sources(com.refractor.core.foundation PRIVATE ${foundation_sources})

add_subdirectory(test)
