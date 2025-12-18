#pragma once

#include "Module.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

// Manages a shader-visible sampler descriptor heap with common sampler presets
class ModuleSamplers : public Module
{
public:
    // Predefined sampler types (filtering + address mode)
    enum class Type : uint32_t
    {
        Linear_Wrap = 0,
        Point_Wrap,
        Linear_Clamp,
        Point_Clamp,
        Linear_Mirror,
        Point_Mirror,
        Linear_Border,
        Point_Border,
        Count
    };

public:
    ModuleSamplers() = default;
    ~ModuleSamplers() override = default;

    // Module lifecycle
    bool init() override;
    bool cleanUp() override;

    // Returns the underlying sampler descriptor heap
    ID3D12DescriptorHeap* getHeap() const { return heap.Get(); }

    // CPU/GPU descriptor handles for a given sampler type
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(Type type) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(Type type) const;

private:
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{ 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{ 0 };
    uint32_t descriptorSize = 0;
};
