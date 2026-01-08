#pragma once

#include "Module.h"

#include <d3d12.h>
#include <wrl.h>
#include <cstddef>
#include <cstdint>

class ModuleRingBuffer : public Module
{
public:
    ModuleRingBuffer() = default;
    ~ModuleRingBuffer() override = default;

    bool init() override;
    bool cleanUp() override;
    void preRender() override;

    template<typename T>
    D3D12_GPU_VIRTUAL_ADDRESS allocBuffer(const T* data)
    {
        if (!data) return 0;
        const size_t sz = alignUp(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        return allocBufferRaw(data, sz);
    }

    template<typename T>
    D3D12_GPU_VIRTUAL_ADDRESS allocBuffer(const T* data, size_t count)
    {
        if (!data || count == 0) return 0;
        const size_t sz = alignUp(sizeof(T) * count, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        return allocBufferRaw(data, sz);
    }

private:
    static size_t alignUp(size_t v, size_t a) { return (v + (a - 1)) & ~(a - 1); }
    D3D12_GPU_VIRTUAL_ADDRESS allocBufferRaw(const void* data, size_t size);

private:
    static constexpr size_t kDefaultTotalSizeBytes = size_t(10) * size_t(1 << 20); // 10 MB

    uint8_t* bufferData = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

    size_t totalMemorySize = 0;
    size_t head = 0; // next free write position
    size_t tail = 0; // reclaimed up to here

    static constexpr unsigned kFramesInFlight = 2; // must match swapchain buffer count
    size_t allocatedInFrame[kFramesInFlight] = {};
    size_t totalAllocated = 0;

    unsigned currentFrameIdx = 0;
};
