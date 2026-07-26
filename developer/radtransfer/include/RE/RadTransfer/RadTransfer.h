/*
	radtransfer.h
	Developer tool for precomputing radiance transfer for 3D assets.
	Intended to be used per-asset by a background baking process, not per-scene (which tends to be too expensive with too much churn in development).
	Per-scene radiance is calculated in real-time with either VXGI (compute-only) or raytraced light field probes (hardware raytracing only).
*/

#pragma once


