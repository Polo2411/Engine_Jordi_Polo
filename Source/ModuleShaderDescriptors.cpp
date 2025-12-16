#include "Globals.h"
#include "ModuleShaderDescriptors.h"

#include "Application.h"
#include "D3D12Module.h"

bool ModuleShaderDescriptors::init()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12->getDevice();

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = MAX_DESCRIPTORS;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap))))
        return false;

    heap->SetName(L"Shader Descriptor Heap (CBV/SRV/UAV)");

    descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
    gpuStart = heap->GetGPUDescriptorHandleForHeapStart();

    return true;
}

bool ModuleShaderDescriptors::cleanUp()
{
    heap.Reset();
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getCPUHandle(uint32_t index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, index, descriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getGPUHandle(uint32_t index) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, index, descriptorSize);
}
