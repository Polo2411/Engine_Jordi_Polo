#include "Globals.h"
#include "DepthStencilDesc.h"

#include "Application.h"
#include "ModuleTargetDescriptors.h"

DepthStencilDesc::DepthStencilDesc(uint32_t h, uint32_t* rc) : handle(h), refCount(rc)
{
    addRef();
}

DepthStencilDesc::DepthStencilDesc(const DepthStencilDesc& other) : handle(other.handle), refCount(other.refCount)
{
    addRef();
}

DepthStencilDesc::DepthStencilDesc(DepthStencilDesc&& other) noexcept : handle(other.handle), refCount(other.refCount)
{
    other.handle = 0;
    other.refCount = nullptr;
}

DepthStencilDesc::~DepthStencilDesc()
{
    release();
}

DepthStencilDesc& DepthStencilDesc::operator=(const DepthStencilDesc& other)
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

DepthStencilDesc& DepthStencilDesc::operator=(DepthStencilDesc&& other) noexcept
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

DepthStencilDesc::operator bool() const
{
    ModuleTargetDescriptors* targets = app->getTargetDescriptors();
    return targets && targets->isValidDS(handle);
}

void DepthStencilDesc::release()
{
    if (refCount && --(*refCount) == 0)
    {
        ModuleTargetDescriptors* targets = app->getTargetDescriptors();
        if (targets)
            targets->releaseDS(handle);

        handle = 0;
        refCount = nullptr;
    }
}

void DepthStencilDesc::addRef()
{
    if (refCount)
        ++(*refCount);
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilDesc::getCPUHandle() const
{
    ModuleTargetDescriptors* targets = app->getTargetDescriptors();
    _ASSERTE(targets);
    return targets->getDSCPUHandle(handle);
}
