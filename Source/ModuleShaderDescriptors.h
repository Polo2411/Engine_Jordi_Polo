#pragma once

#include "Module.h"
#include <cstdint>

class ModuleShaderDescriptors : public Module
{
public:
    ModuleShaderDescriptors() = default;
    ~ModuleShaderDescriptors() override = default;

    bool init() override;
    bool cleanUp() override;

    // --- Stub API (no funcional todavía en el commit 1.1) ---
    ID3D12DescriptorHeap* getHeap() const { return nullptr; }

    uint32_t alloc() { return 0u; }
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t /*index*/) const { return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 }; }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t /*index*/) const { return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 }; }
};
