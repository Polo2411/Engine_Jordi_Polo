#pragma once

#include "Module.h"
#include "HandleManager.h"
#include "RenderTargetDesc.h"
#include "DepthStencilDesc.h"

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class ModuleTargetDescriptors : public Module
{
    friend class RenderTargetDesc;
    friend class DepthStencilDesc;

public:
    ModuleTargetDescriptors() = default;
    ~ModuleTargetDescriptors() override;

    bool init() override;
    bool cleanUp() override;

    RenderTargetDesc createRT(ID3D12Resource* resource);
    RenderTargetDesc createRT(ID3D12Resource* resource, UINT arraySlice, UINT mipSlice, DXGI_FORMAT format);
    DepthStencilDesc createDS(ID3D12Resource* resource);

private:
    void releaseRT(uint32_t handle);
    void releaseDS(uint32_t handle);

    bool isValidRT(uint32_t handle) const { return rtHandles.validHandle(handle); }
    bool isValidDS(uint32_t handle) const { return dsHandles.validHandle(handle); }

    uint32_t indexFromRT(uint32_t handle) const { return rtHandles.indexFromHandle(handle); }
    uint32_t indexFromDS(uint32_t handle) const { return dsHandles.indexFromHandle(handle); }

    D3D12_CPU_DESCRIPTOR_HANDLE getRTCPUHandle(uint32_t handle) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDSCPUHandle(uint32_t handle) const;

private:
    static constexpr uint32_t MAX_RTV = 256;
    static constexpr uint32_t MAX_DSV = 128;

    ComPtr<ID3D12DescriptorHeap> heapRTV;
    ComPtr<ID3D12DescriptorHeap> heapDSV;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvStart{ 0 };
    D3D12_CPU_DESCRIPTOR_HANDLE dsvStart{ 0 };

    uint32_t rtvInc = 0;
    uint32_t dsvInc = 0;

    HandleManager<MAX_RTV> rtHandles;
    HandleManager<MAX_DSV> dsHandles;

    std::array<uint32_t, MAX_RTV> rtRefCounts{};
    std::array<uint32_t, MAX_DSV> dsRefCounts{};
};
