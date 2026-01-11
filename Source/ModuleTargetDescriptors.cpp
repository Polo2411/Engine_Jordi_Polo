#include "Globals.h"
#include "ModuleTargetDescriptors.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"

ModuleTargetDescriptors::~ModuleTargetDescriptors()
{
    _ASSERTE(rtHandles.getFreeCount() == rtHandles.getSize());
    _ASSERTE(dsHandles.getFreeCount() == dsHandles.getSize());
}

bool ModuleTargetDescriptors::init()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    ID3D12Device* device = d3d12 ? d3d12->getDevice() : nullptr;
    if (!device)
        return false;

    rtvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = MAX_RTV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heapRTV))))
            return false;

        rtvStart = heapRTV->GetCPUDescriptorHandleForHeapStart();
        heapRTV->SetName(L"RTV Heap");
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = MAX_DSV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heapDSV))))
            return false;

        dsvStart = heapDSV->GetCPUDescriptorHandleForHeapStart();
        heapDSV->SetName(L"DSV Heap");
    }

    // Keep refCounts intact (do not zero them in cleanUp).
    // It's fine to initialize them once here.
    rtRefCounts.fill(0);
    dsRefCounts.fill(0);

    return true;
}

bool ModuleTargetDescriptors::cleanUp()
{
    // IMPORTANT:
    // Do NOT reset refCounts here. Some RenderTargetDesc/DepthStencilDesc objects
    // may still be destroyed after cleanUp(), and they need valid counters.

    heapRTV.Reset();
    heapDSV.Reset();

    rtvStart = {};
    dsvStart = {};
    rtvInc = 0;
    dsvInc = 0;

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleTargetDescriptors::getRTCPUHandle(uint32_t handle) const
{
    _ASSERTE(isValidRT(handle));
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStart, int(indexFromRT(handle)), int(rtvInc));
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleTargetDescriptors::getDSCPUHandle(uint32_t handle) const
{
    _ASSERTE(isValidDS(handle));
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvStart, int(indexFromDS(handle)), int(dsvInc));
}

RenderTargetDesc ModuleTargetDescriptors::createRT(ID3D12Resource* resource)
{
    _ASSERTE(resource);

    uint32_t handle = rtHandles.allocHandle();
    _ASSERTE(rtHandles.validHandle(handle));

    ID3D12Device* device = app->getD3D12Module()->getDevice();
    device->CreateRenderTargetView(resource, nullptr, getRTCPUHandle(handle));

    return RenderTargetDesc(handle, &rtRefCounts[indexFromRT(handle)]);
}

RenderTargetDesc ModuleTargetDescriptors::createRT(ID3D12Resource* resource, UINT arraySlice, UINT mipSlice, DXGI_FORMAT format)
{
    _ASSERTE(resource);

    uint32_t handle = rtHandles.allocHandle();
    _ASSERTE(rtHandles.validHandle(handle));

    D3D12_RENDER_TARGET_VIEW_DESC rtv{};
    rtv.Format = format;
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtv.Texture2DArray.ArraySize = 1;
    rtv.Texture2DArray.FirstArraySlice = arraySlice;
    rtv.Texture2DArray.MipSlice = mipSlice;
    rtv.Texture2DArray.PlaneSlice = 0;

    ID3D12Device* device = app->getD3D12Module()->getDevice();
    device->CreateRenderTargetView(resource, &rtv, getRTCPUHandle(handle));

    return RenderTargetDesc(handle, &rtRefCounts[indexFromRT(handle)]);
}

DepthStencilDesc ModuleTargetDescriptors::createDS(ID3D12Resource* resource)
{
    _ASSERTE(resource);

    uint32_t handle = dsHandles.allocHandle();
    _ASSERTE(dsHandles.validHandle(handle));

    ID3D12Device* device = app->getD3D12Module()->getDevice();
    device->CreateDepthStencilView(resource, nullptr, getDSCPUHandle(handle));

    return DepthStencilDesc(handle, &dsRefCounts[indexFromDS(handle)]);
}

void ModuleTargetDescriptors::releaseRT(uint32_t handle)
{
    if (!handle)
        return;

    rtHandles.freeHandle(handle);
}

void ModuleTargetDescriptors::releaseDS(uint32_t handle)
{
    if (!handle)
        return;

    dsHandles.freeHandle(handle);
}
