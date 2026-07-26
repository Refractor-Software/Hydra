/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#pragma once

/*
    win64_render.h

    Owns the D3D12 device/swapchain/present pipeline for the Windows platform. Kernel-only
    bring-up: validates device creation, command submission, and present in isolation, with zero
    engine involvement. What (if anything) crosses an engine/kernel render boundary is a separate,
    later design pass (core/services/render doesn't exist yet) - out of scope here.
*/

#include "RE/Win64/Win64.h"

#include <RE/Foundation/FoundationPrimitiveTypes.h>

/* Creates the DXGI factory, selects a DXR Tier 1.0+-capable adapter, creates the device, direct
 * command queue, per-frame command allocators/list, swapchain sized to the window's current
 * client rect, RTV heap, and fence. Call once, after the window exists. Returns 0 (with a
 * MessageBoxW explaining why) if no DXR-capable adapter exists or device/swapchain creation
 * otherwise fails.
 */
b8 win64_render_init (HWND window);

/* Records and submits one frame (reset -> clear -> present -> signal fence) using a placeholder
 * clear color. Call once per tick, after application_tick().
 */
void win64_render_draw (void);

/* Records that the window's client size has changed. Does no GPU work - safe to call directly
 * from win64_window_proc's WM_SIZE handler, including before win64_render_init() has run.
 */
void win64_render_notify_resize (u32 width, u32 height);

/* If a resize was queued via win64_render_notify_resize(), waits for GPU idle and resizes the
 * swapchain buffers + RTV heap to match. Call once per tick, after the message pump has drained,
 * before win64_render_draw() - never from inside WndProc.
 */
void win64_render_process_resize (void);

/* Waits for GPU idle, releases every D3D12/DXGI COM object. Call once, after the main loop ends. */
void win64_render_shutdown (void);
