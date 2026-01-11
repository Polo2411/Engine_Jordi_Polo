#include "Globals.h"
#include "Exercise7Module.h"

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
#include <cmath>

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

    uint32_t ClampMin1(uint32_t v) { return (v == 0u) ? 1u : v; }

    double UpdateAvgMs(double* history, int window, int& idx, int& count, double ms)
    {
        history[idx] = ms;
        idx = (idx + 1) % window;
        count = (count < window) ? (count + 1) : window;

        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += history[i];
        return (count > 0) ? (sum / double(count)) : 0.0;
    }

    void UpdateCameraPivotToModel(ModuleCamera* cam, const Matrix& modelM)
    {
        if (!cam) return;
        const Vector3 center(modelM._41, modelM._42, modelM._43);
        cam->setFocusBounds(center, 2.5f);
    }
}

// ---------------------------------------------------------
// init
// ---------------------------------------------------------
bool Exercise7Module::init()
{
    bool ok = true;

    ok &= createRootSignature();
    ok &= createPipelineState();
    ok &= loadModel();
    ok &= createFrameBuffers();

    if (ok)
    {
        UpdateCameraPivotToModel(app->getCamera(), model.getModelMatrix());

        D3D12Module* d3d12 = app->getD3D12Module();

        Microsoft::WRL::ComPtr<ID3D12Device4> device4;
        if (FAILED(d3d12->getDevice()->QueryInterface(IID_PPV_ARGS(&device4))))
            return false;

        debugDrawPass = std::make_unique<DebugDrawPass>(device4.Get(), d3d12->getDrawCommandQueue());

        // Create Scene RenderTexture (start at 1x1; real size comes from ImGui window)
        const Vector4 clearCol(0.05f, 0.05f, 0.06f, 1.0f);
        sceneRT = std::make_unique<RenderTexture>(
            "SceneRT",
            DXGI_FORMAT_R8G8B8A8_UNORM,
            clearCol,
            DXGI_FORMAT_D32_FLOAT,
            1.0f,
            false,
            false
        );
        sceneRT->resize(1, 1);
        lastSceneW = 1;
        lastSceneH = 1;
    }

    return ok;
}

// ---------------------------------------------------------
// cleanUp
// ---------------------------------------------------------
bool Exercise7Module::cleanUp()
{
    debugDrawPass.reset();

    sceneRT.reset();
    lastSceneW = 1;
    lastSceneH = 1;

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
Matrix Exercise7Module::computeNormalMatrixSafe(const Matrix& modelM)
{
    Matrix m = modelM;
    m._41 = 0.0f; m._42 = 0.0f; m._43 = 0.0f;

    const float det = m.Determinant();
    if (fabsf(det) < 1e-8f)
        return Matrix::Identity;

    return m.Invert();
}

// ---------------------------------------------------------
// Step 1 (PowerPoint): Execute ImGui Commands + detect resize + show texture
// ---------------------------------------------------------
void Exercise7Module::buildImGuiAndHandleResize(const Matrix& view, const Matrix& proj, uint32_t& outSceneW, uint32_t& outSceneH)
{
    // Scene window (left): show RenderTexture
    ImGui::SetNextWindowSize(ImVec2(900.0f, 650.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene");

    // Content region size (PowerPoint style)
    const ImVec2 crMin = ImGui::GetWindowContentRegionMin();
    const ImVec2 crMax = ImGui::GetWindowContentRegionMax();
    ImVec2 size = ImVec2(crMax.x - crMin.x, crMax.y - crMin.y);

    // Prevent 0-sized RTs
    const uint32_t w = ClampMin1((uint32_t)(size.x > 1.0f ? size.x : 1.0f));
    const uint32_t h = ClampMin1((uint32_t)(size.y > 1.0f ? size.y : 1.0f));

    // Resize flow (PowerPoint): [ImGui Size Change] -> [Flush GPU] -> [Recreate Textures] -> [Update Descriptors]
    if (sceneRT && (w != lastSceneW || h != lastSceneH))
    {
        // Critical Warning: Never release resources while GPU might be using them -> flush
        if (D3D12Module* d3d12 = app->getD3D12Module())
            d3d12->flush();

        sceneRT->resize(w, h);

        lastSceneW = w;
        lastSceneH = h;
    }

    // Display the texture in ImGui
    if (sceneRT && sceneRT->isValid())
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = sceneRT->getSrvHandle();
        ImGui::Image((ImTextureID)handle.ptr, size);
    }

    ImGui::End();

    // Right window: options + gizmo
    imGuiOptionsAndGizmo(view, proj);

    outSceneW = lastSceneW;
    outSceneH = lastSceneH;
}

// ---------------------------------------------------------
// Options window (same look as Exercise6 screenshot) + ImGuizmo overlay
// ---------------------------------------------------------
void Exercise7Module::imGuiOptionsAndGizmo(const Matrix& view, const Matrix& proj)
{
    const double dt = app ? app->getDeltaTimeSeconds() : 0.0;
    const double ms = (dt > 0.0) ? (dt * 1000.0) : 0.0;
    const double avgMs = UpdateAvgMs(msHistory, kAvgWindow, msIndex, msCount, ms);
    const uint32_t fps = (dt > 0.0) ? uint32_t(1.0 / dt) : 0;

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
        UpdateCameraPivotToModel(app->getCamera(), objectMatrix);
    }

    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Light Direction", reinterpret_cast<float*>(&light.L), 0.1f, -1.0f, 1.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("Normalize"))
            light.L.Normalize();

        ImGui::ColorEdit3("Light Colour", reinterpret_cast<float*>(&light.Lc), ImGuiColorEditFlags_NoAlpha);
        ImGui::ColorEdit3("Ambient Colour", reinterpret_cast<float*>(&light.Ac), ImGuiColorEditFlags_NoAlpha);
    }

    auto& mats = model.getMaterials();
    for (size_t i = 0; i < mats.size(); ++i)
    {
        BasicMaterial& mat = mats[i];
        if (mat.getMaterialType() != BasicMaterial::PHONG)
            continue;

        char header[256]{};
        _snprintf_s(header, 255, "Material %s", mat.getName());

        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            PhongMaterialData ph = mat.getPhongMaterial();
            bool dirty = false;

            dirty |= ImGui::ColorEdit3("Diffuse Colour (Cd)", reinterpret_cast<float*>(&ph.diffuseColour), ImGuiColorEditFlags_NoAlpha);
            dirty |= ImGui::IsItemDeactivatedAfterEdit();

            bool useTex = (ph.hasDiffuseTex != FALSE);
            if (ImGui::Checkbox("Use Texture", &useTex))
            {
                ph.hasDiffuseTex = useTex ? TRUE : FALSE;
                dirty = true;
            }

            dirty |= ImGui::ColorEdit3("Specular Colour (F0)", reinterpret_cast<float*>(&ph.specularColour), ImGuiColorEditFlags_NoAlpha);
            dirty |= ImGui::IsItemDeactivatedAfterEdit();

            dirty |= ImGui::DragFloat("Shininess (n)", &ph.shininess, 1.0f, 1.0f, 2048.0f);
            dirty |= ImGui::IsItemDeactivatedAfterEdit();

            if (dirty)
                mat.setPhongMaterial(ph);
        }
    }

    ImGui::End();

    if (!showGuizmo)
        return;

    D3D12Module* d3d12 = app->getD3D12Module();
    const unsigned winW = d3d12->getWindowWidth();
    const unsigned winH = d3d12->getWindowHeight();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, float(winW), float(winH));

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (gizmoOperation == 1) op = ImGuizmo::ROTATE;
    if (gizmoOperation == 2) op = ImGuizmo::SCALE;

    Matrix objectMatrix = model.getModelMatrix();

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
        UpdateCameraPivotToModel(app->getCamera(), objectMatrix);
    }
}

// ---------------------------------------------------------
// render (PowerPoint 5 steps)
// ---------------------------------------------------------
void Exercise7Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    if (!d3d12 || !sceneRT)
        return;

    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();
    if (!commandList)
        return;

    // Compute view/proj based on the Scene render texture size (ImGui window size)
    // We will fill these after Step 1, but we need valid defaults.
    Matrix view = Matrix::Identity;
    Matrix proj = Matrix::Identity;

    // Begin ImGuizmo frame once per frame (CPU)
    ImGuizmo::BeginFrame();

    // Step 1: Execute ImGui Commands (CPU) + detect scene window size + resize render texture if needed
    uint32_t sceneW = lastSceneW;
    uint32_t sceneH = lastSceneH;

    {
        // Camera matrices should match Scene texture aspect
        if (ModuleCamera* cam = app->getCamera())
        {
            cam->setAspectRatio((sceneH > 0) ? (float(sceneW) / float(sceneH)) : 1.0f);
            view = cam->getViewMatrix();
            proj = cam->getProjectionMatrix();
        }
        else
        {
            view = Matrix::CreateLookAt(Vector3(0.0f, 2.0f, 6.0f), Vector3::Zero, Vector3::Up);
            proj = Matrix::CreatePerspectiveFieldOfView(
                XM_PIDIV4,
                (sceneH > 0) ? (float(sceneW) / float(sceneH)) : 1.0f,
                0.1f, 1000.0f);
        }

        buildImGuiAndHandleResize(view, proj, sceneW, sceneH);

        // Recompute matrices after a possible resize (aspect can change)
        if (ModuleCamera* cam = app->getCamera())
        {
            cam->setAspectRatio((sceneH > 0) ? (float(sceneW) / float(sceneH)) : 1.0f);
            view = cam->getViewMatrix();
            proj = cam->getProjectionMatrix();
        }
    }

    // Step 2: Reset Command List to start recording state
    commandList->Reset(d3d12->getCommandAllocator(), pso.Get());

    BEGIN_EVENT(commandList, "Exercise7 Frame");

    // Frame slot for ringed CBs
    const uint32_t frameSlot = (d3d12->getCurrentFrame() % kFramesInFlight);

    // Update CBs (same as Exercise6)
    {
        const Matrix mvp = (model.getModelMatrix() * view * proj).Transpose();
        if (mvpMapped)
        {
            MVPData cb{};
            cb.mvp = mvp;
            memcpy(mvpMapped + (frameSlot * mvpStride), &cb, sizeof(cb));
        }

        if (perFrameMapped)
        {
            PerFrameData pf{};
            pf.L = light.L;
            pf.L.Normalize();
            pf.Lc = light.Lc;
            pf.Ac = light.Ac;

            if (ModuleCamera* cam = app->getCamera())
                pf.viewPos = cam->getPosition();
            else
                pf.viewPos = Vector3(0.0f, 2.0f, 6.0f);

            memcpy(perFrameMapped + (frameSlot * perFrameStride), &pf, sizeof(pf));
        }
    }

    // Common pipeline state
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    // Bind shader visible heaps (CBV/SRV/UAV + Samplers)
    ID3D12DescriptorHeap* heaps[] =
    {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplers()->getHeap()
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootConstantBufferView(
        0, mvpBuffer->GetGPUVirtualAddress() + (frameSlot * mvpStride));

    commandList->SetGraphicsRootConstantBufferView(
        1, perFrameBuffer->GetGPUVirtualAddress() + (frameSlot * perFrameStride));

    commandList->SetGraphicsRootDescriptorTable(
        4, app->getSamplers()->getGPUHandle(currentSampler));

    // Step 3: Scene Render Pass (Render to Texture + Depth) + DebugDraw
    {
        BEGIN_EVENT(commandList, "Scene Pass -> RenderTexture");

        // Critical: transition SRV -> RTV and bind RT+DS (RenderTexture handles this)
        sceneRT->beginRender(commandList);

        // Draw model
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

            if (perInstanceMapped)
            {
                PerInstanceData pi{};

                const Matrix modelM = model.getModelMatrix();
                const Matrix invNoTranslation = computeNormalMatrixSafe(modelM);

                pi.modelMat = modelM.Transpose();
                pi.normalMat = invNoTranslation.Transpose();
                pi.material = mat.getPhongMaterial();

                const size_t instanceOffset = (frameSlot * meshCount + meshIdx) * perInstanceStride;
                memcpy(perInstanceMapped + instanceOffset, &pi, sizeof(pi));

                commandList->SetGraphicsRootConstantBufferView(
                    2, perInstanceBuffer->GetGPUVirtualAddress() + instanceOffset);
            }

            commandList->SetGraphicsRootDescriptorTable(3, mat.getTexturesTableGPU());
            mesh.draw(commandList);
        }

        // Debug draw in the SAME render target texture (as pwp says)
        {
            if (showGrid)
                dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);

            if (showAxis)
                dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

            if (debugDrawPass)
                debugDrawPass->record(commandList, sceneW, sceneH, view, proj);
        }

        // Critical: transition RTV -> SRV (RenderTexture handles this)
        sceneRT->endRender(commandList);

        END_EVENT(commandList);
    }

    // Step 4: ImGui Render Pass (Backbuffer + Fullscreen Depth)
    {
        BEGIN_EVENT(commandList, "ImGui Pass -> Backbuffer");

        // Backbuffer PRESENT -> RENDER_TARGET
        {
            CD3DX12_RESOURCE_BARRIER barrier =
                CD3DX12_RESOURCE_BARRIER::Transition(
                    d3d12->getBackBuffer(),
                    D3D12_RESOURCE_STATE_PRESENT,
                    D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &barrier);
        }

        const unsigned winW = d3d12->getWindowWidth();
        const unsigned winH = d3d12->getWindowHeight();

        // Viewport/scissor MUST be window size (not ImGui size) as per pwp
        D3D12_VIEWPORT vp{ 0.0f, 0.0f, float(winW), float(winH), 0.0f, 1.0f };
        D3D12_RECT sc{ 0, 0, LONG(winW), LONG(winH) };
        commandList->RSSetViewports(1, &vp);
        commandList->RSSetScissorRects(1, &sc);

        // Bind backbuffer + depth
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        // Optional clear (keeps a clean background)
        {
            float clearColor[] = { 0.05f, 0.05f, 0.06f, 1.0f };
            commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        }

        // Record ImGui draw data
        if (ImGuiPass* ui = d3d12->getImGuiPass())
            ui->record(commandList);

        // Backbuffer RENDER_TARGET -> PRESENT
        {
            CD3DX12_RESOURCE_BARRIER barrier =
                CD3DX12_RESOURCE_BARRIER::Transition(
                    d3d12->getBackBuffer(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT);
            commandList->ResourceBarrier(1, &barrier);
        }

        END_EVENT(commandList);
    }

    END_EVENT(commandList);

    // Step 5: Execute command list & present (present happens in D3D12Module::postRender)
    if (SUCCEEDED(commandList->Close()))
    {
        ID3D12CommandList* lists[] = { commandList };
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(UINT(std::size(lists)), lists);
    }
}

// ---------------------------------------------------------
// createRootSignature (same as Exercise6)
// ---------------------------------------------------------
bool Exercise7Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootParameters[5] = {};
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, BasicMaterial::SLOT_COUNT, 0); // t0
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);                   // s0

    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);     // b0
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);        // b1
    rootParameters[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);        // b2
    rootParameters[3].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL); // t0
    rootParameters[4].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);// s0

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
            LOG("Exercise7Module RootSignature serialize error: %s", (const char*)errorBlob->GetBufferPointer());
        return false;
    }

    HRESULT hr = app->getD3D12Module()->getDevice()->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));

    if (FAILED(hr))
        return false;

    rootSignature->SetName(L"Exercise7 RootSignature");
    return true;
}

// ---------------------------------------------------------
// createPipelineState (Exercise7VS/PS)
// ---------------------------------------------------------
bool Exercise7Module::createPipelineState()
{
    auto vs = DX::ReadData(L"Exercise7VS.cso");
    auto ps = DX::ReadData(L"Exercise7PS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = BasicMesh::getInputLayoutDesc();
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { vs.data(), vs.size() };
    psoDesc.PS = { ps.data(), ps.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // RenderTexture colour + depth formats
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

    pso->SetName(L"Exercise7 PSO");
    return true;
}

// ---------------------------------------------------------
// createFrameBuffers (same as Exercise6)
// ---------------------------------------------------------
bool Exercise7Module::createFrameBuffers()
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
// loadModel (same search as Exercise6)
// ---------------------------------------------------------
bool Exercise7Module::loadModel()
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
        LOG("Exercise7Module: Could not find Duck.gltf (CWD=%s, EXE=%s)",
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
