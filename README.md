# Hydra

Welcome to the source code for **Hydra**, Refractor Software's high-performance 3D game engine.

This repository contains the Hydra engine runtime, the Hydra Studio development environment, and various additional tools
used by the Refractor Software game development and production units.

> [!WARNING]
> Hydra is maintained in the public square under a **"use at your own risk"** model.
>
> While most certainly sufficiently modular and composable enough to be adapted to your team's needs (and if you're a small studio looking to
> get away from the commercial off-the-shelf options, you might find it to be a suitable starting point), it is purpose-built for Refractor
> Software's game development needs which emphasize data-driven and data-oriented design, and generally prefers purpose-built modules that do
> one thing well instead of general-purpose component-system amalgamations that do a lot of things poorly.
>
> Because of this design, Hydra can be extended to support theoretically any game's needs, but doing so requires writing C code into
> a new module. While this isn't particularly difficult if you're familiar with computers and how they work, if it does sound scary to
> you for any reason (or, you know, you're just not familiar with writing code), then Hydra may not be the engine for you. That said,
> virtually any team with a competent engineer on board can probably figure it out.
>
> Refractor Software makes zero guarantees that Hydra is suitable for your use case, and will not make it work for you. See the
> [license](COPYING) for more information.

## Purpose

Hydra is being developed to excel at rendering **high fidelity worlds** at **high resolutions and framerates** on current-generation console hardware,
while supporting the variety of (typically action) games developed at Refractor Software.

### Hardware Performance Targets

| Platform          | Resolution | Antialiasing | Framerate |
|-------------------|------------|--------------|-----------|
| PlayStation 5     | 2560x1440  | SMAA 4x      | 60hz      |
| PlayStation 5 Pro | 3264x1836  | SMAA 4x      | 60hz      |
| Xbox Series X     | 2880x1620  | SMAA 4x      | 60hz      |
| Xbox Series S     | 1920x1080  | SMAA 4x      | 60hz      |
| Nintendo Switch 2 | 1600x900   | SMAA 4x      | 60hz      |
