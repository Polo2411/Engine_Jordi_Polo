#pragma once

#include "Module.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

// Manages a shader-visible CBV/SRV/UAV descriptor heap with a simple linear allocator
class ModuleShaderDescriptors : public Module
{
public:
    ModuleShaderDescriptors() = default;
    ~ModuleShaderDescriptors() override = default;

    // Module lifecycle
    bool init() override;
    bool cleanUp() override;

    // Returns the underlying descriptor heap
    ID3D12DescriptorHeap* getHeap() const { return heap.Get(); }

    // CPU/GPU descriptor handles for a given heap index
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t index) const;

    // Allocates the next free descriptor slot
    uint32_t allocate();

    // Creates a basic SRV for a 2D texture and returns its descriptor index
    uint32_t createSRV(ID3D12Resource* texture);

    uint32_t createNullTexture2DSRV();
    uint32_t getNullTexture2DSrvIndex() const { return nullTexture2DSrvIndex; }

    // Resets the allocator (does not clear heap memory)
    void reset();

private:
    ComPtr<ID3D12DescriptorHeap> heap;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{ 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{ 0 };
    uint32_t descriptorSize = 0;

    static constexpr uint32_t MAX_DESCRIPTORS = 256;
    uint32_t nextFreeIndex = 0;
    uint32_t nullTexture2DSrvIndex = UINT32_MAX;
};
