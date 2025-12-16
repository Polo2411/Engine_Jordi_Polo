#pragma once

#include "Module.h"

#include <dxgi1_6.h>
#include <cstdint>
#include <algorithm> // std::max

class ImGuiPass;

class D3D12Module : public Module
{
public:
    static constexpr UINT kBufferCount = 2; // si en tu proyecto ya existe, quita esta línea

public:
    D3D12Module(HWND wnd);
    ~D3D12Module();

    bool init() override;
    bool cleanUp() override;

    void preRender() override;
    void postRender() override;

    void flush();
    void resize();

    // --- Getters DX12 ---
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

    // --- Fence / sync ---
    UINT signalDrawQueue();

    // --- Frame tracking (para deferred free / GC por frame) ---
    unsigned getCurrentFrame() const { return frameIndex; }
    unsigned getLastCompletedFrame() const { return lastCompletedFrame; }

    void bindShaderVisibleHeaps(ID3D12GraphicsCommandList* cmdList);


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
    UINT drawFenceValues[kBufferCount] = {}; // fence value que corresponde al último frame enviado en ese backbuffer

    // --- NUEVO: tracking de frames por backbuffer ---
    unsigned frameValues[kBufferCount] = {};   // frameIndex asociado al último uso de ese backbuffer
    unsigned frameIndex = 0;                   // contador de frames (CPU)
    unsigned lastCompletedFrame = 0;           // último frame completado (seguro para liberar)

    unsigned currentBackBufferIdx = 0;

    unsigned windowWidth = 0;
    unsigned windowHeight = 0;

    std::unique_ptr<ImGuiPass> imgui;
};
