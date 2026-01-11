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
    uint32_t ClampMin1(uint32_t v) { return (v == 0u) ? 1u : v; }

    std::wstring ToWString(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }
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

    // ImGui::Image expects a non-MSAA Texture2D SRV.
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
            if (texture)      resources->deferRelease(texture.Get());
            if (resolved)     resources->deferRelease(resolved.Get());
            if (depthTexture) resources->deferRelease(depthTexture.Get());
        }
    }

    texture.Reset();
    resolved.Reset();
    depthTexture.Reset();

    width = 0;
    height = 0;

    textureState = D3D12_RESOURCE_STATE_COMMON;
    resolvedState = D3D12_RESOURCE_STATE_COMMON;

    srvDesc = {};
    rtvDesc = {};
    dsvDesc = {};
}

void RenderTexture::resize(uint32_t newWidth, uint32_t newHeight)
{
    newWidth = ClampMin1(newWidth);
    newHeight = ClampMin1(newHeight);

    if (width == newWidth && height == newHeight && isValid())
        return;

    // Caller should flush the GPU before resizing.
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

    // Color render target
    if (texture)
        resources->deferRelease(texture.Get());

    texture = resources->createRenderTarget(
        colourFormat,
        size_t(newWidth),
        size_t(newHeight),
        msaa ? 4u : 1u,
        clearColour,
        name.c_str());

    textureState = D3D12_RESOURCE_STATE_COMMON;

    if (texture)
        texture->SetName(ToWString(name).c_str());

    // Optional resolved target for MSAA
    if (resolved)
        resources->deferRelease(resolved.Get());

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

        if (resolved)
            resolved->SetName(ToWString(resolvedName).c_str());
    }

    // Optional depth
    if (depthTexture)
        resources->deferRelease(depthTexture.Get());

    depthTexture.Reset();

    if (depthFormat != DXGI_FORMAT_UNKNOWN)
    {
        const std::string depthName = name + "_depth";

        depthTexture = resources->createDepthStencil(
            depthFormat,
            size_t(newWidth),
            size_t(newHeight),
            msaa ? 4u : 1u,
            clearDepth,
            0,
            depthName.c_str());

        if (depthTexture)
            depthTexture->SetName(ToWString(depthName).c_str());
    }
}

void RenderTexture::createDescriptors()
{
    if (!app)
        return;

    ModuleShaderDescriptors* shaderDescs = app->getShaderDescriptors();
    ModuleTargetDescriptors* targetDescs = app->getTargetDescriptors();

    if (!shaderDescs || !targetDescs || !texture)
        return;

    // RTV
    rtvDesc = targetDescs->createRT(texture.Get());

    // SRV (use resolved when MSAA+resolve is enabled)
    srvDesc = shaderDescs->allocTable();
    srvDesc.createTextureSRV((msaa && autoResolveMSAA && resolved) ? resolved.Get() : texture.Get());

    // DSV
    if (depthTexture && depthFormat != DXGI_FORMAT_UNKNOWN)
        dsvDesc = targetDescs->createDS(depthTexture.Get());
    else
        dsvDesc = {};
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

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || !isValid())
        return;

    transition(cmdList, texture.Get(), textureState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setRenderTargetAndClear(cmdList);
}

void RenderTexture::endRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || !isValid())
        return;

    if (msaa && autoResolveMSAA && resolved)
        resolveMSAA(cmdList);
    else
        transition(cmdList, texture.Get(), textureState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
    if (!cmdList)
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvDesc.getCPUHandle();

    if (!depthTexture || depthFormat == DXGI_FORMAT_UNKNOWN)
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
