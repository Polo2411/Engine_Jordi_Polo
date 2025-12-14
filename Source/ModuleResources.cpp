#include "Globals.h"
#include "ModuleResources.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"

ModuleResources::ModuleResources()
{
}

ModuleResources::~ModuleResources()
{
    cleanUp();
}

bool ModuleResources::init()
{
    // Obtenemos device y cola de dibujo desde el D3D12Module
    D3D12Module* d3d = app ? app->getD3D12Module() : nullptr;
    if (!d3d)
        return false;

    m_device = d3d->getDevice();
    m_queue = d3d->getDrawCommandQueue();

    if (!m_device || !m_queue)
        return false;

    // Command allocator + command list para copiar recursos
    HRESULT hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_allocator));
    if (FAILED(hr)) return false;

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_cmdList));
    if (FAILED(hr)) return false;

    // La dejamos cerrada, la abriremos con Reset cuando la usemos
    m_cmdList->Close();

    // Fence para sincronizar con la GPU (FlushCopyQueue)
    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) return false;

    m_fenceValue = 0;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    return true;
}

bool ModuleResources::cleanUp()
{
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_fence.Reset();
    m_cmdList.Reset();
    m_allocator.Reset();
    m_queue.Reset();
    m_device.Reset();

    return true;
}

// --------------------------------------------------------------
// UPLOAD BUFFER → CPU writable (heap UPLOAD)
// --------------------------------------------------------------
ComPtr<ID3D12Resource> ModuleResources::createUploadBuffer(
    const void* cpuData,
    size_t      dataSize,
    const char* name)
{
    if (!m_device)
        return nullptr;

    ComPtr<ID3D12Resource> uploadBuffer;

    CD3DX12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
    CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));

    if (FAILED(hr))
        return nullptr;

    if (name)
    {
        std::wstring wname(name, name + strlen(name));
        uploadBuffer->SetName(wname.c_str());
    }

    BYTE* pData = nullptr;
    CD3DX12_RANGE readRange(0, 0); // no vamos a leer desde CPU

    hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));
    if (FAILED(hr))
        return nullptr;

    memcpy(pData, cpuData, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    return uploadBuffer;
}

// --------------------------------------------------------------
// DEFAULT BUFFER → VRAM (usa staging UPLOAD + CopyResource)
// --------------------------------------------------------------
ComPtr<ID3D12Resource> ModuleResources::createDefaultBuffer(
    const void* cpuData,
    size_t      dataSize,
    const char* name)
{
    if (!m_device || !m_queue)
        return nullptr;

    // 1. crear staging buffer en UPLOAD y rellenarlo
    ComPtr<ID3D12Resource> uploadBuffer = createUploadBuffer(cpuData, dataSize, nullptr);
    if (!uploadBuffer)
        return nullptr;

    // 2. crear buffer final en DEFAULT
    ComPtr<ID3D12Resource> defaultBuffer;

    CD3DX12_RESOURCE_DESC   bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&defaultBuffer));

    if (FAILED(hr))
        return nullptr;

    // 3. Copia upload -> default con nuestra command list
    m_allocator->Reset();
    m_cmdList->Reset(m_allocator.Get(), nullptr);

    m_cmdList->CopyResource(defaultBuffer.Get(), uploadBuffer.Get());
    m_cmdList->Close();

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_queue->ExecuteCommandLists(1, lists);

    // 4. Esperar a que termine la copia
    FlushCopyQueue();

    if (name)
    {
        std::wstring wname(name, name + strlen(name));
        defaultBuffer->SetName(wname.c_str());
    }

    return defaultBuffer;
}

// --------------------------------------------------------------
// FlushCopyQueue → esperar a que terminen las copias
// --------------------------------------------------------------
void ModuleResources::FlushCopyQueue()
{
    if (!m_queue || !m_fence || !m_fenceEvent)
        return;

    const UINT64 fenceToWait = ++m_fenceValue;
    m_queue->Signal(m_fence.Get(), fenceToWait);

    if (m_fence->GetCompletedValue() < fenceToWait)
    {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
