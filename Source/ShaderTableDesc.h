#pragma once

#include <cstdint>
#include <d3d12.h>

struct ID3D12Resource;

class ShaderTableDesc
{
    uint32_t  handle = 0;
    uint32_t* refCount = nullptr;

public:
    ShaderTableDesc() = default;
    ShaderTableDesc(uint32_t handle, uint32_t* refCount);
    ShaderTableDesc(const ShaderTableDesc& other);
    ShaderTableDesc(ShaderTableDesc&& other) noexcept;
    ~ShaderTableDesc();

    ShaderTableDesc& operator=(const ShaderTableDesc& other);
    ShaderTableDesc& operator=(ShaderTableDesc&& other) noexcept;

    explicit operator bool() const;

    void createCBV(ID3D12Resource* resource, uint8_t slot = 0);
    void createTextureSRV(ID3D12Resource* resource, uint8_t slot = 0);
    void createTexture2DSRV(ID3D12Resource* resource, uint32_t arraySlice, uint32_t mipSlice, uint8_t slot = 0);
    void createTexture2DUAV(ID3D12Resource* resource, uint32_t arraySlice, uint32_t mipSlice, uint8_t slot = 0);
    void createCubeTextureSRV(ID3D12Resource* resource, uint8_t slot = 0);
    void createNullTexture2DSRV(uint8_t slot = 0);

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint8_t slot = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint8_t slot = 0) const;

    void reset() { release(); }

private:
    void release();
    void addRef();
};
