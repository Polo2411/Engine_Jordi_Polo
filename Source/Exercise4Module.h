#pragma once

#include "Module.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class Exercise4Module : public Module
{
public:
    Exercise4Module() = default;
    ~Exercise4Module() override = default;

    bool init() override;
    bool cleanUp() override;
    void render() override;

private:
    bool createVertexBuffer();
    bool createRootSignature();
    bool createPipelineState();

private:
    ComPtr<ID3D12Resource>          vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW        vbView{};

    ComPtr<ID3D12RootSignature>     rootSignature;
    ComPtr<ID3D12PipelineState>     pso;

    ComPtr<ID3D12Resource> texture;
    uint32_t textureSRV = UINT32_MAX;

};
