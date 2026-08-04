# Jolt Plugin Plan
It's rather self-evident. This is basically just a plugin to implement physics for Hydra using Jolt.

Rationale for using a plugin (and to that end, using physics as a first test) is threefold:

1. Test the plugin system and make sure it actually works and enables the extensibility I want.
2. Validate that the architectural layering in the engine core itself is correct (i.e., engine physics API can be implemented by a plugin, whatever form that takes)
3. Validate that physics fuckin' works, yo!

Kill three birds with one stone.

After this we can probably start doing some rendering stuff which I also want to live in a plugin, possibly with individual features split across modules in that plugin, so that rendering becomes moddable and swappable later if/when we open this up to the public.

If we did the layering right too, this may not even need an `include/` directory, just a `src/` directory and letting the linker (and DLL loader) do the rest. That's because this plugin should be treated as a leaf with nobody else depending on it.
