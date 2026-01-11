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

    UINT ClampSampleCount(UINT sc)
    {
        if (sc == 0) return 1;
        return sc;
    }
}

ModuleResources::ModuleResources()
{
}

ModuleResources::~ModuleResources()
{
    cleanUp();
}

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

bool ModuleResources::init()
{
    D3D12Module* d3d = app ? app->getD3D12Module() : nullptr;
    if (!d3d)
        return false;

    m_device = d3d->getDevice();
    m_queue = d3d->getDrawCommandQueue();

    if (!m_device || !m_queue)
        return false;

    HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator));
    if (FAILED(hr)) return false;

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_cmdList));
    if (FAILED(hr)) return false;

    m_cmdList->Close();

    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) return false;

    m_fenceValue = 0;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    deferred.clear();
    return true;
}

void ModuleResources::preRender()
{
    collectGarbage();
}

bool ModuleResources::cleanUp()
{
    deferred.clear();

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

void ModuleResources::deferRelease(ComPtr<ID3D12Resource>& resource)
{
    if (!resource)
        return;

    D3D12Module* d3d = app ? app->getD3D12Module() : nullptr;
    if (!d3d)
    {
        resource.Reset();
        return;
    }

    DeferredResource dr;
    dr.resource = resource;
    dr.frame = d3d->getCurrentFrame();
    deferred.push_back(std::move(dr));

    resource.Reset();
}

void ModuleResources::collectGarbage()
{
    D3D12Module* d3d = app ? app->getD3D12Module() : nullptr;
    if (!d3d)
        return;

    const unsigned completed = d3d->getLastCompletedFrame();

    for (size_t i = 0; i < deferred.size();)
    {
        if (deferred[i].frame <= completed)
        {
            deferred[i] = deferred.back();
            deferred.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

ComPtr<ID3D12Resource> ModuleResources::createUploadBuffer(
    const void* cpuData,
    size_t dataSize,
    const char* debugName)
{
    const std::wstring w = utf8ToWString(debugName);
    return createUploadBuffer(cpuData, dataSize, w.empty() ? nullptr : w.c_str());
}

ComPtr<ID3D12Resource> ModuleResources::createUploadBuffer(
    const void* cpuData,
    size_t dataSize,
    const wchar_t* debugNameW)
{
    if (!m_device)
        return nullptr;

    ComPtr<ID3D12Resource> uploadBuffer;

    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

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

    if (cpuData && dataSize > 0)
    {
        BYTE* pData = nullptr;
        CD3DX12_RANGE readRange(0, 0);

        hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));
        if (FAILED(hr))
            return nullptr;

        memcpy(pData, cpuData, dataSize);
        uploadBuffer->Unmap(0, nullptr);
    }

    return uploadBuffer;
}

ComPtr<ID3D12Resource> ModuleResources::createDefaultBuffer(
    const void* cpuData,
    size_t dataSize,
    const char* debugName)
{
    const std::wstring w = utf8ToWString(debugName);
    return createDefaultBuffer(cpuData, dataSize, w.empty() ? nullptr : w.c_str());
}

ComPtr<ID3D12Resource> ModuleResources::createDefaultBuffer(
    const void* cpuData,
    size_t dataSize,
    const wchar_t* debugNameW)
{
    if (!m_device || !m_queue || !cpuData || dataSize == 0)
        return nullptr;

    std::wstring uploadName;
    if (debugNameW && debugNameW[0] != L'\0')
    {
        uploadName = debugNameW;
        uploadName += L"_UPLOAD";
    }

    ComPtr<ID3D12Resource> uploadBuffer = createUploadBuffer(
        cpuData,
        dataSize,
        uploadName.empty() ? nullptr : uploadName.c_str());

    if (!uploadBuffer)
        return nullptr;

    ComPtr<ID3D12Resource> defaultBuffer;

    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
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

    m_allocator->Reset();
    m_cmdList->Reset(m_allocator.Get(), nullptr);

    m_cmdList->CopyResource(defaultBuffer.Get(), uploadBuffer.Get());
    m_cmdList->Close();

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_queue->ExecuteCommandLists(1, lists);

    FlushCopyQueue();
    return defaultBuffer;
}

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
    }

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

    if (debugName && debugName[0] != L'\0')
        setDebugName(texture.Get(), debugName);
    else
        setDebugName(texture.Get(), filePath.c_str());

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

    {
        std::wstring stagingName = L"TextureUpload_";
        stagingName += (debugName && debugName[0] != L'\0') ? debugName : filePath.c_str();
        setDebugName(staging.Get(), stagingName.c_str());
    }

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

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    m_cmdList->ResourceBarrier(1, &barrier);

    m_cmdList->Close();

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_queue->ExecuteCommandLists(1, lists);

    FlushCopyQueue();
    return texture;
}

ComPtr<ID3D12Resource> ModuleResources::createRenderTarget(
    DXGI_FORMAT format,
    size_t width,
    size_t height,
    UINT sampleCount,
    const Vector4& clearColor,
    const char* debugName)
{
    if (!m_device || width == 0 || height == 0)
        return nullptr;

    sampleCount = ClampSampleCount(sampleCount);

    D3D12_CLEAR_VALUE clear = {};
    clear.Format = format;
    clear.Color[0] = clearColor.x;
    clear.Color[1] = clearColor.y;
    clear.Color[2] = clearColor.z;
    clear.Color[3] = clearColor.w;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
        format,
        static_cast<UINT64>(width),
        static_cast<UINT>(height),
        1,
        1,
        sampleCount,
        0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    );

    ComPtr<ID3D12Resource> tex;
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        &clear,
        IID_PPV_ARGS(&tex)
    );

    if (FAILED(hr))
        return nullptr;

    setDebugName(tex.Get(), debugName);
    return tex;
}

ComPtr<ID3D12Resource> ModuleResources::createDepthStencil(
    DXGI_FORMAT format,
    size_t width,
    size_t height,
    UINT sampleCount,
    float clearDepth,
    UINT8 clearStencil,
    const char* debugName)
{
    if (!m_device || width == 0 || height == 0)
        return nullptr;

    sampleCount = ClampSampleCount(sampleCount);

    D3D12_CLEAR_VALUE clear = {};
    clear.Format = format;
    clear.DepthStencil.Depth = clearDepth;
    clear.DepthStencil.Stencil = clearStencil;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
        format,
        static_cast<UINT64>(width),
        static_cast<UINT>(height),
        1,
        1,
        sampleCount,
        0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
    );

    ComPtr<ID3D12Resource> tex;
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear,
        IID_PPV_ARGS(&tex)
    );

    if (FAILED(hr))
        return nullptr;

    setDebugName(tex.Get(), debugName);
    return tex;
}
