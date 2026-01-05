#include "Globals.h"
#include "ModuleResources.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"
#include "DirectXTex.h"
#include <vector>

namespace
{
    bool LoadTextureFile(const std::wstring& filePath, DirectX::ScratchImage& image)
    {
        using namespace DirectX;

        if (SUCCEEDED(LoadFromDDSFile(filePath.c_str(), DDS_FLAGS_NONE, nullptr, image)))
            return true;

        if (SUCCEEDED(LoadFromTGAFile(filePath.c_str(), nullptr, image)))
            return true;

        return SUCCEEDED(LoadFromWICFile(filePath.c_str(), WIC_FLAGS_NONE, nullptr, image));
    }
}

ModuleResources::ModuleResources()
{
}

ModuleResources::~ModuleResources()
{
    cleanUp();
}

// --------------------------------------------------------------
// Debug name helpers
// --------------------------------------------------------------
std::wstring ModuleResources::utf8ToWString(const char* s)
{
    if (!s || s[0] == '\0')
        return {};

    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, s, (int)strlen(s), nullptr, 0);
    if (sizeNeeded <= 0)
        return {};

    std::wstring w;
    w.resize(sizeNeeded);
    MultiByteToWideChar(CP_UTF8, 0, s, (int)strlen(s), w.data(), sizeNeeded);
    return w;
}

void ModuleResources::setDebugName(ID3D12Object* obj, const wchar_t* nameW)
{
    if (!obj || !nameW || nameW[0] == L'\0')
        return;

    obj->SetName(nameW);
}

void ModuleResources::setDebugName(ID3D12Object* obj, const char* nameUtf8)
{
    if (!obj || !nameUtf8 || nameUtf8[0] == '\0')
        return;

    const std::wstring w = utf8ToWString(nameUtf8);
    if (!w.empty())
        obj->SetName(w.c_str());
}

// --------------------------------------------------------------
// init / cleanUp
// --------------------------------------------------------------
bool ModuleResources::init()
{
    // Get device and draw queue from D3D12Module
    D3D12Module* d3d = app ? app->getD3D12Module() : nullptr;
    if (!d3d)
        return false;

    m_device = d3d->getDevice();
    m_queue = d3d->getDrawCommandQueue();

    if (!m_device || !m_queue)
        return false;

    // Command allocator + command list used for copy/upload operations
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

    // Keep it closed; it will be Reset() before use
    m_cmdList->Close();

    // Fence to sync copy work with the GPU (FlushCopyQueue)
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
// UPLOAD BUFFER: CPU-writable (UPLOAD heap)
// --------------------------------------------------------------
ComPtr<ID3D12Resource> ModuleResources::createUploadBuffer(
    const void* cpuData,
    size_t      dataSize,
    const char* debugName)
{
    const std::wstring w = utf8ToWString(debugName);
    return createUploadBuffer(cpuData, dataSize, w.empty() ? nullptr : w.c_str());
}

ComPtr<ID3D12Resource> ModuleResources::createUploadBuffer(
    const void* cpuData,
    size_t          dataSize,
    const wchar_t* debugNameW)
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

    setDebugName(uploadBuffer.Get(), debugNameW);

    BYTE* pData = nullptr;
    CD3DX12_RANGE readRange(0, 0); // CPU will not read from this resource

    hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));
    if (FAILED(hr))
        return nullptr;

    memcpy(pData, cpuData, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    return uploadBuffer;
}

// --------------------------------------------------------------
// DEFAULT BUFFER: VRAM (uses an UPLOAD staging buffer + CopyResource)
// --------------------------------------------------------------
ComPtr<ID3D12Resource> ModuleResources::createDefaultBuffer(
    const void* cpuData,
    size_t      dataSize,
    const char* debugName)
{
    const std::wstring w = utf8ToWString(debugName);
    return createDefaultBuffer(cpuData, dataSize, w.empty() ? nullptr : w.c_str());
}

ComPtr<ID3D12Resource> ModuleResources::createDefaultBuffer(
    const void* cpuData,
    size_t          dataSize,
    const wchar_t* debugNameW)
{
    if (!m_device || !m_queue)
        return nullptr;

    // Name upload and default resources for PIX
    std::wstring uploadName;
    if (debugNameW && debugNameW[0] != L'\0')
    {
        uploadName = debugNameW;
        uploadName += L"_UPLOAD";
    }

    // 1) Create and fill staging buffer (UPLOAD)
    ComPtr<ID3D12Resource> uploadBuffer = createUploadBuffer(
        cpuData,
        dataSize,
        uploadName.empty() ? nullptr : uploadName.c_str());

    if (!uploadBuffer)
        return nullptr;

    // 2) Create final buffer in DEFAULT heap
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

    setDebugName(defaultBuffer.Get(), debugNameW);

    // 3) Copy UPLOAD -> DEFAULT using the internal command list
    m_allocator->Reset();
    m_cmdList->Reset(m_allocator.Get(), nullptr);

    m_cmdList->CopyResource(defaultBuffer.Get(), uploadBuffer.Get());
    m_cmdList->Close();

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_queue->ExecuteCommandLists(1, lists);

    // 4) Wait for the copy to finish (so the upload buffer can be released)
    FlushCopyQueue();

    return defaultBuffer;
}

// --------------------------------------------------------------
// FlushCopyQueue: wait for pending copy work to finish
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

// --------------------------------------------------------------
// createTextureFromFile: loads texture and generates mipmaps if missing
// --------------------------------------------------------------
ComPtr<ID3D12Resource> ModuleResources::createTextureFromFile(
    const std::wstring& filePath,
    const wchar_t* debugName)
{
    if (!m_device || !m_queue)
        return nullptr;

    DirectX::ScratchImage image;
    if (!LoadTextureFile(filePath, image))
        return nullptr;

    DirectX::TexMetadata meta = image.GetMetadata();

    // If the texture has no mip chain (common for WIC formats), generate mipmaps
    if (meta.mipLevels <= 1)
    {
        DirectX::ScratchImage mipChain;

        HRESULT hrMip = DirectX::GenerateMipMaps(
            image.GetImages(),
            image.GetImageCount(),
            meta,
            DirectX::TEX_FILTER_DEFAULT,
            0,
            mipChain
        );

        if (SUCCEEDED(hrMip))
        {
            image = std::move(mipChain);
            meta = image.GetMetadata();
        }
        else
        {
            OutputDebugStringA("ModuleResources: GenerateMipMaps FAILED, continuing without mipmaps\n");
        }
    }

    // Create final GPU texture in DEFAULT heap (start as COPY_DEST)
    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        meta.format,
        static_cast<UINT64>(meta.width),
        static_cast<UINT>(meta.height),
        static_cast<UINT16>(meta.arraySize),
        static_cast<UINT16>(meta.mipLevels)
    );

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

    ComPtr<ID3D12Resource> texture;
    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture)
    );

    if (FAILED(hr))
        return nullptr;

    // PIX-friendly: if debugName is not provided, use filePath
    if (debugName && debugName[0] != L'\0')
        setDebugName(texture.Get(), debugName);
    else
        setDebugName(texture.Get(), filePath.c_str());

    // Build subresource list: for each array item, for each mip level
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    subresources.reserve(image.GetImageCount());

    for (size_t item = 0; item < meta.arraySize; ++item)
    {
        for (size_t level = 0; level < meta.mipLevels; ++level)
        {
            const DirectX::Image* subImg = image.GetImage(level, item, 0);
            if (!subImg)
                return nullptr;

            D3D12_SUBRESOURCE_DATA data = {};
            data.pData = subImg->pixels;
            data.RowPitch = subImg->rowPitch;
            data.SlicePitch = subImg->slicePitch;
            subresources.push_back(data);
        }
    }

    // Create staging (UPLOAD) buffer for subresource upload
    const UINT64 uploadSize = GetRequiredIntermediateSize(
        texture.Get(),
        0,
        static_cast<UINT>(subresources.size())
    );

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    ComPtr<ID3D12Resource> staging;
    hr = m_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&staging)
    );

    if (FAILED(hr))
        return nullptr;

    // Name staging too (useful in PIX)
    {
        std::wstring stagingName = L"TextureUpload_";
        stagingName += (debugName && debugName[0] != L'\0') ? debugName : filePath.c_str();
        setDebugName(staging.Get(), stagingName.c_str());
    }

    // Record upload + transitions
    m_allocator->Reset();
    m_cmdList->Reset(m_allocator.Get(), nullptr);

    UpdateSubresources(
        m_cmdList.Get(),
        texture.Get(),
        staging.Get(),
        0, 0,
        static_cast<UINT>(subresources.size()),
        subresources.data()
    );

    // Transition to shader-readable state
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    m_cmdList->ResourceBarrier(1, &barrier);

    m_cmdList->Close();

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_queue->ExecuteCommandLists(1, lists);

    // Ensure staging can be safely released after returning
    FlushCopyQueue();

    return texture;
}
