#pragma once

#include "Module.h"
#include "ModuleSamplers.h"
#include "ShaderTableDesc.h"

#include <d3d12.h>
#include <wrl.h>
#include <memory>

class DebugDrawPass;

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
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbView = {};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

    // Texture
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;

    // NEW: shader-visible SRV table (instead of legacy uint32 handle)
    ShaderTableDesc textureTable;

    std::unique_ptr<DebugDrawPass> debugDrawPass;

    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;
};
