#pragma once

#include <cstdint>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi.h>
#include <Windows.h>

class ImGuiPass
{
public:
    ImGuiPass(
        ID3D12Device* inDevice,
        HWND inHwnd,
        ID3D12DescriptorHeap* inSrvHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle,
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
        uint32_t framesInFlight = 2);

    ~ImGuiPass();

    ImGuiPass(const ImGuiPass&) = delete;
    ImGuiPass& operator=(const ImGuiPass&) = delete;

    void startFrame();
    void record(ID3D12GraphicsCommandList* cmdList);

private:
    ID3D12Device* device = nullptr;
    HWND hwnd = nullptr;

    // Borrowed pointer (owned by ModuleShaderDescriptors).
    ID3D12DescriptorHeap* srvHeap = nullptr;

    bool initialized = false;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t frames = 2;
};
