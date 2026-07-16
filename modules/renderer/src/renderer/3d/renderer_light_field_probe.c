/*
	renderer_light_field_probe.c

	Implementation of a real-time raytraced light field probes (RTLFP).

	The fundamental idea around this is basically an evolved, advanced version of RTXGI/DDGI that uses Spherical Gaussian (SG) encoding instead of Spherical Harmonic (SH) encoding.
	The reason to do this is that SG provides much better directionality and glossy response, which is important for physically plausible lighting in scenes not dominated by purely diffuse materials.

	RTLFP starts by subdividing the scene into a 3D octree. It tests occupancy/density per cell checking for geometry proximity and complexity, then subdividing as needed.
	Flat open areas get coarse, sparse cells, while cluttered/complex areas get finer, denser cells.

	Once subdivision is completed (it's constrained up to some given budget), probes can be spawned at the resulting adaptive positions, optionally relocated out of geometry if subdivision
	somehow created candidate positions inside of geometry. From here it's the same update loop as RTXGI/DDGI: raytrace the scene, update probes, fit SG lobes, accumulate temporally, interpolate
	with visibility awareness, and so on.

	To accelerate convergence and improve temporal stability, RTLFP uses both probe feedback loops and a spatially hashed radiance cache (SHaRC). The SHaRC itself is implemented by renderer_sharc
	and is populated both by probe rays as well as the low-resolution per-pixel raytracing path.
*/
