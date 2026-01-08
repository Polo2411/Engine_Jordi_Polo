#include "Globals.h"
#include "Exercise5Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleCamera.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplers.h"

#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "ReadData.h"

#include "d3dx12.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <filesystem>
#include <string>

using namespace DirectX;

namespace fs = std::filesystem;

namespace
{
    fs::path GetExeDir()
    {
        wchar_t buf[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
            return fs::current_path();

        fs::path p(buf);
        return p.has_parent_path() ? p.parent_path() : fs::current_path();
    }

    bool FindUpwards(const fs::path& startDir, const fs::path& relativeFile, fs::path& outAbsFile, int maxLevels = 12)
    {
        fs::path dir = startDir;

        for (int i = 0; i <= maxLevels; ++i)
        {
            fs::path candidate = dir / relativeFile;
            if (fs::exists(candidate))
            {
                outAbsFile = fs::absolute(candidate);
                return true;
            }

            if (!dir.has_parent_path())
                break;

            dir = dir.parent_path();
        }

        return false;
    }

    std::string ToGenericString(const fs::path& p)
    {
        return p.generic_string();
    }

    std::string EnsureTrailingSlash(std::string s)
    {
        if (!s.empty() && s.back() != '/')
            s.push_back('/');
        return s;
    }
}


// ---------------------------------------------------------
// init
// ---------------------------------------------------------
bool Exercise5Module::init()
{
    bool ok = true;

    ok &= createRootSignature();
    ok &= createPipelineState();
    ok &= createTransformsBuffer();
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

    if (transformsBuffer && transformsMapped)
    {
        transformsBuffer->Unmap(0, nullptr);
        transformsMapped = nullptr;
    }
    transformsBuffer.Reset();
    transformsCBSize = 0;

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
// ImGui + ImGuizmo
// ---------------------------------------------------------
void Exercise5Module::imGuiCommands(const Matrix& view, const Matrix& proj)
{
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

    if (ImGui::IsKeyPressed(ImGuiKey_T)) gizmoOperation = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) gizmoOperation = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) gizmoOperation = 2;

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

    if (!showGuizmo)
        return;

    D3D12Module* d3d12 = app->getD3D12Module();
    const unsigned width = d3d12->getWindowWidth();
    const unsigned height = d3d12->getWindowHeight();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, float(width), float(height));

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (gizmoOperation == 1) op = ImGuizmo::ROTATE;
    if (gizmoOperation == 2) op = ImGuizmo::SCALE;

    ImGuizmo::Manipulate(
        (const float*)&view,
        (const float*)&proj,
        op,
        ImGuizmo::LOCAL,
        (float*)&objectMatrix
    );

    if (ImGuizmo::IsUsing())
        model.setModelMatrix(objectMatrix);
}

// ---------------------------------------------------------
// render
// ---------------------------------------------------------
void Exercise5Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), pso.Get());

    // IMPORTANT: End this BEFORE Close().
    BEGIN_EVENT(commandList, "Exercise5 Frame");

    // Backbuffer: PRESENT -> RENDER_TARGET
    BEGIN_EVENT(commandList, "BackBuffer Transition: PRESENT -> RT");
    {
        CD3DX12_RESOURCE_BARRIER barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(
                d3d12->getBackBuffer(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);
    }
    END_EVENT(commandList);

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

    // MVP (keep transpose because shader uses column_major float4x4 + mul(v, m))
    Matrix mvp = (model.getModelMatrix() * view * proj).Transpose();

    // Update transforms CB (b0)
    if (transformsMapped)
    {
        TransformsCBData cb = {};
        cb.mvp = mvp;
        memcpy(transformsMapped, &cb, sizeof(cb));
    }

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
    BEGIN_EVENT(commandList, "Clear");
    {
        float clearColor[] = { 0.05f, 0.05f, 0.06f, 1.0f };

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        commandList->ClearDepthStencilView(
            dsv,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            1.0f, 0, 0, nullptr);
    }
    END_EVENT(commandList);

    // Pipeline setup
    BEGIN_EVENT(commandList, "Pipeline Setup");
    {
        commandList->SetGraphicsRootSignature(rootSignature.Get());
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);

        ID3D12DescriptorHeap* heaps[] =
        {
            app->getShaderDescriptors()->getHeap(),
            app->getSamplers()->getHeap()
        };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        // b0 = transforms CBV (VS)
        if (transformsBuffer)
        {
            commandList->SetGraphicsRootConstantBufferView(
                0, transformsBuffer->GetGPUVirtualAddress());
        }

        // s0 sampler (PS)
        commandList->SetGraphicsRootDescriptorTable(
            3, app->getSamplers()->getGPUHandle(currentSampler));
    }
    END_EVENT(commandList);

    // Draw model
    BEGIN_EVENT(commandList, "Model Render Pass");
    {
        const auto& meshes = model.getMeshes();
        const auto& mats = model.getMaterials();

        if (meshes.empty())
            SET_MARKER(commandList, "Warning: no meshes");

        for (const BasicMesh& mesh : meshes)
        {
            char meshEvent[256]{};
            sprintf_s(meshEvent, "Draw Mesh: %s", mesh.getName().c_str());
            BEGIN_EVENT(commandList, meshEvent);

            const int matIndex = mesh.getMaterialIndex();
            if (matIndex < 0 || matIndex >= (int)mats.size())
            {
                SET_MARKER(commandList, "Warning: invalid material index");
                END_EVENT(commandList);
                continue;
            }

            // b1 = material CBV (PS)
            commandList->SetGraphicsRootConstantBufferView(
                1, materialBuffers[(size_t)matIndex]->GetGPUVirtualAddress());

            // t0..t4 = material textures table (PS)
            commandList->SetGraphicsRootDescriptorTable(
                2, mats[(size_t)matIndex].getTexturesTableGPU());

            mesh.draw(commandList);

            END_EVENT(commandList);
        }
    }
    END_EVENT(commandList);

    // Debug draw
    BEGIN_EVENT(commandList, "Debug Draw");
    {
        if (showGrid)
            dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);
        if (showAxis)
            dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

        if (debugDrawPass)
            debugDrawPass->record(commandList, width, height, view, proj);
    }
    END_EVENT(commandList);

    // ImGui
    BEGIN_EVENT(commandList, "ImGui");
    {
        if (ImGuiPass* ui = d3d12->getImGuiPass())
        {
            ImGuizmo::BeginFrame();
            imGuiCommands(view, proj);
            ui->record(commandList);
        }
    }
    END_EVENT(commandList);

    // Backbuffer: RENDER_TARGET -> PRESENT
    BEGIN_EVENT(commandList, "BackBuffer Transition: RT -> PRESENT");
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            d3d12->getBackBuffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        commandList->ResourceBarrier(1, &barrier);
    }
    END_EVENT(commandList);

    // IMPORTANT: End frame BEFORE Close().
    END_EVENT(commandList);

    if (SUCCEEDED(commandList->Close()))
    {
        ID3D12CommandList* commandLists[] = { commandList };
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(
            UINT(std::size(commandLists)), commandLists);
    }
}

// ---------------------------------------------------------
// createRootSignature
// ---------------------------------------------------------
bool Exercise5Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootParameters[4] = {};
    CD3DX12_DESCRIPTOR_RANGE tableRanges;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    // t0..t4 contiguous (matches BasicMaterial table)
    tableRanges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, BasicMaterial::SLOT_COUNT, 0);
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    // b0 = transforms (VS)
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    // b1 = material (PS)
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // t0..t4 table (PS)
    rootParameters[2].InitAsDescriptorTable(1, &tableRanges, D3D12_SHADER_VISIBILITY_PIXEL);

    // s0 table (PS)
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

    HRESULT hr = app->getD3D12Module()->getDevice()->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));

    if (FAILED(hr))
        return false;

    rootSignature->SetName(L"Exercise5 RootSignature");
    return true;
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

    HRESULT hr = app->getD3D12Module()->getDevice()->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pso));

    if (FAILED(hr))
        return false;

    pso->SetName(L"Exercise5 PSO");
    return true;
}

// ---------------------------------------------------------
// createTransformsBuffer (UPLOAD CBV, persistently mapped)
// ---------------------------------------------------------
bool Exercise5Module::createTransformsBuffer()
{
    ID3D12Device* device = app->getD3D12Module()->getDevice();
    if (!device)
        return false;

    transformsCBSize = alignUp(sizeof(TransformsCBData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(transformsCBSize);

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&transformsBuffer)
    );

    if (FAILED(hr) || !transformsBuffer)
        return false;

    transformsBuffer->SetName(L"Exercise5 Transforms CB (b0)");

    transformsMapped = nullptr;
    CD3DX12_RANGE readRange(0, 0); // we do not read from CPU
    hr = transformsBuffer->Map(0, &readRange, reinterpret_cast<void**>(&transformsMapped));
    if (FAILED(hr) || !transformsMapped)
        return false;

    // Initialize to identity
    TransformsCBData init = {};
    init.mvp = Matrix::Identity;
    memcpy(transformsMapped, &init, sizeof(init));

    return true;
}

// ---------------------------------------------------------
// loadModel (robust path for PIX / exe / VS / Release)
// ---------------------------------------------------------
bool Exercise5Module::loadModel()
{
    const fs::path relGltf = fs::path("Game") / "Assets" / "Models" / "Duck" / "Duck.gltf";

    fs::path absGltf;
    const fs::path cwd = fs::current_path();
    const fs::path exeDir = GetExeDir();

    bool found = FindUpwards(cwd, relGltf, absGltf, 12);
    if (!found)
        found = FindUpwards(exeDir, relGltf, absGltf, 12);

    if (!found)
    {
        LOG("Exercise5Module: Could not find %s (CWD=%s, EXE=%s)",
            relGltf.generic_string().c_str(),
            cwd.generic_string().c_str(),
            exeDir.generic_string().c_str());
        return false;
    }

    const fs::path absDuckDir = absGltf.parent_path();

    const std::string gltfPath = ToGenericString(absGltf);
    const std::string basePath = EnsureTrailingSlash(ToGenericString(absDuckDir));

    model.load(gltfPath.c_str(), basePath.c_str(), BasicMaterial::BASIC);

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
