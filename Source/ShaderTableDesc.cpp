#include "Globals.h"
#include "ShaderTableDesc.h"

#include "Application.h"
#include "ModuleShaderDescriptors.h"
#include "D3D12Module.h"

#include <cassert>

namespace
{
    static inline uint32_t AlignUp(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1u) & ~(alignment - 1u);
    }
}

ShaderTableDesc::ShaderTableDesc(uint32_t h, uint32_t* rc) : handle(h), refCount(rc)
{
    addRef();
}

ShaderTableDesc::ShaderTableDesc(const ShaderTableDesc& other) : handle(other.handle), refCount(other.refCount)
{
    addRef();
}

ShaderTableDesc::ShaderTableDesc(ShaderTableDesc&& other) noexcept : handle(other.handle), refCount(other.refCount)
{
    other.handle = 0;
    other.refCount = nullptr;
}

ShaderTableDesc::~ShaderTableDesc()
{
    release();
}

ShaderTableDesc& ShaderTableDesc::operator=(const ShaderTableDesc& other)
{
    if (this != &other)
    {
        release();
        handle = other.handle;
        refCount = other.refCount;
        addRef();
    }
    return *this;
}

ShaderTableDesc& ShaderTableDesc::operator=(ShaderTableDesc&& other) noexcept
{
    if (this != &other)
    {
        release();
        handle = other.handle;
        refCount = other.refCount;
        other.handle = 0;
        other.refCount = nullptr;
    }
    return *this;
}

ShaderTableDesc::operator bool() const
{
    ModuleShaderDescriptors* descriptors = app ? app->getShaderDescriptors() : nullptr;
    return descriptors && handle != 0 && descriptors->isValid(handle);
}

void ShaderTableDesc::release()
{
    // Always clear local state (this object is being reset/destroyed)
    const uint32_t h = handle;
    uint32_t* rc = refCount;

    handle = 0;
    refCount = nullptr;

    if (!h || !rc)
        return;

    // Defensive: avoid underflow if something went wrong
    if (*rc == 0)
        return;

    // Decrement and release only on last ref
    --(*rc);
    if (*rc == 0)
    {
        ModuleShaderDescriptors* descriptors = app ? app->getShaderDescriptors() : nullptr;
        if (descriptors)
            descriptors->deferRelease(h);
    }
}

void ShaderTableDesc::addRef()
{
    if (refCount)
        ++(*refCount);
}

void ShaderTableDesc::createCBV(ID3D12Resource* resource, uint8_t slot)
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;

    _ASSERTE(descriptors && device);
    _ASSERTE(descriptors->isValid(handle));
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);

    D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
    if (resource)
    {
        const D3D12_RESOURCE_DESC desc = resource->GetDesc();
        _ASSERTE(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

        viewDesc.BufferLocation = resource->GetGPUVirtualAddress();
        viewDesc.SizeInBytes = AlignUp(uint32_t(desc.Width), 256u);
    }

    device->CreateConstantBufferView(&viewDesc, descriptors->getCPUHandle(handle, slot));
}

void ShaderTableDesc::createTextureSRV(ID3D12Resource* resource, uint8_t slot)
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;

    _ASSERTE(descriptors && device);
    _ASSERTE(descriptors->isValid(handle));
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);

    if (!resource)
    {
        createNullTexture2DSRV(slot);
        return;
    }

    device->CreateShaderResourceView(resource, nullptr, descriptors->getCPUHandle(handle, slot));
}

void ShaderTableDesc::createTexture2DSRV(ID3D12Resource* resource, uint32_t arraySlice, uint32_t mipSlice, uint8_t slot)
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;

    _ASSERTE(descriptors && device);
    _ASSERTE(descriptors->isValid(handle));
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);

    if (!resource)
    {
        createNullTexture2DSRV(slot);
        return;
    }

    const D3D12_RESOURCE_DESC desc = resource->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.Format = desc.Format;
    viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    viewDesc.Texture2DArray.MostDetailedMip = mipSlice;
    viewDesc.Texture2DArray.MipLevels = 1;
    viewDesc.Texture2DArray.FirstArraySlice = arraySlice;
    viewDesc.Texture2DArray.ArraySize = 1;
    viewDesc.Texture2DArray.PlaneSlice = 0;
    viewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

    device->CreateShaderResourceView(resource, &viewDesc, descriptors->getCPUHandle(handle, slot));
}

void ShaderTableDesc::createTexture2DUAV(ID3D12Resource* resource, uint32_t arraySlice, uint32_t mipSlice, uint8_t slot)
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;

    _ASSERTE(descriptors && device);
    _ASSERTE(descriptors->isValid(handle));
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);

    if (!resource)
        return;

    const D3D12_RESOURCE_DESC desc = resource->GetDesc();

    D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
    viewDesc.Format = desc.Format;
    viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    viewDesc.Texture2DArray.MipSlice = mipSlice;
    viewDesc.Texture2DArray.FirstArraySlice = arraySlice;
    viewDesc.Texture2DArray.ArraySize = 1;
    viewDesc.Texture2DArray.PlaneSlice = 0;

    device->CreateUnorderedAccessView(resource, nullptr, &viewDesc, descriptors->getCPUHandle(handle, slot));
}

void ShaderTableDesc::createCubeTextureSRV(ID3D12Resource* resource, uint8_t slot)
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;

    _ASSERTE(descriptors && device);
    _ASSERTE(descriptors->isValid(handle));
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);

    if (!resource)
    {
        createNullTexture2DSRV(slot);
        return;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = resource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = UINT(-1);
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    device->CreateShaderResourceView(resource, &srvDesc, descriptors->getCPUHandle(handle, slot));
}

void ShaderTableDesc::createNullTexture2DSRV(uint8_t slot)
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;

    _ASSERTE(descriptors && device);
    _ASSERTE(descriptors->isValid(handle));
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    device->CreateShaderResourceView(nullptr, &srvDesc, descriptors->getCPUHandle(handle, slot));
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderTableDesc::getGPUHandle(uint8_t slot) const
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    _ASSERTE(descriptors);
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);
    return descriptors->getGPUHandle(handle, slot);
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTableDesc::getCPUHandle(uint8_t slot) const
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    _ASSERTE(descriptors);
    _ASSERTE(slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE);
    return descriptors->getCPUHandle(handle, slot);
}
