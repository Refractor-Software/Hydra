/* SPDX-License-Identifier: Zlib */
/* Copyright (c) William Pimentel-Tonche and contributors. All rights reserved. */

#ifndef core_scene_h
#define core_scene_h

/*	core_scene.h

	Exposes interfaces used to manage scenes.

	Like Godot, **scenes** are the basic unit of game object composition in the engine. A scene triples as a scene,
	a prefab, and a game object. A scene is a flat listing of **entities**. There are three types of entities:

		- Functional Entities	(no scene transform)
		- Canvas Entities		(live in the 2D transform space)
		- Spatial Entities		(live in the 3D transform space)

	Canvas and spatial entities can collectively be referred to as Positional Entities.

	Perhaps slightly counterintuitively to their name, entities are a lot more like "components" in other game engines.
	They typically store only the single set of properties relevant to their functionality that they were authored with,
	plus a transform of the appropriate type for positional entities.

	To attach positional entities to other positional entities, the transform system provides additional facilities for
	attachment, allowing positional entities to define attachment rules, which can be one of:

		- No children		(cannot have attached children, it can only stand alone)
		- One child			(can have at most one attached child)
		- Many children		(can have as many children attached as desired)

	In addition to attachment rules, the transform system also provides facilities for **sockets**, which are named slots
	that positional entities can attach to and follow. This is useful for meshes and mesh entities (static meshes, skeletal
	meshes, etc.) and is heavily used by many types of canvas entities in the user interface world (for example, a button
	offering a named socket for the content it contains).

	Sockets and transform attachment rules combine to enforce any invariants in the scene tree that need them. Typically
	such restrictions aren't super necessary or important, but they can be in some cases, so these facilities are present for
	when that is the case.

	Entities do **not** share any kind of inheritance hierarchy. Instead, they are laid out plainly in memory, with each
	entity type storing its data in a manner that's optimized for its problem space (typically a mix of AoS, SoA, and AoSoA).

	When entities do need to split out and share some of their properties (for example, positional entities all have transforms,
	and it would be wasteful to scatter transforms across memory in different entity arrays), that property tends to be split
	out into a separate subsystem and entities will hold a reference to it instead (for example, positional entities hold a
	serial index to their transforms, rather than the transform itself).

	The way this design is meant to play out in practice, compared to other engines, with an example of creating a crate
	object:

		Hydra (this engine):
		- Create crate.scn
		- Add a Static Mesh entity
		- Add a Health entity (or Gameplay Attributes entity and add a Health attribute)
		- Instantiate within other scenes as needed; changes to the archetype and/or default values affect all instances'
		default values

		Unreal Engine:
		- Create BP_Crate
		- Add a Static Mesh Component
		- Add a Health Component (or Ability System Component and a Health attribute set)
		- Instantiate within levels as needed; changes to the archetype and/or default values affect all instances'
		default values

	It's a fairly similar workflow, but with this engine you don't need to know how to use multiple editors, you just
	need to know how to use the one unified scene editor to do all the things.

	The way Hydra makes this work, and makes it work stupid fast (like, substantially faster than Godot for example)
	is that the engine-side representation of things is a lot more optimized and in fact is entirely centered around
	asynchronous partitioned streaming and data-oriented design. Specifically:

		- More optimized data layout than Godot (optimized flat arrays of entities instead of heap-allocated Nodes).
		- All scenes are part of an underlying partitioned World, which can automagically stream scenes in/out
		as the camera travels, or even as entire chunks of scenes are activated/deactivated.
		- Scenes themselves are ultra-lightweight and easy to stream; just serial indices that map to its entities and
		maybe some baked data.

	This also feeds into the engine's streaming architecture. Like RE ENGINE, games cannot directly load or unload assets;
	they can only change the active state of a scene, either via partition streaming that respects streaming origins (camera
	and other optional origins, which can be added to any scene via the Streaming Origin entity), or manually activating
	or deactivating scenes via game logic.

	With the Scene system, the engine empowers designers with incredible flexibility to create the games of their dreams,
	without needing to concern themselves with complicated underlying performance or streaming characteristics or needing
	to master a variety of topics - they can master just one, the scene editor, and be off to the races.
*/

#endif /* core_scene_h */
