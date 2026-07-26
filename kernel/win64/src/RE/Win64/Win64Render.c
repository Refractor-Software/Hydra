/* SPDX-License-Identifier: Zlib
 * Copyright (C) William Pimentel-Tonche
 */

#include "RE/Win64/Win64Render.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <stdio.h>

#include <RE/Log/Log.h>

/*
    Win64Render.c

    Kernel-only D3D12 bring-up: device/swapchain/command-queue/fence plumbing and a clear-color
    present loop, with zero engine involvement. What (if anything) crosses an engine/kernel render
    boundary is a separate, later design pass - this only proves the platform-side pipeline works.

    Single-instance module (one window, one device, for the process's lifetime) - state lives as
    plain statics here, same pattern as Win64Input/Win64Gamepad, rather than an opaque handle.
*/

#define WIN64_RENDER_FRAME_COUNT 3

global IDXGIFactory6              *gWin64RenderFactory;
global IDXGIAdapter1              *gWin64RenderAdapter;
global ID3D12Device               *gWin64RenderDevice;
global ID3D12CommandQueue         *gWin64RenderCommandQueue;
global ID3D12CommandAllocator     *gWin64RenderCommandAllocators[WIN64_RENDER_FRAME_COUNT];
global ID3D12GraphicsCommandList  *gWin64RenderCommandList;
global IDXGISwapChain4            *gWin64RenderSwapChain;
global ID3D12DescriptorHeap       *gWin64RenderRtvHeap;
global UINT                        gWin64RenderRtvDescriptorSize;
global ID3D12Resource             *gWin64RenderBackBuffers[WIN64_RENDER_FRAME_COUNT];

global ID3D12Fence *gWin64RenderFence;
global HANDLE        gWin64RenderFenceEvent;
global ReUint64            gWin64RenderNextFenceValue = 1;
global ReUint64            gWin64RenderFrameFenceValues[WIN64_RENDER_FRAME_COUNT];

global ReBool  gWin64RenderInitialized;
global ReBool  gWin64RenderResizePending;
global ReUint32 gWin64RenderPendingWidth;
global ReUint32 gWin64RenderPendingHeight;

internal void
Win64_Render_ShowError (const wchar_t *message)
{
    MessageBoxW (NULL, message, L"Hydra - Fatal Render Error", MB_OK | MB_ICONERROR);
}

internal void
Win64_Render_LogAdapter (const DXGI_ADAPTER_DESC1 *desc, D3D12_RAYTRACING_TIER tier)
{
    RE_LOG_INFO ("[Win64Render] adapter: %ls  DXR tier: %d", desc->Description, (int) tier);
}

internal D3D12_RESOURCE_BARRIER
Win64_Render_TransitionBarrier (ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter  = after;

    return barrier;
}

internal D3D12_CPU_DESCRIPTOR_HANDLE
Win64_Render_RtvHandle (ReUint32 bufferIndex)
{
    /* Struct-by-value COM methods take an extra out-pointer parameter in the C vtable headers
     * (C has no equivalent to C++'s hidden-return-pointer calling convention here).
     */
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    gWin64RenderRtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart (gWin64RenderRtvHeap, &handle);
    handle.ptr += (SIZE_T) bufferIndex * gWin64RenderRtvDescriptorSize;

    return handle;
}

/* Full GPU-idle wait - used for resize/shutdown, not the per-frame draw path (see
 * Win64_Render_WaitForFrame for the cheaper per-slot wait used there).
 */
internal void
Win64_Render_FlushGpu (void)
{
    ReUint64 valueToWaitFor = gWin64RenderNextFenceValue;
    gWin64RenderCommandQueue->lpVtbl->Signal (gWin64RenderCommandQueue, gWin64RenderFence, valueToWaitFor);
    gWin64RenderNextFenceValue += 1;

    if (gWin64RenderFence->lpVtbl->GetCompletedValue (gWin64RenderFence) < valueToWaitFor)
    {
        gWin64RenderFence->lpVtbl->SetEventOnCompletion (gWin64RenderFence, valueToWaitFor, gWin64RenderFenceEvent);
        WaitForSingleObject (gWin64RenderFenceEvent, INFINITE);
    }
}

internal void
Win64_Render_WaitForFrame (ReUint32 bufferIndex)
{
    ReUint64 targetValue = gWin64RenderFrameFenceValues[bufferIndex];

    if (targetValue != 0 && gWin64RenderFence->lpVtbl->GetCompletedValue (gWin64RenderFence) < targetValue)
    {
        gWin64RenderFence->lpVtbl->SetEventOnCompletion (gWin64RenderFence, targetValue, gWin64RenderFenceEvent);
        WaitForSingleObject (gWin64RenderFenceEvent, INFINITE);
    }
}

internal ReBool
Win64_Render_CreateFactory (void)
{
    UINT flags = 0;

    /* NDEBUG (not _DEBUG) is the macro CMake actually guarantees here - only the win64-debug
     * preset's CMAKE_BUILD_TYPE=Debug omits it; win64-development/win64-shipping both define it.
     */
#if !defined(NDEBUG)
    ID3D12Debug *debugController = NULL;
    if (SUCCEEDED (D3D12GetDebugInterface (&IID_ID3D12Debug, (void **) &debugController)))
    {
        debugController->lpVtbl->EnableDebugLayer (debugController);
        debugController->lpVtbl->Release (debugController);
    }
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    IDXGIFactory2 *factory2 = NULL;
    HRESULT hr = CreateDXGIFactory2 (flags, &IID_IDXGIFactory2, (void **) &factory2);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"CreateDXGIFactory2 failed.");
        return RE_False;
    }

    hr = factory2->lpVtbl->QueryInterface (factory2, &IID_IDXGIFactory6, (void **) &gWin64RenderFactory);
    factory2->lpVtbl->Release (factory2);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"IDXGIFactory6 not available - please update your GPU driver.");
        return RE_False;
    }

    return RE_True;
}

/* Picks the highest-preference adapter (favors the discrete GPU on hybrid-graphics laptops) that
 * also supports DXR Tier 1.0+ - this engine's rendering plan fundamentally depends on hardware
 * raytracing, so a machine without it is a real, immediate problem worth failing loudly on now.
 */
internal ReBool
Win64_Render_SelectAdapterAndCreateDevice (void)
{
    for (UINT index = 0; ; index += 1)
    {
        IDXGIAdapter1 *adapter = NULL;
        HRESULT hr = gWin64RenderFactory->lpVtbl->EnumAdapterByGpuPreference (
            gWin64RenderFactory, index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            &IID_IDXGIAdapter1, (void **) &adapter);

        if (hr == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc;
        adapter->lpVtbl->GetDesc1 (adapter, &desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            adapter->lpVtbl->Release (adapter);
            continue;
        }

        ID3D12Device *device = NULL;
        hr = D3D12CreateDevice ((IUnknown *) adapter, D3D_FEATURE_LEVEL_12_1, &IID_ID3D12Device, (void **) &device);
        if (FAILED (hr))
        {
            adapter->lpVtbl->Release (adapter);
            continue;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {0};
        hr = device->lpVtbl->CheckFeatureSupport (device, D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof (options5));

        if (SUCCEEDED (hr) && options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0)
        {
            Win64_Render_LogAdapter (&desc, options5.RaytracingTier);
            gWin64RenderAdapter = adapter;
            gWin64RenderDevice  = device;
            return RE_True;
        }

        device->lpVtbl->Release (device);
        adapter->lpVtbl->Release (adapter);
    }

    Win64_Render_ShowError (
        L"No DXR (DirectX Raytracing) Tier 1.0+ capable GPU found.\n\n"
        L"This engine requires hardware raytracing support.");

    return RE_False;
}

internal ReBool
Win64_Render_CreateCommandInfrastructure (void)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {0};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT hr = gWin64RenderDevice->lpVtbl->CreateCommandQueue (
        gWin64RenderDevice, &queueDesc, &IID_ID3D12CommandQueue, (void **) &gWin64RenderCommandQueue);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"CreateCommandQueue failed.");
        return RE_False;
    }

    for (ReUint32 i = 0; i < WIN64_RENDER_FRAME_COUNT; i += 1)
    {
        hr = gWin64RenderDevice->lpVtbl->CreateCommandAllocator (
            gWin64RenderDevice, D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator, (void **) &gWin64RenderCommandAllocators[i]);
        if (FAILED (hr))
        {
            Win64_Render_ShowError (L"CreateCommandAllocator failed.");
            return RE_False;
        }
    }

    /* One reusable command list - it isn't permanently bound to the allocator it was created
     * with, so Reset()-ing it against a different frame slot's allocator each tick is correct.
     */
    hr = gWin64RenderDevice->lpVtbl->CreateCommandList (
        gWin64RenderDevice, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, gWin64RenderCommandAllocators[0], NULL,
        &IID_ID3D12GraphicsCommandList, (void **) &gWin64RenderCommandList);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"CreateCommandList failed.");
        return RE_False;
    }

    /* Command lists start open/recording - close it since Win64_Render_Draw always Reset()s
     * before recording anyway.
     */
    gWin64RenderCommandList->lpVtbl->Close (gWin64RenderCommandList);

    return RE_True;
}

internal void
Win64_Render_RecreateRtvs (void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    gWin64RenderRtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart (gWin64RenderRtvHeap, &handle);

    for (ReUint32 i = 0; i < WIN64_RENDER_FRAME_COUNT; i += 1)
    {
        gWin64RenderSwapChain->lpVtbl->GetBuffer (gWin64RenderSwapChain, i, &IID_ID3D12Resource, (void **) &gWin64RenderBackBuffers[i]);

        gWin64RenderDevice->lpVtbl->CreateRenderTargetView (gWin64RenderDevice, gWin64RenderBackBuffers[i], NULL, handle);

        handle.ptr += gWin64RenderRtvDescriptorSize;
    }
}

internal ReBool
Win64_Render_CreateSwapchain (HWND window)
{
    RECT clientRect;
    GetClientRect (window, &clientRect);
    UINT width  = (UINT) (clientRect.right - clientRect.left);
    UINT height = (UINT) (clientRect.bottom - clientRect.top);

    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    desc.Width            = width;
    desc.Height           = height;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = WIN64_RENDER_FRAME_COUNT;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode        = DXGI_ALPHA_MODE_UNSPECIFIED;

    IDXGISwapChain1 *swapChain1 = NULL;
    HRESULT hr = gWin64RenderFactory->lpVtbl->CreateSwapChainForHwnd (
        gWin64RenderFactory, (IUnknown *) gWin64RenderCommandQueue, window, &desc, NULL, NULL, &swapChain1);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"CreateSwapChainForHwnd failed.");
        return RE_False;
    }

    /* Disables DXGI's automatic Alt+Enter fullscreen toggle - a known footgun alongside manual
     * resize handling, and not something this bring-up is set up to handle correctly yet.
     */
    gWin64RenderFactory->lpVtbl->MakeWindowAssociation (gWin64RenderFactory, window, DXGI_MWA_NO_ALT_ENTER);

    hr = swapChain1->lpVtbl->QueryInterface (swapChain1, &IID_IDXGISwapChain4, (void **) &gWin64RenderSwapChain);
    swapChain1->lpVtbl->Release (swapChain1);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"IDXGISwapChain4 not available.");
        return RE_False;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {0};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = WIN64_RENDER_FRAME_COUNT;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = gWin64RenderDevice->lpVtbl->CreateDescriptorHeap (
        gWin64RenderDevice, &rtvHeapDesc, &IID_ID3D12DescriptorHeap, (void **) &gWin64RenderRtvHeap);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"CreateDescriptorHeap (RTV) failed.");
        return RE_False;
    }

    gWin64RenderRtvDescriptorSize =
        gWin64RenderDevice->lpVtbl->GetDescriptorHandleIncrementSize (gWin64RenderDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    Win64_Render_RecreateRtvs ();

    return RE_True;
}

internal ReBool
Win64_Render_CreateFence (void)
{
    HRESULT hr = gWin64RenderDevice->lpVtbl->CreateFence (
        gWin64RenderDevice, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **) &gWin64RenderFence);
    if (FAILED (hr))
    {
        Win64_Render_ShowError (L"CreateFence failed.");
        return RE_False;
    }

    gWin64RenderFenceEvent = CreateEventW (NULL, FALSE, FALSE, NULL);
    if (!gWin64RenderFenceEvent)
    {
        Win64_Render_ShowError (L"CreateEventW (fence event) failed.");
        return RE_False;
    }

    return RE_True;
}

ReBool
Win64_Render_Init (HWND window)
{
    if (!Win64_Render_CreateFactory ())                   { return 0; }
    if (!Win64_Render_SelectAdapterAndCreateDevice ()) { return 0; }
    if (!Win64_Render_CreateCommandInfrastructure ())    { return 0; }
    if (!Win64_Render_CreateSwapchain (window))           { return 0; }
    if (!Win64_Render_CreateFence ())                     { return 0; }

    gWin64RenderInitialized = RE_True;

    return RE_True;
}

void
Win64_Render_Draw (void)
{
    ReUint32 bufferIndex = gWin64RenderSwapChain->lpVtbl->GetCurrentBackBufferIndex (gWin64RenderSwapChain);

    Win64_Render_WaitForFrame (bufferIndex);

    ID3D12CommandAllocator *allocator = gWin64RenderCommandAllocators[bufferIndex];
    allocator->lpVtbl->Reset (allocator);
    gWin64RenderCommandList->lpVtbl->Reset (gWin64RenderCommandList, allocator, NULL);

    D3D12_RESOURCE_BARRIER toRenderTarget = Win64_Render_TransitionBarrier (
        gWin64RenderBackBuffers[bufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    gWin64RenderCommandList->lpVtbl->ResourceBarrier (gWin64RenderCommandList, 1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = Win64_Render_RtvHandle (bufferIndex);
    gWin64RenderCommandList->lpVtbl->OMSetRenderTargets (gWin64RenderCommandList, 1, &rtv, FALSE, NULL);

    /* Deliberately not black - black is indistinguishable from "nothing rendered" at a glance. */
    local_persist const ReFloat32 clearColor[4] = {0.392f, 0.584f, 0.929f, 1.0f};
    gWin64RenderCommandList->lpVtbl->ClearRenderTargetView (gWin64RenderCommandList, rtv, clearColor, 0, NULL);

    D3D12_RESOURCE_BARRIER toPresent = Win64_Render_TransitionBarrier (
        gWin64RenderBackBuffers[bufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    gWin64RenderCommandList->lpVtbl->ResourceBarrier (gWin64RenderCommandList, 1, &toPresent);

    gWin64RenderCommandList->lpVtbl->Close (gWin64RenderCommandList);

    ID3D12CommandList *lists[] = {(ID3D12CommandList *) gWin64RenderCommandList};
    gWin64RenderCommandQueue->lpVtbl->ExecuteCommandLists (gWin64RenderCommandQueue, 1, lists);

    gWin64RenderSwapChain->lpVtbl->Present (gWin64RenderSwapChain, 1, 0);

    ReUint64 signalValue = gWin64RenderNextFenceValue;
    gWin64RenderCommandQueue->lpVtbl->Signal (gWin64RenderCommandQueue, gWin64RenderFence, signalValue);
    gWin64RenderFrameFenceValues[bufferIndex] = signalValue;
    gWin64RenderNextFenceValue += 1;
}

void
Win64_Render_NotifyResize (ReUint32 width, ReUint32 height)
{
    gWin64RenderPendingWidth  = width;
    gWin64RenderPendingHeight = height;
    gWin64RenderResizePending = RE_True;
}

void
Win64_Render_ProcessResize (void)
{
    if (!gWin64RenderInitialized || !gWin64RenderResizePending)
    {
        return;
    }

    gWin64RenderResizePending = RE_False;

    if (gWin64RenderPendingWidth == 0 || gWin64RenderPendingHeight == 0)
    {
        /* Window minimized - WM_SIZE reports 0,0. Nothing to resize to; keep the last valid
         * swapchain size.
         */
        return;
    }

    Win64_Render_FlushGpu ();

    for (ReUint32 i = 0; i < WIN64_RENDER_FRAME_COUNT; i += 1)
    {
        gWin64RenderBackBuffers[i]->lpVtbl->Release (gWin64RenderBackBuffers[i]);
        gWin64RenderBackBuffers[i]      = NULL;
        gWin64RenderFrameFenceValues[i] = 0;
    }

    gWin64RenderSwapChain->lpVtbl->ResizeBuffers (
        gWin64RenderSwapChain, WIN64_RENDER_FRAME_COUNT,
        gWin64RenderPendingWidth, gWin64RenderPendingHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0);

    Win64_Render_RecreateRtvs ();
}

void
Win64_Render_Shutdown (void)
{
    if (!gWin64RenderInitialized)
    {
        return;
    }

    Win64_Render_FlushGpu ();

    gWin64RenderCommandList->lpVtbl->Release (gWin64RenderCommandList);

    for (ReUint32 i = 0; i < WIN64_RENDER_FRAME_COUNT; i += 1)
    {
        gWin64RenderCommandAllocators[i]->lpVtbl->Release (gWin64RenderCommandAllocators[i]);
        gWin64RenderBackBuffers[i]->lpVtbl->Release (gWin64RenderBackBuffers[i]);
    }

    CloseHandle (gWin64RenderFenceEvent);
    gWin64RenderFence->lpVtbl->Release (gWin64RenderFence);

    gWin64RenderRtvHeap->lpVtbl->Release (gWin64RenderRtvHeap);
    gWin64RenderSwapChain->lpVtbl->Release (gWin64RenderSwapChain);
    gWin64RenderCommandQueue->lpVtbl->Release (gWin64RenderCommandQueue);
    gWin64RenderDevice->lpVtbl->Release (gWin64RenderDevice);
    gWin64RenderAdapter->lpVtbl->Release (gWin64RenderAdapter);
    gWin64RenderFactory->lpVtbl->Release (gWin64RenderFactory);
}
