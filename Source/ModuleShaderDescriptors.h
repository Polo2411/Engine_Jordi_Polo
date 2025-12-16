#pragma once

#include "Module.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class ModuleShaderDescriptors : public Module
{
public:
    ModuleShaderDescriptors() = default;
    ~ModuleShaderDescriptors() override = default;

    bool init() override;
    bool cleanUp() override;

    ID3D12DescriptorHeap* getHeap() const { return heap.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t index) const;
    uint32_t allocate();
    uint32_t createSRV(ID3D12Resource* texture);

    void reset();

private:
    ComPtr<ID3D12DescriptorHeap> heap;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{ 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{ 0 };
    uint32_t descriptorSize = 0;

    static constexpr uint32_t MAX_DESCRIPTORS = 256;
    uint32_t nextFreeIndex = 0;

};
