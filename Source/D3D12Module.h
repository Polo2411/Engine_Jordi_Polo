#pragma once

#include "Module.h"
#include "ShaderTableDesc.h"

#include <dxgi1_6.h>
#include <cstdint>
#include <algorithm>

class ImGuiPass;

class D3D12Module : public Module
{
public:
    static constexpr UINT kBufferCount = 2;

public:
    D3D12Module(HWND wnd);
    ~D3D12Module();

    bool init() override;
    bool cleanUp() override;

    void preRender() override;
    void postRender() override;

    void flush();
    void resize();

    void setMinimized(bool v) { minimized = v; }
    bool isMinimized() const { return minimized; }

    ID3D12Device* getDevice() { return device.Get(); }
    ID3D12GraphicsCommandList* getCommandList() { return commandList.Get(); }
    ID3D12CommandAllocator* getCommandAllocator() { return commandAllocators[currentBackBufferIdx].Get(); }
    ID3D12Resource* getBackBuffer() { return backBuffers[currentBackBufferIdx].Get(); }
    ID3D12CommandQueue* getDrawCommandQueue() { return drawCommandQueue.Get(); }

    unsigned getWindowWidth() const { return windowWidth; }
    unsigned getWindowHeight() const { return windowHeight; }

    D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilDescriptor();

    ImGuiPass* getImGuiPass() const { return imgui.get(); }

    UINT signalDrawQueue();

    unsigned getCurrentFrame() const { return frameIndex; }
    unsigned getLastCompletedFrame() const { return lastCompletedFrame; }

    UINT getCurrentBackBufferIndex() const { return currentBackBufferIdx; }

    void bindShaderVisibleHeaps(ID3D12GraphicsCommandList* cmdList);

    // NEW: create ImGui after ModuleShaderDescriptors exists
    void initImGui();

private:
    void getWindowSize(unsigned& width, unsigned& height);

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

    unsigned frameValues[kBufferCount] = {};
    unsigned frameIndex = 0;
    unsigned lastCompletedFrame = 0;

    unsigned currentBackBufferIdx = 0;

    unsigned windowWidth = 0;
    unsigned windowHeight = 0;

    std::unique_ptr<ImGuiPass> imgui;
    ShaderTableDesc imguiDescTable;

};
