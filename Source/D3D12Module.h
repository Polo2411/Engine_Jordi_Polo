#pragma once

#include "Module.h"

#include <dxgi1_6.h>
#include <cstdint>
#include <algorithm> // std::max

class ImGuiPass;

// Core Direct3D 12 renderer module (device, swapchain, frame sync)
class D3D12Module : public Module
{
public:
    // Number of swapchain backbuffers
    static constexpr UINT kBufferCount = 2;

public:
    D3D12Module(HWND wnd);
    ~D3D12Module();

    // Module lifecycle
    bool init() override;
    bool cleanUp() override;

    // Per-frame entry points
    void preRender() override;
    void postRender() override;

    // GPU synchronization helpers
    void flush();
    void resize();

    // Minimized window handling
    void setMinimized(bool v) { minimized = v; }
    bool isMinimized() const { return minimized; }

    // --- D3D12 accessors ---
    ID3D12Device* getDevice() { return device.Get(); }
    ID3D12GraphicsCommandList* getCommandList() { return commandList.Get(); }
    ID3D12CommandAllocator* getCommandAllocator() { return commandAllocators[currentBackBufferIdx].Get(); }
    ID3D12Resource* getBackBuffer() { return backBuffers[currentBackBufferIdx].Get(); }
    ID3D12CommandQueue* getDrawCommandQueue() { return drawCommandQueue.Get(); }

    unsigned getWindowWidth() const { return windowWidth; }
    unsigned getWindowHeight() const { return windowHeight; }

    // Current render target / depth-stencil descriptors
    D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilDescriptor();

    ImGuiPass* getImGuiPass() const { return imgui.get(); }

    // --- Fence / frame sync ---
    UINT signalDrawQueue();

    // Frame indices for deferred resource management
    unsigned getCurrentFrame() const { return frameIndex; }
    unsigned getLastCompletedFrame() const { return lastCompletedFrame; }

    // Needed by RingBuffer to reclaim per-frame allocations safely
    UINT getCurrentBackBufferIndex() const { return currentBackBufferIdx; }

    // Bind shader-visible descriptor heaps (CBV/SRV/UAV + samplers)
    void bindShaderVisibleHeaps(ID3D12GraphicsCommandList* cmdList);

private:
    // Window size query
    void getWindowSize(unsigned& width, unsigned& height);

    // D3D12 initialization helpers
    void enableDebugLayer();
    bool createFactory();
    bool createDevice(bool useWarp);
    bool setupInfoQueue();
    bool createDrawCommandQueue();
    bool createSwapChain();
    bool createRenderTargets();
    bool createDepthStencil();
    bool createCommandList();
    bool createDrawFence();

private:
    HWND hWnd = nullptr;

    bool minimized = false;

    ComPtr<IDXGIFactory6> factory;
    ComPtr<ID3D12Device> device;

    ComPtr<IDXGISwapChain4> swapChain;
    ComPtr<ID3D12DescriptorHeap> rtDescriptorHeap;
    ComPtr<ID3D12Resource> backBuffers[kBufferCount];

    ComPtr<ID3D12DescriptorHeap> dsDescriptorHeap;
    ComPtr<ID3D12Resource> depthStencilBuffer;

    ComPtr<ID3D12CommandAllocator> commandAllocators[kBufferCount];
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12CommandQueue> drawCommandQueue;

    ComPtr<ID3D12Fence> drawFence;
    HANDLE drawEvent = nullptr;

    UINT drawFenceCounter = 0;
    UINT drawFenceValues[kBufferCount] = {};

    // Per-backbuffer frame tracking
    unsigned frameValues[kBufferCount] = {};
    unsigned frameIndex = 0;
    unsigned lastCompletedFrame = 0;

    unsigned currentBackBufferIdx = 0;

    unsigned windowWidth = 0;
    unsigned windowHeight = 0;

    // ImGui renderer pass
    std::unique_ptr<ImGuiPass> imgui;
};
