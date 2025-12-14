#include "Globals.h"
#include "Exercise3Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleResources.h"
#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "ModuleCamera.h"
#include "TimeManager.h"
#include "ReadData.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

using namespace DirectX;

// ---------------------------------------------------------
// init
// ---------------------------------------------------------
bool Exercise3Module::init()
{
    struct Vertex
    {
        Vector3 position;
    };

    static Vertex vertices[3] =
    {
        { Vector3(-1.0f, -1.0f, 0.0f) },  // 0
        { Vector3(0.0f,  1.0f, 0.0f) },  // 1
        { Vector3(1.0f, -1.0f, 0.0f) }   // 2
    };

    bool ok = createVertexBuffer(&vertices[0], sizeof(vertices), sizeof(Vertex));
    ok = ok && createRootSignature();
    ok = ok && createPSO();

    if (ok)
    {
        D3D12Module* d3d12 = app->getD3D12Module();
        debugDrawPass = std::make_unique<DebugDrawPass>(
            d3d12->getDevice(),
            d3d12->getDrawCommandQueue()
        );

        // --- Para que la tecla F tenga algo a lo que enfocar ---
        // Ajusta el radio si quieres (triángulo: ~2, grid: ~10)
        if (ModuleCamera* cam = app->getCamera())
        {
            cam->setFocusBounds(Vector3::Zero, 10.0f);
        }

    }

    return ok;
}


// ---------------------------------------------------------
// render
// ---------------------------------------------------------
void Exercise3Module::render()
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

    // -------- Camera matrices (preferred) --------
    Matrix model = Matrix::Identity;

    Matrix view = Matrix::CreateLookAt(
        Vector3(0.0f, 10.0f, 10.0f),
        Vector3::Zero,
        Vector3::Up);

    Matrix proj = Matrix::CreatePerspectiveFieldOfView(
        XM_PIDIV4,
        (height > 0) ? (float(width) / float(height)) : 1.0f,
        0.1f,
        1000.0f);

    if (ModuleCamera* cam = app->getCamera())
    {
        // Ajusta nombres si tu clase usa otros getters
        view = cam->getViewMatrix();
        proj = cam->getProjectionMatrix();
    }

    mvp = (model * view * proj).Transpose();

    // -------- Viewport / Scissor --------
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

    // -------- Clear --------
    float clearColor[] = { 0.05f, 0.05f, 0.06f, 1.0f };

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(
        dsv,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    // -------- Draw triangle --------
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    commandList->SetGraphicsRoot32BitConstants(
        0,
        sizeof(XMMATRIX) / sizeof(UINT32),
        &mvp,
        0);

    commandList->DrawInstanced(3, 1, 0, 0);

    // -------- DebugDraw (grid + axes) --------
    dd::xzSquareGrid(-50.0f, 50.0f, 0.0f, 1.0f, dd::colors::LightGray);
    dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    debugDrawPass->record(commandList, width, height, view, proj);

    // -------- ImGui --------
    if (ImGuiPass* ui = d3d12->getImGuiPass())
    {
        TimeManager* tm = app->getTimeManager();

        ImGui::Begin("Exercise 3");
        ImGui::Text("Depth + DebugDraw + Camera");
        ImGui::Separator();

        if (tm)
        {
            ImGui::Text("FPS (avg): %.1f", tm->getFPS());
            ImGui::Text("Avg ms:   %.2f", tm->getAvgFrameMs());
            ImGui::Text("dt (real): %.4f s", tm->getRealDeltaTime());
            ImGui::Text("dt (game): %.4f s", tm->getDeltaTime());
            ImGui::PlotLines("Frame ms", tm->getFrameMsHistory(),
                (int)TimeManager::kHistorySize, 0, nullptr, 0.0f, 40.0f, ImVec2(0, 60));
        }
        else
        {
            ImGui::Text("TimeManager: nullptr");
            ImGui::Text("ImGui FPS: %.1f", ImGui::GetIO().Framerate);
        }

        ImGui::End();

        // Render ImGui draw data
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
            UINT(std::size(commandLists)),
            commandLists);
    }
}

// ---------------------------------------------------------
// createVertexBuffer
// ---------------------------------------------------------
bool Exercise3Module::createVertexBuffer(
    void* bufferData,
    unsigned bufferSize,
    unsigned stride)
{
    ModuleResources* resources = app->getResources();

    vertexBuffer = resources->createDefaultBuffer(
        bufferData,
        bufferSize,
        "Triangle");

    bool ok = (vertexBuffer != nullptr);

    if (ok)
    {
        vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexBufferView.SizeInBytes = bufferSize;
        vertexBufferView.StrideInBytes = stride;
    }

    return ok;
}

// ---------------------------------------------------------
// createRootSignature
// ---------------------------------------------------------
bool Exercise3Module::createRootSignature()
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    CD3DX12_ROOT_PARAMETER rootParameters[1];

    rootParameters[0].InitAsConstants(
        sizeof(Matrix) / sizeof(UINT32), 0);

    rootSignatureDesc.Init(
        1,
        rootParameters,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> rootSignatureBlob;

    if (FAILED(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &rootSignatureBlob,
        nullptr)))
    {
        return false;
    }

    D3D12Module* d3d12 = app->getD3D12Module();

    if (FAILED(d3d12->getDevice()->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature))))
    {
        return false;
    }

    return true;
}

// ---------------------------------------------------------
// createPSO
// ---------------------------------------------------------
bool Exercise3Module::createPSO()
{
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "MY_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto dataVS = DX::ReadData(L"Exercise3VS.cso");
    auto dataPS = DX::ReadData(L"Exercise3PS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {
        inputLayout,
        UINT(sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC))
    };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { dataVS.data(), dataVS.size() };
    psoDesc.PS = { dataPS.data(), dataPS.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc = { 1, 0 };
    psoDesc.SampleMask = 0xffffffff;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1;

    D3D12Module* d3d12 = app->getD3D12Module();
    return SUCCEEDED(d3d12->getDevice()->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pso)));
}
