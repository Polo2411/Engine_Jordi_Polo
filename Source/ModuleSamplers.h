#pragma once

#include "Module.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class ModuleSamplers : public Module
{
public:
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

    bool init() override;
    bool cleanUp() override;

    ID3D12DescriptorHeap* getHeap() const { return heap.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(Type type) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(Type type) const;

private:
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{ 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{ 0 };
    uint32_t descriptorSize = 0;
};
