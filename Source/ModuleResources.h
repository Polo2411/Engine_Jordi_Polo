#pragma once

#include "Module.h"
#include "Globals.h"

#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class ModuleResources : public Module
{
public:
    ModuleResources();
    ~ModuleResources() override;

    bool init() override;
    void preRender() override;
    bool cleanUp() override;

    ComPtr<ID3D12Resource> createUploadBuffer(
        const void* cpuData,
        size_t dataSize,
        const char* debugName = nullptr);

    ComPtr<ID3D12Resource> createUploadBuffer(
        const void* cpuData,
        size_t dataSize,
        const wchar_t* debugNameW);

    ComPtr<ID3D12Resource> createDefaultBuffer(
        const void* cpuData,
        size_t dataSize,
        const char* debugName = nullptr);

    ComPtr<ID3D12Resource> createDefaultBuffer(
        const void* cpuData,
        size_t dataSize,
        const wchar_t* debugNameW);

    ComPtr<ID3D12Resource> createTextureFromFile(
        const std::wstring& filePath,
        const wchar_t* debugName = nullptr);

    // Render-to-texture helpers (Exercise 7)
    ComPtr<ID3D12Resource> createRenderTarget(
        DXGI_FORMAT format,
        size_t width,
        size_t height,
        UINT sampleCount,
        const Vector4& clearColor,
        const char* debugName = nullptr);

    ComPtr<ID3D12Resource> createDepthStencil(
        DXGI_FORMAT format,
        size_t width,
        size_t height,
        UINT sampleCount,
        float clearDepth,
        UINT8 clearStencil,
        const char* debugName = nullptr);

    // Deferred release (powerpoint requirement)
    void deferRelease(ComPtr<ID3D12Resource>& resource);

private:
    void FlushCopyQueue();
    void collectGarbage();

    static std::wstring utf8ToWString(const char* s);
    static void setDebugName(ID3D12Object* obj, const wchar_t* nameW);
    static void setDebugName(ID3D12Object* obj, const char* nameUtf8);

private:
    struct DeferredResource
    {
        ComPtr<ID3D12Resource> resource;
        unsigned frame = 0;
    };

    std::vector<DeferredResource> deferred;

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;

    ComPtr<ID3D12CommandAllocator> m_allocator;
    ComPtr<ID3D12GraphicsCommandList> m_cmdList;

    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;
};
