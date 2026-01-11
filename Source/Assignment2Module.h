#pragma once

#include "Module.h"
#include "ModuleSamplers.h"

#include "BasicModel.h"
#include "RenderTexture.h"

#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <cstdint>

class DebugDrawPass;

class Assignment2Module : public Module
{
public:
    Assignment2Module() = default;
    ~Assignment2Module() override = default;

    bool init() override;
    bool cleanUp() override;
    void render() override;

private:
    bool createRootSignature();
    bool createPipelineState();
    bool createFrameBuffers();
    bool loadModel();

    void buildImGuiAndHandleResize(const Matrix& view, const Matrix& proj, uint32_t& outSceneW, uint32_t& outSceneH);
    void imGuiOptionsAndGizmo(const Matrix& view, const Matrix& proj);
    static Matrix computeNormalMatrixSafe(const Matrix& model);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

    BasicModel model;

    std::unique_ptr<RenderTexture> sceneRT;
    uint32_t lastSceneW = 1;
    uint32_t lastSceneH = 1;

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
        BasicMaterial::PhongMaterialData material;
    };

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

    bool showAxis = false;
    bool showGrid = true;
    bool showGuizmo = true;

    int gizmoOperation = 0;

    ModuleSamplers::Type currentSampler = ModuleSamplers::Type::Linear_Wrap;

    static constexpr int kAvgWindow = 60;
    double msHistory[kAvgWindow] = {};
    int    msIndex = 0;
    int    msCount = 0;

};
