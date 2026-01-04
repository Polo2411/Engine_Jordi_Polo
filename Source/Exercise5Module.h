#pragma once

#include "Module.h"
#include "ModuleSamplers.h"

#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "BasicModel.h"

class DebugDrawPass;

class Exercise5Module : public Module
{
public:
    Exercise5Module() = default;
    ~Exercise5Module() override = default;

    bool init() override;
    bool cleanUp() override;
    void render() override;

private:
    bool createRootSignature();
    bool createPipelineState();
    bool loadModel();
    bool createMaterialBuffers();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

    BasicModel model;

    // One CBV buffer per material (b1)
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> materialBuffers;

    // Fallback material CBV (used when glTF has no materials or mesh has materialIndex < 0)
    Microsoft::WRL::ComPtr<ID3D12Resource> fallbackMaterialBuffer;

    std::unique_ptr<DebugDrawPass> debugDrawPass;

    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;
};
