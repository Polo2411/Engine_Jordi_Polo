#include "Globals.h"
#include "ModuleShaderDescriptors.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"

bool ModuleShaderDescriptors::init()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12->getDevice();

    // Shader-visible CBV/SRV/UAV heap
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

    nextFreeIndex = 0;
    return true;
}

bool ModuleShaderDescriptors::cleanUp()
{
    heap.Reset();
    nextFreeIndex = 0;
    descriptorSize = 0;
    cpuStart = {};
    gpuStart = {};
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

uint32_t ModuleShaderDescriptors::allocate()
{
    if (nextFreeIndex >= MAX_DESCRIPTORS)
        return UINT32_MAX;

    return nextFreeIndex++;
}

uint32_t ModuleShaderDescriptors::createSRV(ID3D12Resource* texture)
{
    if (!texture)
        return UINT32_MAX;

    uint32_t index = allocate();
    if (index == UINT32_MAX)
        return UINT32_MAX;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;

    ID3D12Device* device = app->getD3D12Module()->getDevice();
    device->CreateShaderResourceView(texture, &srvDesc, getCPUHandle(index));

    return index;
}

void ModuleShaderDescriptors::reset()
{
    // Allows descriptor reuse (heap memory is not cleared)
    nextFreeIndex = 0;
}
