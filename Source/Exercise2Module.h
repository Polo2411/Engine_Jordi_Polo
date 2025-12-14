#pragma once

#include "Module.h"
#include "Globals.h"

#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

class Exercise2Module : public Module
{
public:
    bool init() override;
    void render() override;
    bool cleanUp() override { return true; }

private:
    bool createVertexBuffer();
    bool createRootSignature();
    bool createPSO();

private:
    ComPtr<ID3D12Resource>      vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW    vertexBufferView{};
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pso;
};
