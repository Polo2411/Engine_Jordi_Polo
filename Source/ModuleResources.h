#pragma once

#include "Module.h"
#include "Globals.h"
#include <string>

// Helper module to create buffers (UPLOAD / DEFAULT) and copy data.
class ModuleResources : public Module
{
public:
    ModuleResources();
    ~ModuleResources() override;

    // --- Module overrides ---
    bool init() override;
    void preRender() override {}        // currently unused
    void render() override {}           // currently unused
    void postRender() override {}       // currently unused
    bool cleanUp() override;

    // Creates an UPLOAD heap buffer (CPU-writable) and fills it with cpuData
    ComPtr<ID3D12Resource> createUploadBuffer(const void* cpuData,
        size_t      dataSize,
        const char* name = nullptr);

    // Creates a DEFAULT heap buffer (VRAM), uploads via staging (UPLOAD), and flushes
    ComPtr<ID3D12Resource> createDefaultBuffer(const void* cpuData,
        size_t      dataSize,
        const char* name = nullptr);

    // Loads a texture from disk (DDS/TGA/WIC) and returns it ready for sampling in shaders.
    ComPtr<ID3D12Resource> createTextureFromFile(const std::wstring& filePath, const wchar_t* debugName = nullptr);

private:
    void FlushCopyQueue();

private:
    ComPtr<ID3D12Device>              m_device;
    ComPtr<ID3D12CommandQueue>        m_queue;

    ComPtr<ID3D12CommandAllocator>    m_allocator;
    ComPtr<ID3D12GraphicsCommandList> m_cmdList;

    ComPtr<ID3D12Fence>               m_fence;
    UINT64                            m_fenceValue = 0;
    HANDLE                            m_fenceEvent = nullptr;
};
