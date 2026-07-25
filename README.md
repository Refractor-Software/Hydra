# Hydra

Welcome to the source code for **Hydra**, Refractor Software's high-performance 3D game engine.

This repository contains the Hydra engine runtime, the Hydra Studio development environment, and various additional tools
used by the Refractor Software game development and production units.

> [!WARNING]
> Hydra is maintained in the public square, currently under a **"use at your own risk"** or **"throw it over the wall"** model.
>
> While most certainly sufficiently modular and composable enough to be adapted to your team's needs (and if you're a small studio looking to
> get away from the commercial off-the-shelf options, you might find it to be a suitable starting point), it is purpose-built for Refractor
> Software's game development needs which emphasize data-driven and data-oriented design, particularly using data assets, data tables, and
> composition.
>
> Because of this design, Hydra can be extended to support theoretically any game's needs, but it doesn't have a general-purpose scripting
> language designed to be used by noobs and script kiddies. You need to know what the hell you're doing. If you want a general-purpose hammer
> that little Timmy can come home and make a AAA game overnight with visual scripting or something like that, then **Hydra is not the engine
> for you** and you should probably look elsewhere.
>
> Refractor Software makes zero guarantees that Hydra is suitable for your use case, and will not make it work for you. See the
> [license](COPYING) for more information.

## Purpose

Hydra is being developed to excel at rendering **high fidelity worlds** at **high resolutions and framerates** on current-generation console hardware,
while supporting the variety of (typically action) games developed at Refractor Software. It emphasizes data-oriented and data-driven design, with a
heavily multithreaded codebase that's designed to be extended as needs evolve and scale to huge volumes of content with ruthless efficiency.

### Hardware Performance Targets

- **Input Resolution (Max)** is the target input resolution that we optimize to hit as often as possible.
- **Input Resolution (Avg)** is for hard-to-predict bad cases (<10% of all gameplay time).
- **Input Resolution (Min)** is for unpredictable worst-cases (<1% of all gameplay time).

| Platform                     | Input Resolution (Min) | Input Resolution (Avg) | Input Resolution (Max) | Output Resolution | Framerate |
|------------------------------|------------------------|------------------------|------------------------|-------------------|-----------|
| PlayStation 5                | 1920x1080              | 2304x1296              | 2560x1440              | 3840x2160         | 60hz      |
| PlayStation 5 Pro            | 2304x1296              | 2688x1512              | 3072x1728              | 3840x2160         | 60hz      |
| Xbox Series X                | 1920x1080              | 2560x1440              | 2880x1620              | 3840x2160         | 60hz      |
| Xbox Series S                | 1536x864               | 1792x1008              | 1920x1080              | 2560x1440         | 60hz      |
| Nintendo Switch 2 (Docked)   | 1536x864               | 1792x1008              | 1920x1080              | 2560x1440         | 60hz      |
| Nintendo Switch 2 (Handheld) | 1056x594               | 1344x756               | 1440x810               | 2560x1440         | 60hz      |
