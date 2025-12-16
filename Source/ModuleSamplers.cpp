#include "Globals.h"
#include "ModuleSamplers.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"

bool ModuleSamplers::init()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12->getDevice();

    descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    heapDesc.NumDescriptors = (UINT)Type::Count;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap))))
        return false;

    heap->SetName(L"Sampler Descriptor Heap");

    cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
    gpuStart = heap->GetGPUDescriptorHandleForHeapStart();

    const float borderColor[4] = { 0.f, 0.f, 0.f, 1.f };

    auto makeSampler = [&](D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE mode)
        {
            D3D12_SAMPLER_DESC s = {};
            s.Filter = filter;
            s.AddressU = mode;
            s.AddressV = mode;
            s.AddressW = mode;
            s.MipLODBias = 0.0f;
            s.MaxAnisotropy = 1;
            s.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            s.BorderColor[0] = borderColor[0];
            s.BorderColor[1] = borderColor[1];
            s.BorderColor[2] = borderColor[2];
            s.BorderColor[3] = borderColor[3];
            s.MinLOD = 0.0f;
            s.MaxLOD = D3D12_FLOAT32_MAX;
            return s;
        };

    const D3D12_SAMPLER_DESC samplers[] =
    {
        makeSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
        makeSampler(D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_WRAP),

        makeSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
        makeSampler(D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP),

        makeSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR),
        makeSampler(D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_MIRROR),

        makeSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER),
        makeSampler(D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_BORDER),
    };

    for (uint32_t i = 0; i < (uint32_t)Type::Count; ++i)
    {
        auto cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, i, descriptorSize);
        device->CreateSampler(&samplers[i], cpu);
    }

    return true;
}

bool ModuleSamplers::cleanUp()
{
    heap.Reset();
    descriptorSize = 0;
    cpuStart = {};
    gpuStart = {};
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleSamplers::getCPUHandle(Type type) const
{
    const uint32_t idx = (uint32_t)type;
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, idx, descriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE ModuleSamplers::getGPUHandle(Type type) const
{
    const uint32_t idx = (uint32_t)type;
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, idx, descriptorSize);
}
