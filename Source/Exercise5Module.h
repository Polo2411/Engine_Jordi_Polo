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

    void imGuiCommands(const Matrix& view, const Matrix& proj);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

    BasicModel model;

    struct MaterialCBData
    {
        XMFLOAT4 colour;
        BOOL     hasColourTex;
        UINT     padding[3] = { 0, 0, 0 };
    };

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> materialBuffers;

    std::unique_ptr<DebugDrawPass> debugDrawPass;

    // UI toggles (as in screenshot)
    bool showAxis = false;
    bool showGrid = true;
    bool showGuizmo = true;

    // Gizmo mode: 0=Translate, 1=Rotate, 2=Scale
    int gizmoOperation = 0;

    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;
};
