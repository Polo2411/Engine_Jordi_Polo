#include "Globals.h"
#include "Exercise6Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleCamera.h"
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
#include <algorithm>

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

    double UpdateAvgMs(double* history, int window, int& idx, int& count, double ms)
    {
        history[idx] = ms;
        idx = (idx + 1) % window;
        count = (count < window) ? (count + 1) : window;

        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += history[i];
        return (count > 0) ? (sum / double(count)) : 0.0;
    }
}

// ---------------------------------------------------------
// init
// ---------------------------------------------------------
bool Exercise6Module::init()
{
    bool ok = true;

    ok &= createRootSignature();
    ok &= createPipelineState();
    ok &= loadModel();
    ok &= createFrameBuffers();

    if (ok)
    {
        D3D12Module* d3d12 = app->getD3D12Module();

        Microsoft::WRL::ComPtr<ID3D12Device4> device4;
        if (FAILED(d3d12->getDevice()->QueryInterface(IID_PPV_ARGS(&device4))))
            return false;

        debugDrawPass = std::make_unique<DebugDrawPass>(device4.Get(), d3d12->getDrawCommandQueue());
    }

    return ok;
}

// ---------------------------------------------------------
// cleanUp
// ---------------------------------------------------------
bool Exercise6Module::cleanUp()
{
    debugDrawPass.reset();

    if (mvpBuffer && mvpMapped) { mvpBuffer->Unmap(0, nullptr); mvpMapped = nullptr; }
    if (perFrameBuffer && perFrameMapped) { perFrameBuffer->Unmap(0, nullptr); perFrameMapped = nullptr; }
    if (perInstanceBuffer && perInstanceMapped) { perInstanceBuffer->Unmap(0, nullptr); perInstanceMapped = nullptr; }

    mvpBuffer.Reset();
    perFrameBuffer.Reset();
    perInstanceBuffer.Reset();

    mvpStride = 0;
    perFrameStride = 0;
    perInstanceStride = 0;

    pso.Reset();
    rootSignature.Reset();

    showAxis = false;
    showGrid = true;
    showGuizmo = true;
    gizmoOperation = 0;

    currentSampler = ModuleSamplers::Type::Linear_Wrap;

    msIndex = 0;
    msCount = 0;
    for (int i = 0; i < kAvgWindow; ++i) msHistory[i] = 0.0;

    return true;
}

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------
Matrix Exercise6Module::computeNormalMatrixSafe(const Matrix& modelM)
{
    // Normal matrix should ignore translation.
    Matrix m = modelM;
    m._41 = 0.0f; m._42 = 0.0f; m._43 = 0.0f;

    const float det = m.Determinant();
    if (fabsf(det) < 1e-8f)
        return Matrix::Identity;

    Matrix inv = m.Invert();
    return inv.Transpose();
}

// ---------------------------------------------------------
// ImGui + ImGuizmo
// Uses BasicMaterial::setPhongMaterial (no duplicated material caches)
// ---------------------------------------------------------
void Exercise6Module::imGuiCommands(const Matrix& view, const Matrix& proj)
{
    const double dt = app ? app->getDeltaTimeSeconds() : 0.0;
    const double ms = (dt > 0.0) ? (dt * 1000.0) : 0.0;
    const double avgMs = UpdateAvgMs(msHistory, kAvgWindow, msIndex, msCount, ms);
    const uint32_t fps = (dt > 0.0) ? uint32_t(1.0 / dt) : 0;

    // Window size (touch this to make it bigger)
    ImGui::SetNextWindowSize(ImVec2(520.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Geometry Viewer Options");

    ImGui::Separator();
    ImGui::Text("FPS: [%u]. Avg. elapsed (Ms): [%g] ", fps, avgMs);
    ImGui::Separator();

    ImGui::Checkbox("Show grid", &showGrid);
    ImGui::Checkbox("Show axis", &showAxis);
    ImGui::Checkbox("Show guizmo", &showGuizmo);

    ImGui::Text("Model loaded %s with %u meshes and %u materials",
        model.getSrcFile().c_str(),
        model.getNumMeshes(),
        model.getNumMaterials());

    for (const BasicMesh& mesh : model.getMeshes())
    {
        const uint32_t tris = (mesh.getNumIndices() > 0) ? (mesh.getNumIndices() / 3u) : (mesh.getNumVertices() / 3u);
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

    bool transformChanged = false;
    transformChanged |= ImGui::DragFloat3("Tr", translation, 0.1f);
    transformChanged |= ImGui::DragFloat3("Rt", rotation, 0.1f);
    transformChanged |= ImGui::DragFloat3("Sc", scale, 0.001f);

    if (transformChanged)
    {
        ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, (float*)&objectMatrix);
        model.setModelMatrix(objectMatrix);
    }

    // Light section
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Light Direction", reinterpret_cast<float*>(&light.L), 0.1f, -1.0f, 1.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("Normalize"))
        {
            light.L.Normalize();
        }

        ImGui::ColorEdit3("Light Colour", reinterpret_cast<float*>(&light.Lc), ImGuiColorEditFlags_NoAlpha);
        ImGui::ColorEdit3("Ambient Colour", reinterpret_cast<float*>(&light.Ac), ImGuiColorEditFlags_NoAlpha);
    }

    // Materials section (Phong)
    auto& mats = model.getMaterials();
    for (size_t i = 0; i < mats.size(); ++i)
    {
        BasicMaterial& mat = mats[i];
        if (mat.getMaterialType() != BasicMaterial::PHONG)
            continue;

        char header[256]{};
        _snprintf_s(header, 255, "Materila %s", mat.getName()); // keep professor typo if you want exact UI

        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            PhongMaterialData ph = mat.getPhongMaterial();
            bool dirty = false;

            if (ImGui::ColorEdit3("Diffuse Colour", reinterpret_cast<float*>(&ph.diffuseColour)))
                dirty = true;

            bool useTex = (ph.hasDiffuseTex != FALSE);
            if (ImGui::Checkbox("Use Texture", &useTex))
            {
                ph.hasDiffuseTex = useTex ? TRUE : FALSE;
                dirty = true;
            }

            if (ImGui::DragFloat("Kd", &ph.Kd, 0.01f))
                dirty = true;

            if (ImGui::DragFloat("Ks", &ph.Ks, 0.01f))
                dirty = true;

            if (ImGui::DragFloat("shininess", &ph.shininess))
                dirty = true;

            if (dirty)
            {
                // This is the key: use the setter you already wrote.
                // It clamps values and disables texture if slot doesn't exist.
                mat.setPhongMaterial(ph);
            }
        }
    }

    ImGui::End();

    // Gizmo
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
void Exercise6Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), pso.Get());

    BEGIN_EVENT(commandList, "Exercise6 Frame");

    // Backbuffer: PRESENT -> RT
    {
        CD3DX12_RESOURCE_BARRIER barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(
                d3d12->getBackBuffer(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);
    }

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

    const uint32_t frameSlot = (d3d12->getCurrentFrame() % kFramesInFlight);

    // Update MVP (b0)
    const Matrix mvp = (model.getModelMatrix() * view * proj).Transpose();
    if (mvpMapped)
    {
        MVPData cb{};
        cb.mvp = mvp;
        memcpy(mvpMapped + (frameSlot * mvpStride), &cb, sizeof(cb));
    }

    // Update PerFrame (b1)
    if (perFrameMapped)
    {
        PerFrameData pf{};
        pf.L = light.L;
        pf.L.Normalize();
        pf.Lc = light.Lc;
        pf.Ac = light.Ac;

        if (ModuleCamera* cam = app->getCamera())
            pf.viewPos = cam->getPosition(); // rename if needed
        else
            pf.viewPos = Vector3(0.0f, 2.0f, 6.0f);

        memcpy(perFrameMapped + (frameSlot * perFrameStride), &pf, sizeof(pf));
    }

    // Viewport / Scissor
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.Width = float(width);
    viewport.Height = float(height);

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = LONG(width);
    scissor.bottom = LONG(height);

    // Clear (touch this to change background color)
    {
        float clearColor[] = { 0.05f, 0.05f, 0.06f, 1.0f };

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    }

    // Pipeline setup
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    ID3D12DescriptorHeap* heaps[] =
    {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplers()->getHeap()
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // b0
    commandList->SetGraphicsRootConstantBufferView(
        0, mvpBuffer->GetGPUVirtualAddress() + (frameSlot * mvpStride));

    // b1
    commandList->SetGraphicsRootConstantBufferView(
        1, perFrameBuffer->GetGPUVirtualAddress() + (frameSlot * perFrameStride));

    // s0
    commandList->SetGraphicsRootDescriptorTable(
        4, app->getSamplers()->getGPUHandle(currentSampler));

    // Draw model
    {
        const auto& meshes = model.getMeshes();
        const auto& mats = model.getMaterials();
        const size_t meshCount = std::max<size_t>(1, meshes.size());

        for (size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx)
        {
            const BasicMesh& mesh = meshes[meshIdx];
            const int matIndex = mesh.getMaterialIndex();

            if (matIndex < 0 || matIndex >= (int)mats.size())
                continue;

            const BasicMaterial& mat = mats[(size_t)matIndex];

            // PerInstance (b2)
            if (perInstanceMapped)
            {
                PerInstanceData pi{};
                const Matrix modelM = model.getModelMatrix();
                const Matrix normalM = computeNormalMatrixSafe(modelM);

                pi.modelMat = modelM.Transpose();
                pi.normalMat = normalM.Transpose();
                pi.material = mat.getPhongMaterial();

                const size_t instanceOffset = (frameSlot * meshCount + meshIdx) * perInstanceStride;
                memcpy(perInstanceMapped + instanceOffset, &pi, sizeof(pi));

                commandList->SetGraphicsRootConstantBufferView(
                    2, perInstanceBuffer->GetGPUVirtualAddress() + instanceOffset);
            }

            // t0..t4
            commandList->SetGraphicsRootDescriptorTable(3, mat.getTexturesTableGPU());

            mesh.draw(commandList);
        }
    }

    // Debug draw (touch these params to change grid size / axis scale)
    {
        if (showGrid)
            dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);

        if (showAxis)
            dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

        if (debugDrawPass)
            debugDrawPass->record(commandList, width, height, view, proj);
    }

    // ImGui
    {
        if (ImGuiPass* ui = d3d12->getImGuiPass())
        {
            ImGuizmo::BeginFrame();
            imGuiCommands(view, proj);
            ui->record(commandList);
        }
    }

    // Backbuffer: RT -> PRESENT
    {
        CD3DX12_RESOURCE_BARRIER barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(
                d3d12->getBackBuffer(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);
        commandList->ResourceBarrier(1, &barrier);
    }

    END_EVENT(commandList);

    if (SUCCEEDED(commandList->Close()))
    {
        ID3D12CommandList* lists[] = { commandList };
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(UINT(std::size(lists)), lists);
    }
}

// ---------------------------------------------------------
// createRootSignature
// ---------------------------------------------------------
bool Exercise6Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootParameters[5] = {};
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, BasicMaterial::SLOT_COUNT, 0);
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);          // b0
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);             // b1
    rootParameters[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);             // b2
    rootParameters[3].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);      // t0..t4
    rootParameters[4].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);     // s0

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        _countof(rootParameters), rootParameters,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errorBlob)))
    {
        if (errorBlob)
            LOG("Exercise6Module RootSignature serialize error: %s", (const char*)errorBlob->GetBufferPointer());
        return false;
    }

    HRESULT hr = app->getD3D12Module()->getDevice()->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));

    if (FAILED(hr))
        return false;

    rootSignature->SetName(L"Exercise6 RootSignature");
    return true;
}

// ---------------------------------------------------------
// createPipelineState
// ---------------------------------------------------------
bool Exercise6Module::createPipelineState()
{
    auto vs = DX::ReadData(L"Exercise6VS.cso");
    auto ps = DX::ReadData(L"Exercise6PS.cso");

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

    pso->SetName(L"Exercise6 PSO");
    return true;
}

// ---------------------------------------------------------
// createFrameBuffers
// ---------------------------------------------------------
bool Exercise6Module::createFrameBuffers()
{
    ID3D12Device* device = app->getD3D12Module()->getDevice();
    if (!device)
        return false;

    const size_t meshCount = std::max<size_t>(1, model.getMeshes().size());

    mvpStride = alignUp(sizeof(MVPData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    perFrameStride = alignUp(sizeof(PerFrameData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    perInstanceStride = alignUp(sizeof(PerInstanceData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    const size_t mvpTotal = mvpStride * kFramesInFlight;
    const size_t perFrameTotal = perFrameStride * kFramesInFlight;
    const size_t perInstanceTotal = perInstanceStride * kFramesInFlight * meshCount;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

    // b0
    {
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mvpTotal);
        if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mvpBuffer))))
            return false;

        CD3DX12_RANGE readRange(0, 0);
        if (FAILED(mvpBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mvpMapped))) || !mvpMapped)
            return false;
    }

    // b1
    {
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(perFrameTotal);
        if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&perFrameBuffer))))
            return false;

        CD3DX12_RANGE readRange(0, 0);
        if (FAILED(perFrameBuffer->Map(0, &readRange, reinterpret_cast<void**>(&perFrameMapped))) || !perFrameMapped)
            return false;
    }

    // b2
    {
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(perInstanceTotal);
        if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&perInstanceBuffer))))
            return false;

        CD3DX12_RANGE readRange(0, 0);
        if (FAILED(perInstanceBuffer->Map(0, &readRange, reinterpret_cast<void**>(&perInstanceMapped))) || !perInstanceMapped)
            return false;
    }

    return true;
}

// ---------------------------------------------------------
// loadModel
// ---------------------------------------------------------
bool Exercise6Module::loadModel()
{
    const fs::path candidates[] =
    {
        fs::path("Game") / "Assets" / "Models" / "Duck" / "duck.gltf",
        fs::path("Game") / "Assets" / "Models" / "Duck" / "Duck.gltf",
        fs::path("Assets") / "Models" / "Duck" / "duck.gltf",
        fs::path("Assets") / "Models" / "Duck" / "Duck.gltf",
    };

    fs::path absGltf;
    const fs::path cwd = fs::current_path();
    const fs::path exeDir = GetExeDir();

    bool found = false;

    for (const fs::path& rel : candidates)
    {
        if (FindUpwards(cwd, rel, absGltf, 12) || FindUpwards(exeDir, rel, absGltf, 12))
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        LOG("Exercise6Module: Could not find Duck.gltf (CWD=%s, EXE=%s)",
            cwd.generic_string().c_str(),
            exeDir.generic_string().c_str());
        return false;
    }

    const fs::path absDir = absGltf.parent_path();
    const std::string gltfPath = ToGenericString(absGltf);
    const std::string basePath = EnsureTrailingSlash(ToGenericString(absDir));

    model.load(gltfPath.c_str(), basePath.c_str(), BasicMaterial::PHONG);
    model.scale() = Vector3(0.01f, 0.01f, 0.01f);

    return model.getNumMeshes() > 0;
}
