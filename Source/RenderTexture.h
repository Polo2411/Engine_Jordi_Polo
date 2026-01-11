#pragma once

#include <string>
#include <cstdint>

#include <wrl/client.h>
#include <d3d12.h>

#include "MathUtils.h"
#include "ShaderTableDesc.h"
#include "RenderTargetDesc.h"
#include "DepthStencilDesc.h"

class RenderTexture
{
public:
    RenderTexture(
        const char* name,
        DXGI_FORMAT colourFormat,
        const Vector4& clearColour,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN,
        float clearDepth = 1.0f,
        bool msaa = false,
        bool autoResolveMSAA = false);

    ~RenderTexture();

    RenderTexture(const RenderTexture&) = delete;
    RenderTexture& operator=(const RenderTexture&) = delete;

    bool isValid() const { return width > 0 && height > 0 && texture != nullptr; }

    void resize(uint32_t newWidth, uint32_t newHeight);

    void beginRender(ID3D12GraphicsCommandList* cmdList);
    void endRender(ID3D12GraphicsCommandList* cmdList);

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle() const { return srvDesc.getGPUHandle(); }
    const ShaderTableDesc& getSrvTableDesc() const { return srvDesc; }

    const RenderTargetDesc& getRtvDesc() const { return rtvDesc; }
    const DepthStencilDesc& getDsvDesc() const { return dsvDesc; }

    bool hasDepth() const { return depthFormat != DXGI_FORMAT_UNKNOWN; }

private:
    void releaseResources();

    void createResources(uint32_t newWidth, uint32_t newHeight);
    void createDescriptors();

    void transition(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* res,
        D3D12_RESOURCE_STATES& current,
        D3D12_RESOURCE_STATES target);

    void transitionToRTV(ID3D12GraphicsCommandList* cmdList);
    void transitionToSRV(ID3D12GraphicsCommandList* cmdList);

    void resolveMSAA(ID3D12GraphicsCommandList* cmdList);
    void setRenderTargetAndClear(ID3D12GraphicsCommandList* cmdList);

private:
    std::string name;

    uint32_t width = 0;
    uint32_t height = 0;

    DXGI_FORMAT colourFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;

    Vector4 clearColour = Vector4(0, 0, 0, 1);
    float clearDepth = 1.0f;

    bool msaa = false;
    bool autoResolveMSAA = false;
    uint32_t sampleCount = 1;

    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> resolved;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthTexture;

    D3D12_RESOURCE_STATES textureState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES resolvedState = D3D12_RESOURCE_STATE_COMMON;

    ShaderTableDesc  srvDesc;
    RenderTargetDesc rtvDesc;
    DepthStencilDesc dsvDesc;
};
