#pragma once

#include "Module.h"
#include "ModuleSamplers.h"
#include "ShaderTableDesc.h"

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

    void imGuiCommands(); // NEW: UI lives here

private:
    // GPU objects
    Microsoft::WRL::ComPtr<ID3D12Resource>       vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                    vbView = {};

    Microsoft::WRL::ComPtr<ID3D12RootSignature>  rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  pso;

    // Texture + SRV table
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    ShaderTableDesc textureTable;

    // Debug draw pass
    std::unique_ptr<DebugDrawPass> debugDrawPass;

    // UI state (moved from UIModule)
    bool showGrid = true;
    bool showAxis = true;

    // Only 4 options required by the assignment
    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;
};
