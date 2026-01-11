#include "Globals.h"
#include "Assignment1Module.h"

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

#include <filesystem>

#include <d3dcompiler.h>
#include "d3dx12.h"

#include "imgui.h"

using namespace DirectX;

struct Vertex
{
    Vector3 position;
    Vector2 uv;
};

// ---------------------------------------------------------
// UI (moved from UIModule)
// ---------------------------------------------------------
void Assignment1Module::imGuiCommands()
{
    TimeManager* tm = app ? app->getTimeManager() : nullptr;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("Rendering Settings", nullptr, flags))
    {
        // a) FPS
        if (tm)
        {
            ImGui::Text("FPS: %.1f", tm->getFPS());
            ImGui::Text("Frame Time: %.2f ms", tm->getAvgFrameMs());
        }
        else
        {
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        }

        ImGui::Separator();

        // b) Options to show/hide the grid and the axis
        ImGui::Checkbox("Show Grid", &showGrid);
        ImGui::Checkbox("Show Axis", &showAxis);

        ImGui::Separator();

        // c) Required sampler modes
        static const char* modes[] =
        {
            "Wrap + Bilinear",
            "Clamp + Bilinear",
            "Wrap + Point",
            "Clamp + Point"
        };

        int mode = 0;
        switch (currentSampler)
        {
        case ModuleSamplers::Type::Linear_Wrap:  mode = 0; break;
        case ModuleSamplers::Type::Linear_Clamp: mode = 1; break;
        case ModuleSamplers::Type::Point_Wrap:   mode = 2; break;
        case ModuleSamplers::Type::Point_Clamp:  mode = 3; break;
        default: mode = 0; break;
        }

        if (ImGui::Combo("Texture Sampling", &mode, modes, IM_ARRAYSIZE(modes)))
        {
            switch (mode)
            {
            case 0: currentSampler = ModuleSamplers::Type::Linear_Wrap;  break;
            case 1: currentSampler = ModuleSamplers::Type::Linear_Clamp; break;
            case 2: currentSampler = ModuleSamplers::Type::Point_Wrap;   break;
            case 3: currentSampler = ModuleSamplers::Type::Point_Clamp;  break;
            default: currentSampler = ModuleSamplers::Type::Linear_Wrap; break;
            }
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------
// init
// ---------------------------------------------------------
bool Assignment1Module::init()
{
    bool ok = true;

    ok &= createVertexBuffer();
    ok &= createRootSignature();
    ok &= createPipelineState();

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

        // Load texture and create SRV table
        ModuleResources* resources = app->getResources();
        ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

        namespace fs = std::filesystem;

        const wchar_t* pathDebug = L"../Game/Assets/Textures/WizardCat.jpg";
        const wchar_t* pathRelease = L"Game/Assets/Textures/WizardCat.jpg";

        if (fs::exists(pathRelease))
            texture = resources->createTextureFromFile(pathRelease, L"WizardCat");
        else if (fs::exists(pathDebug))
            texture = resources->createTextureFromFile(pathDebug, L"WizardCat");
        else
        {
            OutputDebugStringA("ERROR: WizardCat texture not found in any expected path\n");
            return false;
        }

        if (!texture)
            return false;

        textureTable = descriptors->allocTable();
        if (!textureTable)
            return false;

        textureTable.createTextureSRV(texture.Get(), 0);

        // Optional: focus bounds for camera "F" key
        if (ModuleCamera* cam = app->getCamera())
        {
            Vector3 center = Vector3::Zero;
            float radius = 1.5f;
            cam->setFocusBounds(center, radius);
        }
    }

    return ok;
}

// ---------------------------------------------------------
// cleanUp
// ---------------------------------------------------------
bool Assignment1Module::cleanUp()
{
    vertexBuffer.Reset();
    rootSignature.Reset();
    pso.Reset();

    texture.Reset();
    textureTable.reset();

    debugDrawPass.reset();

    // Reset UI state to defaults
    showGrid = true;
    showAxis = true;
    currentSampler = ModuleSamplers::Type::Linear_Wrap;

    return true;
}

// ---------------------------------------------------------
// render
// ---------------------------------------------------------
void Assignment1Module::render()
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

    // -------- Camera matrices --------
    Matrix model = Matrix::Identity;

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

    Matrix mvp = (model * view * proj).Transpose();

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

    float clearColor[] = { 0.05f, 0.05f, 0.06f, 1.0f };

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(
        dsv,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    // -------- Draw textured quad --------
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vbView);

    // Bind shader-visible descriptor heaps (SRV heap + Sampler heap)
    ID3D12DescriptorHeap* heaps[] =
    {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplers()->getHeap()
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Root bindings: MVP constants + SRV table + sampler table
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / 4, &mvp, 0);
    commandList->SetGraphicsRootDescriptorTable(1, textureTable.getGPUHandle(0));
    commandList->SetGraphicsRootDescriptorTable(2, app->getSamplers()->getGPUHandle(currentSampler));

    commandList->DrawInstanced(6, 1, 0, 0);

    // -------- DebugDraw (grid + axis) controlled by this module UI --------
    if (showGrid)
        dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);

    if (showAxis)
        dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(commandList, width, height, view, proj);

    // -------- ImGui: build window + record draw data --------
    if (ImGuiPass* uiPass = d3d12->getImGuiPass())
    {
        imGuiCommands();
        uiPass->record(commandList);
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
            UINT(std::size(commandLists)),
            commandLists);
    }
}

// ---------------------------------------------------------
// createVertexBuffer
// ---------------------------------------------------------
bool Assignment1Module::createVertexBuffer()
{
    static const Vertex vertices[6] =
    {
        {{-1.f, -1.f, 0.f}, {-0.2f,  1.2f}},
        {{-1.f,  1.f, 0.f}, {-0.2f, -0.2f}},
        {{ 1.f,  1.f, 0.f}, { 1.2f, -0.2f}},

        {{-1.f, -1.f, 0.f}, {-0.2f,  1.2f}},
        {{ 1.f,  1.f, 0.f}, { 1.2f, -0.2f}},
        {{ 1.f, -1.f, 0.f}, { 1.2f,  1.2f}},
    };

    ModuleResources* resources = app->getResources();
    vertexBuffer = resources->createDefaultBuffer(vertices, sizeof(vertices), "QuadVB");
    if (!vertexBuffer)
        return false;

    vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes = sizeof(vertices);
    vbView.StrideInBytes = sizeof(Vertex);

    return true;
}

// ---------------------------------------------------------
// createRootSignature
// ---------------------------------------------------------
bool Assignment1Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER params[3] = {};

    // b0: MVP constants
    params[0].InitAsConstants(sizeof(Matrix) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    // t0: texture SRV table
    CD3DX12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // s0: sampler table
    CD3DX12_DESCRIPTOR_RANGE sampRange = {};
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
    params[2].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        _countof(params), params,
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
bool Assignment1Module::createPipelineState()
{
    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
          D3D12_APPEND_ALIGNED_ELEMENT,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto vs = DX::ReadData(L"Assignment1VS.cso");
    auto ps = DX::ReadData(L"Assignment1PS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = { layout, _countof(layout) };
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
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    return SUCCEEDED(
        app->getD3D12Module()->getDevice()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(&pso))
    );
}
