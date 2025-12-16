#include "Globals.h"
#include "Exercise4Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleCamera.h"
#include "ModuleResources.h"
#include "ReadData.h"
#include "d3dx12.h"

struct Vertex
{
    Vector3 position;
    Vector2 uv;
};

bool Exercise4Module::init()
{
    bool ok = true;
    ok &= createVertexBuffer();
    ok &= createRootSignature();
    ok &= createPipelineState();
    return ok;
}

bool Exercise4Module::cleanUp()
{
    vertexBuffer.Reset();
    rootSignature.Reset();
    pso.Reset();
    return true;
}

void Exercise4Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ModuleCamera* camera = app->getCamera();

    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();
    cmd->Reset(d3d12->getCommandAllocator(), pso.Get());

    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            d3d12->getBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmd->ResourceBarrier(1, &barrier);

    const unsigned w = d3d12->getWindowWidth();
    const unsigned h = d3d12->getWindowHeight();

    Matrix model = Matrix::Identity;
    camera->setAspectRatio((h > 0) ? (float(w) / float(h)) : 1.0f);

    Matrix view = camera->getViewMatrix();
    Matrix proj = camera->getProjectionMatrix();

    Matrix mvp = (model * view * proj).Transpose();

    D3D12_VIEWPORT vp{ 0, 0, float(w), float(h), 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, (LONG)w, (LONG)h };

    auto rtv = d3d12->getRenderTargetDescriptor();
    auto dsv = d3d12->getDepthStencilDescriptor();

    float clearColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    cmd->OMSetRenderTargets(1, &rtv, false, &dsv);
    cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmd->SetGraphicsRootSignature(rootSignature.Get());
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &scissor);

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbView);

    cmd->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / 4, &mvp, 0);
    cmd->DrawInstanced(6, 1, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    cmd->ResourceBarrier(1, &barrier);

    cmd->Close();
    ID3D12CommandList* lists[] = { cmd };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
}

bool Exercise4Module::createVertexBuffer()
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

    auto* resources = app->getResources();
    vertexBuffer = resources->createDefaultBuffer(vertices, sizeof(vertices), "QuadVB");

    if (!vertexBuffer)
        return false;

    vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes = sizeof(vertices);
    vbView.StrideInBytes = sizeof(Vertex);

    return true;
}

bool Exercise4Module::createRootSignature()
{
    CD3DX12_ROOT_PARAMETER param;
    param.InitAsConstants(sizeof(Matrix) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(1, &param, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob;
    if (FAILED(D3D12SerializeRootSignature(&desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)))
        return false;

    return SUCCEEDED(
        app->getD3D12Module()->getDevice()->CreateRootSignature(
            0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)));
}

bool Exercise4Module::createPipelineState()
{
    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
          D3D12_APPEND_ALIGNED_ELEMENT,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto vs = DX::ReadData(L"Exercise4VS.cso");
    auto ps = DX::ReadData(L"Exercise4PS.cso");

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
            &psoDesc, IID_PPV_ARGS(&pso)));
}
