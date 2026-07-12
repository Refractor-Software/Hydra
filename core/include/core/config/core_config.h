/* SPDX-License-Identifier: Zlib */
/* Copyright (c) William Pimentel-Tonche and contributors. All rights reserved. */

#ifndef core_config_h
#define core_config_h

/*	core_config.h

	Exposes interfaces used to read/write configuration files.

	Configuration files are how projects configure individual modules' parameters/behaviors via Project Settings.
	More like Unreal Engine and less like RE ENGINE, we configure modules' parameters/behaviors with these config files, instead of over command line parameters.
	They can also be read by the build system, which uses them to strip dead code from shipping builds.

	Changes to configuration files generally require a runtime restart to apply, but this module has no bearing on that requirement, the engine kernel and modules do.
	Either way, restarting from Studio should be easy, just press the restart button. Maybe changing project settings will do this automatically someday.
*/

#endif /* core_config_h */
