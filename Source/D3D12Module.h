#pragma once

#include "Module.h"
#include "Globals.h"

class ImGuiPass;   // <--- forward declaration

class D3D12Module : public Module
{
public:
    explicit D3D12Module(HWND hWnd);
    ~D3D12Module() override;

    bool init() override;
    void preRender() override;
    void render() override {}            // NO dibuja nada
    void postRender() override;
    bool cleanUp() override;

    // --- Getters estilo profe, para los ejercicios ---
    ID3D12Device5* getDevice() { return device.Get(); }
    ID3D12GraphicsCommandList* getCommandList() { return commandList.Get(); }
    ID3D12CommandAllocator* getCommandAllocator() { return commandAllocators[currentBackBufferIdx].Get(); }
    ID3D12CommandQueue* getDrawCommandQueue() { return drawCommandQueue.Get(); }
    ID3D12Resource* getBackBuffer() { return backBuffers[currentBackBufferIdx].Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilDescriptor();

    UINT getWindowWidth()  const { return windowWidth; }
    UINT getWindowHeight() const { return windowHeight; }

    void flush();
    void resize();

    // 🔹 acceso al ImGuiPass
    ImGuiPass* getImGuiPass() { return imgui.get(); }

private:
    // ---- helpers internos ----
    void  enableDebugLayer();
    bool  createFactory();
    bool  createDevice(bool useWarp);
    bool  setupInfoQueue();
    bool  createDrawCommandQueue();
    bool  createSwapChain();
    bool  createRenderTargets();
    bool  createDepthStencil(); 
    bool  createCommandList();
    bool  createDrawFence();
    void  getWindowSize(unsigned& width, unsigned& height);

    UINT  signalDrawQueue();

private:
    HWND                            hWnd = nullptr;

    ComPtr<IDXGIFactory7>           factory;
    ComPtr<ID3D12Device5>           device;

    ComPtr<ID3D12CommandQueue>      drawCommandQueue;
    ComPtr<IDXGISwapChain4>         swapChain;

    static constexpr UINT           kBufferCount = FRAMES_IN_FLIGHT;
    ComPtr<ID3D12Resource>          backBuffers[kBufferCount];
    ComPtr<ID3D12DescriptorHeap>    rtDescriptorHeap;

    // 👇 NUEVO: depth buffer + heap DSV
    ComPtr<ID3D12Resource>          depthStencilBuffer;
    ComPtr<ID3D12DescriptorHeap>    dsDescriptorHeap;

    ComPtr<ID3D12CommandAllocator>      commandAllocators[kBufferCount];
    ComPtr<ID3D12GraphicsCommandList>   commandList;

    ComPtr<ID3D12Fence>             drawFence;
    HANDLE                          drawEvent = nullptr;
    UINT64                          drawFenceValues[kBufferCount] = {};
    UINT64                          drawFenceCounter = 0;

    UINT                            currentBackBufferIdx = 0;
    UINT                            windowWidth = 0;
    UINT                            windowHeight = 0;

    // 🔹 responsable de ImGui (heap SRV + backends DX12/Win32)
    std::unique_ptr<ImGuiPass>      imgui;
};
