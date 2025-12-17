#pragma once

#include "Module.h"
#include "ModuleSamplers.h"   // para ModuleSamplers::Type

#include <d3d12.h>
#include <wrl.h>
#include <memory>

// Forward declaration
class DebugDrawPass;

class Assignment1Module : public Module
{
public:
    Assignment1Module() = default;
    ~Assignment1Module() override = default;

    bool init() override;
    bool cleanUp() override;
    void render() override;

private:
    bool createVertexBuffer();
    bool createRootSignature();
    bool createPipelineState();

private:
    // GPU objects
    Microsoft::WRL::ComPtr<ID3D12Resource>       vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                    vbView = {};

    Microsoft::WRL::ComPtr<ID3D12RootSignature>  rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  pso;

    // Texture + SRV index
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    uint32_t textureSRV = UINT32_MAX;

    // Debug draw pass (grid + axes)
    std::unique_ptr<DebugDrawPass> debugDrawPass;

    // Sampler selection (ImGui)
    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;
};
