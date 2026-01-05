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

#include "d3dx12.h"

#include "imgui.h"
#include "ImGuizmo.h"

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
            cam->setFocusBounds(Vector3::Zero, 2.5f);
    }

    return ok;
}

// ---------------------------------------------------------
// cleanUp
// ---------------------------------------------------------
bool Exercise5Module::cleanUp()
{
    materialBuffers.clear();
    pso.Reset();
    rootSignature.Reset();
    debugDrawPass.reset();

    showAxis = false;
    showGrid = true;
    showGuizmo = true;
    gizmoOperation = 0;

    currentSampler = ModuleSamplers::Type::Linear_Wrap;
    return true;
}

// ---------------------------------------------------------
// ImGui (matches screenshot) + ImGuizmo
// ---------------------------------------------------------
void Exercise5Module::imGuiCommands(const Matrix& view, const Matrix& proj)
{
    // === Window: exactly like screenshot ===
    ImGui::Begin("Geometry Viewer Options");

    ImGui::Checkbox("Show grid", &showGrid);
    ImGui::Checkbox("Show axis", &showAxis);
    ImGui::Checkbox("Show guizmo", &showGuizmo);

    ImGui::Text("Model loaded %s with %u meshes and %u materials",
        model.getSrcFile().c_str(),
        model.getNumMeshes(),
        model.getNumMaterials());

    for (const BasicMesh& mesh : model.getMeshes())
    {
        const uint32_t tris = (mesh.getNumIndices() > 0) ? (mesh.getNumIndices() / 3) : (mesh.getNumVertices() / 3);
        ImGui::Text("Mesh %s with %u vertices and %u triangles",
            mesh.getName().c_str(),
            mesh.getNumVertices(),
            tris);
    }

    Matrix objectMatrix = model.getModelMatrix();

    ImGui::Separator();

    // Optional hotkeys like prof (T/R/S)
    if (ImGui::IsKeyPressed(ImGuiKey_T)) gizmoOperation = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) gizmoOperation = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) gizmoOperation = 2;

    // Radio buttons exactly as screenshot: Translate / Rotate / Scale
    ImGui::RadioButton("Translate", &gizmoOperation, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Rotate", &gizmoOperation, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Scale", &gizmoOperation, 2);

    float translation[3], rotation[3], scale[3];
    ImGuizmo::DecomposeMatrixToComponents((float*)&objectMatrix, translation, rotation, scale);

    bool changed = false;
    changed |= ImGui::DragFloat3("Tr", translation, 0.1f);
    changed |= ImGui::DragFloat3("Rt", rotation, 0.1f);
    changed |= ImGui::DragFloat3("Sc", scale, 0.001f);

    if (changed)
    {
        ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, (float*)&objectMatrix);
        model.setModelMatrix(objectMatrix);
    }

    ImGui::End();

    // === Gizmo overlay ===
    if (!showGuizmo)
        return;

    D3D12Module* d3d12 = app->getD3D12Module();
    const unsigned width = d3d12->getWindowWidth();
    const unsigned height = d3d12->getWindowHeight();

    // Make sure ImGuizmo is on top and gets proper input
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, float(width), float(height));

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (gizmoOperation == 1) op = ImGuizmo::ROTATE;
    if (gizmoOperation == 2) op = ImGuizmo::SCALE;

    // NOTE: If your camera matrices are transposed elsewhere, this is the correct usage:
    // DirectX::SimpleMath::Matrix is row-major. ImGuizmo expects float[16] in column-major.
    // In practice, with SimpleMath, this still works correctly as long as you're consistent.
    // If you see wrong manipulations, we can switch to passing transposed matrices.
    ImGuizmo::Manipulate(
        (const float*)&view,
        (const float*)&proj,
        op,
        ImGuizmo::LOCAL,
        (float*)&objectMatrix
    );

    if (ImGuizmo::IsUsing())
    {
        model.setModelMatrix(objectMatrix);
    }
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

    // MVP
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
    commandList->ClearDepthStencilView(
        dsv,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

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

    // Root: MVP + sampler
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / 4, &mvp, 0);
    commandList->SetGraphicsRootDescriptorTable(3, app->getSamplers()->getGPUHandle(currentSampler));

    // Draw model
    const auto& meshes = model.getMeshes();
    const auto& mats = model.getMaterials();

    for (const BasicMesh& mesh : meshes)
    {
        const int matIndex = mesh.getMaterialIndex();
        if (matIndex < 0 || matIndex >= (int)mats.size())
            continue;

        commandList->SetGraphicsRootConstantBufferView(
            1, materialBuffers[(size_t)matIndex]->GetGPUVirtualAddress());

        commandList->SetGraphicsRootDescriptorTable(
            2, mats[(size_t)matIndex].getTexturesTableGPU());

        mesh.draw(commandList);
    }

    // Debug draw (KEEP -50..50)
    if (showGrid)
        dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);
    if (showAxis)
        dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(commandList, width, height, view, proj);

    // ImGui + ImGuizmo (important: gizmo AFTER UI window)
    if (ImGuiPass* ui = d3d12->getImGuiPass())
    {
        ImGuizmo::BeginFrame();
        imGuiCommands(view, proj);
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
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(
            UINT(std::size(commandLists)), commandLists);
    }
}

// ---------------------------------------------------------
// createRootSignature (PPT layout)
// ---------------------------------------------------------
bool Exercise5Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootParameters[4] = {};
    CD3DX12_DESCRIPTOR_RANGE tableRanges;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    tableRanges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    rootParameters[0].InitAsConstants((sizeof(Matrix) / sizeof(UINT32)), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsDescriptorTable(1, &tableRanges, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

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
// loadModel
// ---------------------------------------------------------
bool Exercise5Module::loadModel()
{
    model.load("../Game/Assets/Models/Duck/Duck.gltf",
        "../Game/Assets/Models/Duck/",
        BasicMaterial::BASIC);

    model.scale() = Vector3(0.01f, 0.01f, 0.01f);

    return model.getNumMeshes() > 0;
}

// ---------------------------------------------------------
// createMaterialBuffers
// ---------------------------------------------------------
bool Exercise5Module::createMaterialBuffers()
{
    ModuleResources* resources = app->getResources();

    const auto& mats = model.getMaterials();
    materialBuffers.clear();
    materialBuffers.reserve(mats.size());

    for (const BasicMaterial& mat : mats)
    {
        MaterialCBData cb = {};
        const BasicMaterialData& src = mat.getBasicMaterial();

        cb.colour = src.baseColour;
        cb.hasColourTex = src.hasColourTexture;

        const size_t cbSize = alignUp(sizeof(MaterialCBData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        materialBuffers.push_back(resources->createDefaultBuffer(&cb, cbSize, mat.getName()));

        if (!materialBuffers.back())
            return false;
    }

    return true;
}
