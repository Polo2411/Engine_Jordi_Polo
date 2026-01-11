#pragma once

#include "Module.h"
#include "DeferredFreeHandleManager.h"
#include "ShaderTableDesc.h"

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class ModuleShaderDescriptors : public Module
{
    friend class ShaderTableDesc;

public:
    ModuleShaderDescriptors() = default;
    ~ModuleShaderDescriptors() override;

    bool init() override;
    bool cleanUp() override;

    void preRender() override;

    ID3D12DescriptorHeap* getHeap() const { return heap.Get(); }

    ShaderTableDesc allocTable();

private:
    uint32_t alloc() { return handles.allocHandle(); }
    void release(uint32_t handle) { if (handle) handles.freeHandle(handle); }

    void deferRelease(uint32_t handle);
    void collectGarbage();

    bool isValid(uint32_t handle) const { return handles.validHandle(handle); }
    uint32_t indexFromHandle(uint32_t handle) const { return handles.indexFromHandle(handle); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t handle, uint8_t slot) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t handle, uint8_t slot) const;

private:
    static constexpr uint32_t NUM_TABLES = 4096;
    static constexpr uint32_t DESCRIPTORS_PER_TABLE = 8;
    static constexpr uint32_t NUM_DESCRIPTORS = NUM_TABLES * DESCRIPTORS_PER_TABLE;

    ComPtr<ID3D12DescriptorHeap> heap;

    DeferredFreeHandleManager<NUM_TABLES> handles;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{ 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{ 0 };
    uint32_t descriptorSize = 0;

    std::array<uint32_t, NUM_TABLES> refCounts{};
};
