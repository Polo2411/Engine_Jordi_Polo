#include "Globals.h"
#include "Assignment1Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleCamera.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplers.h"

#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "ReadData.h"

#include "UIModule.h"   // <- NUEVO

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

        ModuleResources* resources = app->getResources();
        ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

        texture = resources->createTextureFromFile(L"../Game/Assets/Textures/dog.dds");
        if (!texture)
            return false;

        textureSRV = descriptors->createSRV(texture.Get());
        if (textureSRV == UINT32_MAX)
            return false;

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
    textureSRV = UINT32_MAX;

    debugDrawPass.reset();

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

    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            d3d12->getBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    const unsigned width = d3d12->getWindowWidth();
    const unsigned height = d3d12->getWindowHeight();

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

    // -------- Read UI state (Assignment window lives in UIModule) --------
    bool showGrid = true;
    bool showAxis = true;

    if (UIModule* ui = app->getUIModule())
    {
        showGrid = ui->getShowGrid();
        showAxis = ui->getShowAxis();
        currentSampler = ui->getSelectedSampler();
    }

    // -------- Draw textured quad --------
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vbView);

    ID3D12DescriptorHeap* heaps[] =
    {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplers()->getHeap()
    };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / 4, &mvp, 0);
    commandList->SetGraphicsRootDescriptorTable(1, app->getShaderDescriptors()->getGPUHandle(textureSRV));
    commandList->SetGraphicsRootDescriptorTable(2, app->getSamplers()->getGPUHandle(currentSampler));

    commandList->DrawInstanced(6, 1, 0, 0);

    // -------- DebugDraw (grid + axis) controlled by UI --------
    if (showGrid)
        dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);

    if (showAxis)
        dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(commandList, width, height, view, proj);

    // -------- ImGui: only render draw data here --------
    if (ImGuiPass* uiPass = d3d12->getImGuiPass())
    {
        uiPass->record(commandList);
    }

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

    params[0].InitAsConstants(sizeof(Matrix) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

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
