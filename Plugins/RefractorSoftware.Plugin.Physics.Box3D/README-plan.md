# Box3D Plugin Plan
It's rather self-evident. This is basically just a plugin to implement physics for Hydra using Box3D.

Rationale for using a plugin (and to that end, using physics as a first test) is threefold:

1. Test the plugin system and make sure it actually works and enables the extensibility I want.
2. Validate that the architectural layering in the engine core itself is correct (i.e., engine physics API can be implemented by a plugin, whatever form that takes)
3. Validate that physics fuckin' works, yo!

Kill three birds with one stone.

Why Box3D alongside Jolt? Again, to stress the plugin system (opportunity to test some conflict stuff), as well as to have an alternative if we don't like Jolt for some reason.
