/*
    RendererSharc.c

    Implements a spatially hashed radiance cache (SHaRC).

    The SHaRC is used to accelerate light field probe convergence. It is populated initially by per-pixel raytracing
    (RendererSsrt), then simultaneously by per-pixel raytracing and, more dominantly, light field probe raytracing
    (RendererLightFieldProbe).
*/
