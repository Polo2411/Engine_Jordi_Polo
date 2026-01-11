#include "Globals.h"
#include "RenderTexture.h"

#include "Application.h"
#include "ModuleResources.h"
#include "ModuleTargetDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "D3D12Module.h"

#include "d3dx12.h"

namespace
{
    uint32_t ClampMin1(uint32_t v) { return v == 0u ? 1u : v; }
}

RenderTexture::RenderTexture(
    const char* inName,
    DXGI_FORMAT inColourFormat,
    const Vector4& inClearColour,
    DXGI_FORMAT inDepthFormat,
    float inClearDepth,
    bool inMSAA,
    bool inAutoResolveMSAA)
    : name(inName ? inName : "RenderTexture")
    , colourFormat(inColourFormat)
    , depthFormat(inDepthFormat)
    , clearColour(inClearColour)
    , clearDepth(inClearDepth)
    , msaa(inMSAA)
    , autoResolveMSAA(inAutoResolveMSAA)
{
    sampleCount = msaa ? 4u : 1u;

    if (msaa && !autoResolveMSAA)
        autoResolveMSAA = true;
}

RenderTexture::~RenderTexture()
{
    releaseResources();
}

void RenderTexture::releaseResources()
{
    if (app)
    {
        if (ModuleResources* resources = app->getResources())
        {
            if (texture)      resources->deferRelease(texture);
            if (resolved)     resources->deferRelease(resolved);
            if (depthTexture) resources->deferRelease(depthTexture);
        }
    }

    texture.Reset();
    resolved.Reset();
    depthTexture.Reset();

    width = 0;
    height = 0;

    textureState = D3D12_RESOURCE_STATE_COMMON;
    resolvedState = D3D12_RESOURCE_STATE_COMMON;

    srvDesc.reset();
    rtvDesc.reset();
    dsvDesc.reset();
}

void RenderTexture::resize(uint32_t newWidth, uint32_t newHeight)
{
    newWidth = ClampMin1(newWidth);
    newHeight = ClampMin1(newHeight);

    if (width == newWidth && height == newHeight && isValid())
        return;

    width = newWidth;
    height = newHeight;

    createResources(width, height);
    createDescriptors();
}

void RenderTexture::createResources(uint32_t newWidth, uint32_t newHeight)
{
    if (!app)
        return;

    ModuleResources* resources = app->getResources();
    if (!resources)
        return;

    if (texture)
        resources->deferRelease(texture);

    texture = resources->createRenderTarget(
        colourFormat,
        size_t(newWidth),
        size_t(newHeight),
        sampleCount,
        clearColour,
        name.c_str());

    textureState = D3D12_RESOURCE_STATE_COMMON;

    if (resolved)
        resources->deferRelease(resolved);

    resolved.Reset();
    resolvedState = D3D12_RESOURCE_STATE_COMMON;

    if (msaa && autoResolveMSAA)
    {
        const std::string resolvedName = name + "_resolved";

        resolved = resources->createRenderTarget(
            colourFormat,
            size_t(newWidth),
            size_t(newHeight),
            1u,
            clearColour,
            resolvedName.c_str());

        // El estado inicial, según el powerpoint, debe ser COMMON.
        resolvedState = D3D12_RESOURCE_STATE_COMMON;
    }

    if (depthTexture)
        resources->deferRelease(depthTexture);

    depthTexture.Reset();

    if (depthFormat != DXGI_FORMAT_UNKNOWN)
    {
        const std::string depthName = name + "_depth";

        depthTexture = resources->createDepthStencil(
            depthFormat,
            size_t(newWidth),
            size_t(newHeight),
            sampleCount,
            clearDepth,
            0,
            depthName.c_str());
    }
}

void RenderTexture::createDescriptors()
{
    if (!app || !texture)
        return;

    ModuleTargetDescriptors* targetDescs = app->getTargetDescriptors();
    ModuleShaderDescriptors* shaderDescs = app->getShaderDescriptors();

    if (!targetDescs || !shaderDescs)
        return;

    rtvDesc = targetDescs->createRT(texture.Get());

    srvDesc = shaderDescs->allocTable();
    srvDesc.createTextureSRV((msaa && autoResolveMSAA && resolved) ? resolved.Get() : texture.Get());

    if (depthTexture && depthFormat != DXGI_FORMAT_UNKNOWN)
        dsvDesc = targetDescs->createDS(depthTexture.Get());
    else
        dsvDesc.reset();
}

void RenderTexture::transition(
    ID3D12GraphicsCommandList* cmdList,
    ID3D12Resource* res,
    D3D12_RESOURCE_STATES& current,
    D3D12_RESOURCE_STATES target)
{
    if (!cmdList || !res || current == target)
        return;

    CD3DX12_RESOURCE_BARRIER b = CD3DX12_RESOURCE_BARRIER::Transition(res, current, target);
    cmdList->ResourceBarrier(1, &b);
    current = target;
}

void RenderTexture::transitionToRTV(ID3D12GraphicsCommandList* cmdList)
{
    transition(cmdList, texture.Get(), textureState, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void RenderTexture::transitionToSRV(ID3D12GraphicsCommandList* cmdList)
{
    transition(cmdList, texture.Get(), textureState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || !isValid())
        return;

    transitionToRTV(cmdList);
    setRenderTargetAndClear(cmdList);
}

void RenderTexture::endRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || !isValid())
        return;

    if (msaa && autoResolveMSAA && resolved)
        resolveMSAA(cmdList);
    else
        transitionToSRV(cmdList);
}

void RenderTexture::resolveMSAA(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || !texture || !resolved)
        return;

    transition(cmdList, texture.Get(), textureState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    transition(cmdList, resolved.Get(), resolvedState, D3D12_RESOURCE_STATE_RESOLVE_DEST);

    cmdList->ResolveSubresource(resolved.Get(), 0, texture.Get(), 0, colourFormat);

    transition(cmdList, texture.Get(), textureState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(cmdList, resolved.Get(), resolvedState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void RenderTexture::setRenderTargetAndClear(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || !rtvDesc)
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvDesc.getCPUHandle();

    if (!depthTexture || depthFormat == DXGI_FORMAT_UNKNOWN || !dsvDesc)
    {
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmdList->ClearRenderTargetView(rtv, reinterpret_cast<const float*>(&clearColour), 0, nullptr);
    }
    else
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDesc.getCPUHandle();
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        cmdList->ClearRenderTargetView(rtv, reinterpret_cast<const float*>(&clearColour), 0, nullptr);
        cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clearDepth, 0, 0, nullptr);
    }

    D3D12_VIEWPORT vp{ 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    D3D12_RECT sc{ 0, 0, LONG(width), LONG(height) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &sc);
}
