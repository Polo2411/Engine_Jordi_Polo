#pragma once

#include <cstdint>
#include <d3d12.h>

class RenderTargetDesc
{
    uint32_t  handle = 0;
    uint32_t* refCount = nullptr;

public:
    RenderTargetDesc() = default;
    RenderTargetDesc(uint32_t handle, uint32_t* refCount);
    RenderTargetDesc(const RenderTargetDesc& other);
    RenderTargetDesc(RenderTargetDesc&& other) noexcept;
    ~RenderTargetDesc();

    RenderTargetDesc& operator=(const RenderTargetDesc& other);
    RenderTargetDesc& operator=(RenderTargetDesc&& other) noexcept;

    explicit operator bool() const;

    void reset() { release(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const;

private:
    void release();
    void addRef();
};
