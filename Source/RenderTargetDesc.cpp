#include "Globals.h"
#include "RenderTargetDesc.h"

#include "Application.h"
#include "ModuleTargetDescriptors.h"

RenderTargetDesc::RenderTargetDesc(uint32_t h, uint32_t* rc) : handle(h), refCount(rc)
{
    addRef();
}

RenderTargetDesc::RenderTargetDesc(const RenderTargetDesc& other) : handle(other.handle), refCount(other.refCount)
{
    addRef();
}

RenderTargetDesc::RenderTargetDesc(RenderTargetDesc&& other) noexcept : handle(other.handle), refCount(other.refCount)
{
    other.handle = 0;
    other.refCount = nullptr;
}

RenderTargetDesc::~RenderTargetDesc()
{
    release();
}

RenderTargetDesc& RenderTargetDesc::operator=(const RenderTargetDesc& other)
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

RenderTargetDesc& RenderTargetDesc::operator=(RenderTargetDesc&& other) noexcept
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

RenderTargetDesc::operator bool() const
{
    ModuleTargetDescriptors* targets = app->getTargetDescriptors();
    return targets && targets->isValidRT(handle);
}

void RenderTargetDesc::release()
{
    if (refCount && --(*refCount) == 0)
    {
        ModuleTargetDescriptors* targets = app->getTargetDescriptors();
        if (targets)
            targets->releaseRT(handle);

        handle = 0;
        refCount = nullptr;
    }
}

void RenderTargetDesc::addRef()
{
    if (refCount)
        ++(*refCount);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetDesc::getCPUHandle() const
{
    ModuleTargetDescriptors* targets = app->getTargetDescriptors();
    _ASSERTE(targets);
    return targets->getRTCPUHandle(handle);
}
