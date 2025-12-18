#pragma once

#include "Module.h"
#include "Globals.h"
#include <string>

// Módulo de ayuda para crear buffers (UPLOAD / DEFAULT) y copiar datos.
class ModuleResources : public Module
{
public:
    ModuleResources();
    ~ModuleResources() override;

    // --- overrides de Module ---
    bool init() override;
    void preRender() override {}        // de momento no hacemos nada
    void render() override {}           // tampoco
    void postRender() override {}       // tampoco
    bool cleanUp() override;

    // Crea un buffer en HEAP UPLOAD (CPU-writable) y lo rellena con cpuData
    ComPtr<ID3D12Resource> createUploadBuffer(const void* cpuData,
        size_t      dataSize,
        const char* name = nullptr);

    // Crea un buffer en HEAP DEFAULT (VRAM), copia los datos vía staging (UPLOAD) y hace flush
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
