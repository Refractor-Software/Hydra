/*
    RendererId8.c

    Implements raytraced indirect lighting similar to id Tech 8 ("SIGGRAPH 2025 Advances in Real-Time Rendering in
    Games: Fast as Hell: idTech8 Global Illumination") and RE ENGINE ("RE:2023 Advances in Ray Tracing").

    Improving on id Software's approach:

        Our solution for working around id's precomputed lighting woes is to precompute lighting data per-object, *not*
        per-scene. We can do this with **precomputed radiance transfer**, storing SH coefficients, and additionally
        we're going to store SG lobes to greatly improve glossy and rough specular response for a pretty low cost. This
        should also resolve id Tech 8's problem of not being able to account for glossy without the use of hand-placed
        cubemaps.

        To address probe spawning and density challenges, we'll replace id Software's combined cascaded and local probe
        volumes with a single unified cascaded clip-mapped adaptive probe volume, like Unity's APV (id Tech 8's
        clustered light grid also appears to function a bit similarly). This adaptive volume voxelizes the scene into a
        coarse octree and subdivides based on geometric proximity and complexity. From here, the octree subdivides based
        on geometric density and/or complexity. We can probably aid this process using signed distance fields (SDF)
        which will already need to be in the engine for other reasons as-is. Additionally, we'll upgrade the probe grid
        to also store SG lobes in addition to the usual SH coefficients.

        In the end this will result in an adaptive indirect light field probe grid that can respond quickly to scene
        changes (many tricks out there on how to update voxel octrees quickly, and because we're not going as dense as
        VXGI/SVOGI it's even faster) and achieve the density that used to require hand-placed volumes to achieve,
        improving overall lighting quality immensely.

        We're also going to experiment with storing the final gather results in either higher-order SH, or SG. At 1/16
        resolution, possibly with checkerboarding, higher-fidelity storage should not be a huge issue. From here we'll
        take notes from Capcom's RE ENGINE on how to denoise and upscale such a low-resolution diffuse and rough
        specular result while maintaining visual quality, possibly in tandem with (or replacing) id Tech 8's approach to
        this.

    Another consideration, while we're at it, is that we'll try to do all of this while only running a single fidelity
    mode that targets *all* platforms with fixed performance profiles:

        Steam Deck        | 640p  | 60hz
        Nintendo Switch 2 | 864p  | 60hz
        Xbox Series S     | 1080p | 60hz
        PlayStation 5     | 1440p | 60hz
        Xbox Series X     | 1620p | 60hz
        PlayStation 5 Pro | 2160p | 60hz

    Finally, while the plan above is focused on hardware raytracing, we'll want to start with a software-raytracing-only
    path. This is because we will likely need/want to do a simpler game to start off - likely one using a restrained,
    on-a-rail, authored, scriptable 3D perspective. The closest reference point being Luigi's Mansion, which isn't quite
    fixed camera like old Resident Evil, but is also not free-camera either, making it a solid balance for something
    needing full 3D without the cost that comes with player-controllable cameras. From here we can ship this GI system
    like that (or at least get it working), then we can extend it with hardware raytracing later as needed.

    It's also worth exploring a software BVH raytracing path, as Crytek's Neon Noir demo did back in 2017. This is being
    entertained as an idea because we're already going to try structuring this system to run as efficiently as possible
    without hardware RT, including all kinds of tricks (such as abusing the properties of SDFs) to do as much work as
    possible *before* we start firing off rays to gather hits and material info and stuff. So, if our BVH tracing only
    ends up being a small fraction of the overall frame, and we can cram it into a software compute kernel rather than
    delegating to the hardware, we could probably get away with it for our first project (which is smaller and stuff)
    and then upgrade later for future projects with ease.
*/
