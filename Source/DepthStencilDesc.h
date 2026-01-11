#pragma once

#include <cstdint>
#include <d3d12.h>

class DepthStencilDesc
{
    uint32_t  handle = 0;
    uint32_t* refCount = nullptr;

public:
    DepthStencilDesc() = default;
    DepthStencilDesc(uint32_t handle, uint32_t* refCount);
    DepthStencilDesc(const DepthStencilDesc& other);
    DepthStencilDesc(DepthStencilDesc&& other) noexcept;
    ~DepthStencilDesc();

    DepthStencilDesc& operator=(const DepthStencilDesc& other);
    DepthStencilDesc& operator=(DepthStencilDesc&& other) noexcept;

    explicit operator bool() const;

    void reset() { release(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const;

private:
    void release();
    void addRef();
};
