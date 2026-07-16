/*
	renderer_ssrt.c

	Implements per-pixel raytracing used to initialize the spatially hashed radiance cache.

	Per-pixel raytracing works by starting at the world-space position of each pixel along the depth buffer (from rasterized primary visibility) and firing outward into the scene.

	For performance reasons, per-pixel raytracing is run at an extremely low resolution (1/16th) and with zero active denoising, relying entirely on product importance sampling
	to produce good candidates. This is because per-pixel raytracing isn't meant to be the primary provider of indirect lighting data; instead, it initializes and provides
	continuous updates to the spatially hashed radiance cache, while raytraced light field probes (renderer_light_field_probe) provides the stable, largely noise-free, quickly-convereged
	final lighting results.
*/
