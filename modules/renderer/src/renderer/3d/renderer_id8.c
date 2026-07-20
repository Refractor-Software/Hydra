/*
	renderer_id8.c

	Implements raytraced indirect lighting similar to id Tech 8 ("SIGGRAPH 2025 Advances in Real-Time Rendering in Games: Fast as Hell: idTech8 Global Illumination") and RE ENGINE ("RE:2023 Advances in Ray Tracing").

	Improving on id Software's approach:

		Our solution for working around id's precomputed lighting woes is to precompute lighting data per-object, *not* per-scene.
		We can do this with **precomputed radiance transfer**, storing SH coefficients, and additionally we're going to store SG lobes to greatly improve glossy and rough specular response for a pretty low cost.
		This should also resolve id Tech 8's problem of not being able to account for glossy without the use of hand-placed cubemaps.

		To address probe spawning and density challenges, we'll replace id Software's combined cascaded and local probe volumes with a single unified cascaded clip-mapped adaptive probe volume, like Unity's
		APV (id Tech 8's clustered light grid also appears to function a bit similarly). This adaptive volume voxelizes the scene into a coarse octree and subdivides based on geometric proximity and complexity.
		From here, the octree subdivides based on geometric density and/or complexity. We can probably aid this process using signed distance fields (SDF) which will already need to be in the engine for other
		reasons as-is. Additionally, we'll upgrade the probe grid to also store SG lobes in addition to the usual SH coefficients.

		In the end this will result in an adaptive indirect light field probe grid that can respond quickly to scene changes (many tricks out there on how to update voxel octrees quickly, and because we're not going
		as dense as VXGI/SVOGI it's even faster) and achieve the density that used to require hand-placed volumes to achieve, improving overall lighting quality immensely.

	Ideas for our implementation to attempt improving on the reference works:

		Instead of combining cascaded and local volumes, we use a single unified cascaded adaptive probe volume, like Unity's APV (id Tech 8's clustered light grid also appears to function a bit similarly).
		This adaptive volume stores the scene into an octree and subdivides based on geometric proximity and complexity. This basically automates what hand-placed local volumes did in id Tech 8.

		Instead of encoding irradiance volume probes with Spherical Harmonics (SH), we're experimenting with encoding *radiance* volume probes, more similar to Lumen's world-space radiance cache or light field probes.
		This would be part of an effort to dramatically improve the system's handling of glossy and rough specular lighting. If this doesn't work out, we can reuse id Tech's strategy of re-fitting environment probe
		grids. And if we don't want to do that, then we can experiment with RE ENGINE's lower-resolution and checkerboarded per-pixel specular path.

		We're also going to experiment with storing the final gather results in either higher-order SH, or SG. At 1/16 resolution, possibly with checkerboarding, higher-fidelity storage should not be a huge issue.
		From here we'll take notes from Capcom's RE ENGINE on how to denoise and upscale such a low-resolution diffuse and rough specular result while maintaining visual quality,
		possibly in tandem with (or replacing) id Tech 8's approach to this.

		Another consideration, while we're at it, is that we'll try to do all of this while only running a single fidelity mode that targets *all* platforms with fixed performance profiles:

			Steam Deck        | 640p  | 60hz
			Nintendo Switch 2 | 864p  | 60hz
			Xbox Series S     | 1080p | 60hz
			PlayStation 5     | 1440p | 60hz
			Xbox Series X     | 1620p | 60hz
			PlayStation 5 Pro | 2160p | 60hz
*/
