/* SPDX-License-Identifier: Zlib */
/* Copyright (c) William Pimentel-Tonche and contributors. All rights reserved. */

#ifndef core_property_h
#define core_property_h

/*	core_property.h

	Exposes interfaces used to manage properties.

	To support a wide range of games with consistently high scalability, fine-tuning capability, and overall performance,
	the engine uses a property-centric game object model, implemented as a dynamic data-driven property registry.

	It is from these properties that game objects are constructed, in a purely data-driven and data-oriented manner.

	An alternative design is in review that would unify scenes and entities to be one and the same, and then properties
	would themselves be renamed as entities, with a clear separation between a "scene entity" and a "logic entity".

	For example, instead of a Crate entity having a Mesh property and a Health property, a Crate scene would have a
	Mesh scene entity and a Health logic entity, and be instantiated within a larger scene.

	That said, such an approach does kind of depend on how ridiculous you want to get, but it would also bring us much
	closer in line with Godot's developer experience and simplicity while also avoiding its inheritance tree pitfalls,
	if that's worth anything.
*/

#endif /* core_property_h */
