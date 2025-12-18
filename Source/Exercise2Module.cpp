#include "Globals.h"
#include "Exercise2Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ImGuiPass.h"
#include "ReadData.h"
#include "d3dx12.h"
#include <d3d12.h>

struct Exercise2Vertex
{
    float x, y, z;
};

bool Exercise2Module::init()
{
    bool ok = createVertexBuffer();
    ok = ok && createRootSignature();
    ok = ok && createPSO();

    return ok;
}

// -------------------------------------------------
// 1) Vertex Buffer
// -------------------------------------------------
bool Exercise2Module::createVertexBuffer()
{
    D3D12Module* d3d = app->getD3D12Module();
    ID3D12Device* device = d3d->getDevice();

    // Triangle in clip-space
    Exercise2Vertex vertices[3] =
    {
        { -1.0f, -1.0f, 0.0f },   // 0
        {  0.0f,  1.0f, 0.0f },   // 1
        {  1.0f, -1.0f, 0.0f }    // 2
    };

    const UINT vbSize = sizeof(vertices);

    // For simplicity: use an UPLOAD heap buffer (no ModuleResources here)
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer))))
    {
        return false;
    }

    // Copy CPU data into the buffer
    void* mapped = nullptr;
    CD3DX12_RANGE readRange(0, 0);   // CPU will not read from this resource
    vertexBuffer->Map(0, &readRange, &mapped);
    memcpy(mapped, vertices, vbSize);
    vertexBuffer->Unmap(0, nullptr);

    // Vertex Buffer View
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Exercise2Vertex);
    vertexBufferView.SizeInBytes = vbSize;

    return true;
}

// -------------------------------------------------
// 2) Empty Root Signature
// -------------------------------------------------
bool Exercise2Module::createRootSignature()
{
    D3D12Module* d3d = app->getD3D12Module();
    ID3D12Device* device = d3d->getDevice();

    CD3DX12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.Init(
        0, nullptr,   // 0 root parameters
        0, nullptr,   // 0 static samplers
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> serializedRS;
    ComPtr<ID3DBlob> errorRS;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRS,
        &errorRS
    );

    if (FAILED(hr))
    {
        if (errorRS)
            OutputDebugStringA((char*)errorRS->GetBufferPointer());
        return false;
    }

    if (FAILED(device->CreateRootSignature(
        0,
        serializedRS->GetBufferPointer(),
        serializedRS->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature))))
    {
        return false;
    }

    return true;
}

// -------------------------------------------------
// 3) PSO (Pipeline State Object)
// -------------------------------------------------
bool Exercise2Module::createPSO()
{
    D3D12Module* d3d = app->getD3D12Module();
    ID3D12Device* device = d3d->getDevice();

    // Input layout: position only
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "MY_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
          0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // DXIL shader bytecode
    auto dataVS = DX::ReadData(L"Exercise2VS.cso");
    auto dataPS = DX::ReadData(L"Exercise2PS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { dataVS.data(), dataVS.size() };
    psoDesc.PS = { dataPS.data(), dataPS.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.SampleDesc = { 1, 0 };
    psoDesc.SampleMask = 0xffffffff;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // Depth/stencil left at default (disabled)

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

// -------------------------------------------------
// 4) Triangle rendering
// -------------------------------------------------
void Exercise2Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), pso.Get());

    // PRESENT -> RENDER_TARGET
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            d3d12->getBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    LONG width = (LONG)d3d12->getWindowWidth();
    LONG height = (LONG)d3d12->getWindowHeight();

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, width, height };

    float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);

    // Render ImGui on top of the same render target
    if (ImGuiPass* imgui = d3d12->getImGuiPass())
    {
        imgui->record(commandList);
    }

    // RENDER_TARGET -> PRESENT
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
