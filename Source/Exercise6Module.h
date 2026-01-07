#pragma once

#include "Module.h"
#include "ModuleSamplers.h"
#include "BasicModel.h"

#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <cstdint>

class DebugDrawPass;

class Exercise6Module : public Module
{
public:
    Exercise6Module() = default;
    ~Exercise6Module() override = default;

    bool init() override;
    bool cleanUp() override;
    void render() override;

private:
    bool createRootSignature();
    bool createPipelineState();
    bool createFrameBuffers();
    bool loadModel();

    void imGuiCommands(const Matrix& view, const Matrix& proj);
    static Matrix computeNormalMatrixSafe(const Matrix& model);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

    BasicModel model;

    // Matches Exercise6.hlsli
    struct MVPData
    {
        Matrix mvp;
    };

    struct PerFrameData
    {
        Vector3 L = Vector3(-0.7f, -0.14f, -0.7f);
        float   _pad0 = 0.0f;

        Vector3 Lc = Vector3(1.0f, 1.0f, 1.0f);
        float   _pad1 = 0.0f;

        Vector3 Ac = Vector3(0.10f, 0.10f, 0.10f);
        float   _pad2 = 0.0f;

        Vector3 viewPos = Vector3::Zero;
        float   _pad3 = 0.0f;
    };

    struct PerInstanceData
    {
        Matrix modelMat;
        Matrix normalMat;
        PhongMaterialData material;
    };

    // UI Light (like the professor)
    struct Light
    {
        Vector3 L = Vector3(-0.7f, -0.14f, -0.7f);
        Vector3 Lc = Vector3(1.0f, 1.0f, 1.0f);
        Vector3 Ac = Vector3(0.10f, 0.10f, 0.10f);
    };

    Light light;

    static constexpr uint32_t kFramesInFlight = 2;

    Microsoft::WRL::ComPtr<ID3D12Resource> mvpBuffer;
    uint8_t* mvpMapped = nullptr;
    size_t   mvpStride = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameBuffer;
    uint8_t* perFrameMapped = nullptr;
    size_t   perFrameStride = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> perInstanceBuffer;
    uint8_t* perInstanceMapped = nullptr;
    size_t   perInstanceStride = 0;

    std::unique_ptr<DebugDrawPass> debugDrawPass;

    // UI
    bool showAxis = false;
    bool showGrid = true;
    bool showGuizmo = true;

    // 0=Translate, 1=Rotate, 2=Scale
    int gizmoOperation = 0;

    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;

    // Local avg ms
    static constexpr int kAvgWindow = 60;
    double msHistory[kAvgWindow] = {};
    int    msIndex = 0;
    int    msCount = 0;
};
