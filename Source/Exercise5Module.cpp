#include "Globals.h"
#include "Exercise5Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleCamera.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplers.h"
#include "TimeManager.h"

#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "ReadData.h"

#include <d3dcompiler.h>
#include "d3dx12.h"

#include "imgui.h"

using namespace DirectX;

// ---------------------------------------------------------
// init
// ---------------------------------------------------------
bool Exercise5Module::init()
{
    bool ok = true;

    ok &= createRootSignature();
    ok &= createPipelineState();
    ok &= loadModel();
    ok &= createMaterialBuffers();

    if (ok)
    {
        D3D12Module* d3d12 = app->getD3D12Module();

        Microsoft::WRL::ComPtr<ID3D12Device4> device4;
        if (FAILED(d3d12->getDevice()->QueryInterface(IID_PPV_ARGS(&device4))))
            return false;

        debugDrawPass = std::make_unique<DebugDrawPass>(
            device4.Get(),
            d3d12->getDrawCommandQueue()
        );

        if (ModuleCamera* cam = app->getCamera())
        {
            cam->setFocusBounds(Vector3::Zero, 3.0f);
        }
    }

    return ok;
}

// ---------------------------------------------------------
// cleanUp
// ---------------------------------------------------------
bool Exercise5Module::cleanUp()
{
    materialBuffers.clear();
    fallbackMaterialBuffer.Reset();

    pso.Reset();
    rootSignature.Reset();
    debugDrawPass.reset();

    currentSampler = ModuleSamplers::Type::Linear_Wrap;
    return true;
}

// ---------------------------------------------------------
// render
// ---------------------------------------------------------
void Exercise5Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), pso.Get());

    // Backbuffer: PRESENT -> RENDER_TARGET
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            d3d12->getBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    const unsigned width = d3d12->getWindowWidth();
    const unsigned height = d3d12->getWindowHeight();

    // Camera
    Matrix view = Matrix::Identity;
    Matrix proj = Matrix::Identity;

    if (ModuleCamera* cam = app->getCamera())
    {
        cam->setAspectRatio((height > 0) ? (float(width) / float(height)) : 1.0f);
        view = cam->getViewMatrix();
        proj = cam->getProjectionMatrix();
    }
    else
    {
        view = Matrix::CreateLookAt(Vector3(0.0f, 2.0f, 6.0f), Vector3::Zero, Vector3::Up);
        proj = Matrix::CreatePerspectiveFieldOfView(
            XM_PIDIV4,
            (height > 0) ? (float(width) / float(height)) : 1.0f,
            0.1f, 1000.0f);
    }

    // MVP (root constants b0)
    Matrix mvp = (model.getModelMatrix() * view * proj).Transpose();

    // Viewport / Scissor
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.Width = float(width);
    viewport.Height = float(height);

    D3D12_RECT scissor = {};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = LONG(width);
    scissor.bottom = LONG(height);

    // Clear
    float clearColor[] = { 0.05f, 0.05f, 0.06f, 1.0f };

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Pipeline
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    // Descriptor heaps (SRV + samplers)
    ID3D12DescriptorHeap* heaps[] =
    {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplers()->getHeap()
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Root bindings:
    // 0: MVP constants (b0)
    // 3: Sampler table (s0) - we bind only one sampler descriptor (selected)
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / 4, &mvp, 0);
    commandList->SetGraphicsRootDescriptorTable(3, app->getSamplers()->getGPUHandle(currentSampler));

    // Draw model
    const auto& meshes = model.getMeshes();
    const auto& mats = model.getMaterials();

    for (const BasicMesh& mesh : meshes)
    {
        int matIndex = mesh.getMaterialIndex();

        // Pick CBV buffer
        ID3D12Resource* cbRes = nullptr;
        if (matIndex >= 0 && matIndex < (int)materialBuffers.size())
        {
            cbRes = materialBuffers[(size_t)matIndex].Get();
        }
        else
        {
            cbRes = fallbackMaterialBuffer.Get();
        }

        if (!cbRes)
            continue;

        // 1: Material CBV (b1)
        commandList->SetGraphicsRootConstantBufferView(1, cbRes->GetGPUVirtualAddress());

        // 2: Texture table (t0) - base colour or null SRV
        D3D12_GPU_DESCRIPTOR_HANDLE texHandle = {};
        if (matIndex >= 0 && matIndex < (int)mats.size())
        {
            texHandle = mats[(size_t)matIndex].getTextureHandle(BasicMaterial::SLOT_BASECOLOUR);
        }
        else if (!mats.empty())
        {
            texHandle = mats[0].getTextureHandle(BasicMaterial::SLOT_BASECOLOUR);
        }
        else
        {
            // No materials at all: bind global null SRV
            ModuleShaderDescriptors* desc = app->getShaderDescriptors();
            texHandle = desc->getGPUHandle(desc->getNullTexture2DSrvIndex());
        }

        commandList->SetGraphicsRootDescriptorTable(2, texHandle);

        mesh.draw(commandList);
    }

    // Debug draw
    dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);
    dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(commandList, width, height, view, proj);

    // ImGui
    if (ImGuiPass* ui = d3d12->getImGuiPass())
    {
        TimeManager* tm = app->getTimeManager();

        ImGui::Begin("Exercise 5");
        ImGui::Text("glTF Model Viewer");

        ImGui::Separator();
        ImGui::Text("File: %s", model.getSrcFile().c_str());
        ImGui::Text("Meshes: %u", model.getNumMeshes());
        ImGui::Text("Materials: %u", model.getNumMaterials());

        Vector3& t = model.translation();
        Vector3& r = model.rotationDeg();
        Vector3& s = model.scale();

        ImGui::DragFloat3("Translation", &t.x, 0.01f);
        ImGui::DragFloat3("Rotation (deg)", &r.x, 0.2f);
        ImGui::DragFloat3("Scale", &s.x, 0.01f, 0.001f, 100.0f);

        static const char* names[] =
        {
            "Linear Wrap",  "Point Wrap",
            "Linear Clamp", "Point Clamp",
            "Linear Mirror","Point Mirror",
            "Linear Border","Point Border"
        };

        int idx = (int)currentSampler;
        if (ImGui::Combo("Sampler", &idx, names, IM_ARRAYSIZE(names)))
            currentSampler = (ModuleSamplers::Type)idx;

        ImGui::Separator();
        if (tm)
        {
            ImGui::Text("FPS (avg): %.1f", tm->getFPS());
            ImGui::Text("Avg ms:   %.2f", tm->getAvgFrameMs());
        }
        else
        {
            ImGui::Text("ImGui FPS: %.1f", ImGui::GetIO().Framerate);
        }

        ImGui::End();

        ui->record(commandList);
    }

    // Backbuffer: RENDER_TARGET -> PRESENT
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);

    if (SUCCEEDED(commandList->Close()))
    {
        ID3D12CommandList* commandLists[] = { commandList };
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(UINT(std::size(commandLists)), commandLists);
    }
}

// ---------------------------------------------------------
// createRootSignature (PPT layout)
// ---------------------------------------------------------
bool Exercise5Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootParameters[4] = {};

    CD3DX12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

    CD3DX12_DESCRIPTOR_RANGE sampRange = {};
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // s0 (selected handle)

    rootParameters[0].InitAsConstants((sizeof(Matrix) / sizeof(UINT32)), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // b0
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);                            // b1
    rootParameters[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);                      // t0
    rootParameters[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);                     // s0

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        _countof(rootParameters), rootParameters,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)))
        return false;

    return SUCCEEDED(
        app->getD3D12Module()->getDevice()->CreateRootSignature(
            0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature))
    );
}

// ---------------------------------------------------------
// createPipelineState
// ---------------------------------------------------------
bool Exercise5Module::createPipelineState()
{
    auto vs = DX::ReadData(L"Exercise5VS.cso");
    auto ps = DX::ReadData(L"Exercise5PS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = BasicMesh::getInputLayoutDesc();
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { vs.data(), vs.size() };
    psoDesc.PS = { ps.data(), ps.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    psoDesc.SampleDesc = { 1, 0 };
    psoDesc.SampleMask = UINT_MAX;

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    return SUCCEEDED(
        app->getD3D12Module()->getDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(&pso))
    );
}

// ---------------------------------------------------------
// loadModel (Duck)
// ---------------------------------------------------------
bool Exercise5Module::loadModel()
{
    model.load("../Game/Assets/Models/Duck/Duck.gltf",
        "../Game/Assets/Models/Duck/",
        BasicMaterial::BASIC);

    // Common duck scale
    model.scale() = Vector3(0.01f, 0.01f, 0.01f);

    return model.getNumMeshes() > 0;
}

// ---------------------------------------------------------
// createMaterialBuffers (CBV per material)
// ---------------------------------------------------------
bool Exercise5Module::createMaterialBuffers()
{
    ModuleResources* resources = app->getResources();

    materialBuffers.clear();
    fallbackMaterialBuffer.Reset();

    // Fallback (white, no texture)
    {
        BasicMaterialData fallback = {};
        fallback.baseColour = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
        fallback.hasColourTexture = FALSE;

        const size_t cbSize = alignUp(sizeof(BasicMaterialData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        fallbackMaterialBuffer = resources->createDefaultBuffer(&fallback, cbSize, "FallbackMaterialCB");
        if (!fallbackMaterialBuffer)
            return false;
    }

    const auto& mats = model.getMaterials();
    materialBuffers.reserve(mats.size());

    for (const BasicMaterial& mat : mats)
    {
        // Exercise 5 uses BASIC material data (colour + hasColourTexture)
        BasicMaterialData cb = {};
        if (mat.getMaterialType() == BasicMaterial::BASIC)
        {
            cb = mat.getBasicMaterial();
        }
        else
        {
            // Convert other types to a reasonable default for this exercise
            cb.baseColour = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
            cb.hasColourTexture = FALSE;
        }

        const size_t cbSize = alignUp(sizeof(BasicMaterialData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        auto buffer = resources->createDefaultBuffer(&cb, cbSize, mat.getName());
        if (!buffer)
            return false;

        materialBuffers.push_back(buffer);
    }

    return true;
}
