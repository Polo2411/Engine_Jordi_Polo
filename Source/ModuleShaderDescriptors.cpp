#include "Globals.h"
#include "ModuleShaderDescriptors.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"

ModuleShaderDescriptors::~ModuleShaderDescriptors()
{
    handles.forceCollectGarbage();
    _ASSERTE(handles.getFreeCount() == handles.getSize());
}

bool ModuleShaderDescriptors::init()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;
    if (!device)
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = NUM_DESCRIPTORS;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap))))
        return false;

    heap->SetName(L"Shader Descriptor Heap");

    descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
    gpuStart = heap->GetGPUDescriptorHandleForHeapStart();

    refCounts.fill(0);
    return true;
}

bool ModuleShaderDescriptors::cleanUp()
{
    handles.forceCollectGarbage();

    heap.Reset();
    cpuStart = {};
    gpuStart = {};
    descriptorSize = 0;
    refCounts.fill(0);

    return true;
}

void ModuleShaderDescriptors::preRender()
{
    collectGarbage();
}

ShaderTableDesc ModuleShaderDescriptors::allocTable()
{
    const uint32_t h = alloc();
    _ASSERTE(handles.validHandle(h));

    return ShaderTableDesc(h, &refCounts[indexFromHandle(h)]);
}

void ModuleShaderDescriptors::deferRelease(uint32_t handle)
{
    if (!handle)
        return;

    D3D12Module* d3d12 = app->getD3D12Module();
    _ASSERTE(d3d12);

    handles.deferRelease(handle, d3d12->getCurrentFrame());
}

void ModuleShaderDescriptors::collectGarbage()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    _ASSERTE(d3d12);

    handles.collectGarbage(d3d12->getLastCompletedFrame());
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getCPUHandle(uint32_t handle, uint8_t slot) const
{
    _ASSERTE(slot < DESCRIPTORS_PER_TABLE);
    _ASSERTE(isValid(handle));

    const uint32_t tableIndex = indexFromHandle(handle);
    const uint32_t linearIndex = tableIndex * DESCRIPTORS_PER_TABLE + uint32_t(slot);

    return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, int(linearIndex), int(descriptorSize));
}

D3D12_GPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getGPUHandle(uint32_t handle, uint8_t slot) const
{
    _ASSERTE(slot < DESCRIPTORS_PER_TABLE);
    _ASSERTE(isValid(handle));

    const uint32_t tableIndex = indexFromHandle(handle);
    const uint32_t linearIndex = tableIndex * DESCRIPTORS_PER_TABLE + uint32_t(slot);

    return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, int(linearIndex), int(descriptorSize));
}
