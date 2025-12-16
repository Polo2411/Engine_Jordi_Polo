#include "Globals.h"
#include "D3D12Module.h"
#include "d3dx12.h"
#include "ImGuiPass.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplers.h"

#include <algorithm>

// -------------------- ctor / dtor --------------------

D3D12Module::D3D12Module(HWND wnd) : hWnd(wnd)
{
}

D3D12Module::~D3D12Module()
{
    flush();
}

// -------------------- init / cleanUp --------------------

bool D3D12Module::init()
{
#if defined(_DEBUG)
    enableDebugLayer();
#endif

    getWindowSize(windowWidth, windowHeight);

    bool ok = createFactory();
    ok = ok && createDevice(false);

#if defined(_DEBUG)
    ok = ok && setupInfoQueue();
#endif

    ok = ok && createDrawCommandQueue();
    ok = ok && createSwapChain();
    ok = ok && createRenderTargets();
    ok = ok && createDepthStencil();
    ok = ok && createCommandList();
    ok = ok && createDrawFence();

    if (ok)
    {
        currentBackBufferIdx = swapChain->GetCurrentBackBufferIndex();

        // ImGuiPass (tu engine lo hace aquí)
        imgui = std::make_unique<ImGuiPass>(device.Get(), hWnd);

        // inicializa tracking
        frameIndex = 0;
        lastCompletedFrame = 0;
        for (UINT i = 0; i < kBufferCount; ++i)
        {
            drawFenceValues[i] = 0;
            frameValues[i] = 0;
        }
    }

    return ok;
}

bool D3D12Module::cleanUp()
{
    imgui.reset();

    if (drawEvent)
        CloseHandle(drawEvent);
    drawEvent = nullptr;

    return true;
}

// -------------------- frame: preRender / postRender --------------------

void D3D12Module::preRender()
{
    currentBackBufferIdx = swapChain->GetCurrentBackBufferIndex();

    // Si este backbuffer aún está en uso por la GPU (tiene fence pendiente), esperamos.
    if (drawFenceValues[currentBackBufferIdx] != 0)
    {
        drawFence->SetEventOnCompletion(drawFenceValues[currentBackBufferIdx], drawEvent);
        WaitForSingleObject(drawEvent, INFINITE);

        // Ya sabemos seguro que el frame que se renderizó en este backbuffer se ha completado.
        lastCompletedFrame = std::max(lastCompletedFrame, frameValues[currentBackBufferIdx]);
    }

    // Nuevo frame (CPU)
    ++frameIndex;
    frameValues[currentBackBufferIdx] = frameIndex;

    commandAllocators[currentBackBufferIdx]->Reset();

    // Arrancar frame ImGui
    if (imgui)
        imgui->startFrame();
}

void D3D12Module::postRender()
{
    swapChain->Present(1, 0); // vsync ON
    signalDrawQueue();
}

UINT D3D12Module::signalDrawQueue()
{
    drawFenceValues[currentBackBufferIdx] = ++drawFenceCounter;
    drawCommandQueue->Signal(drawFence.Get(), drawFenceValues[currentBackBufferIdx]);
    return drawFenceCounter;
}

// -------------------- sync total (para cerrar) --------------------

void D3D12Module::flush()
{
    if (!drawCommandQueue || !drawFence) return;

    drawCommandQueue->Signal(drawFence.Get(), ++drawFenceCounter);
    drawFence->SetEventOnCompletion(drawFenceCounter, drawEvent);
    WaitForSingleObject(drawEvent, INFINITE);

    // Al hacer flush, podemos considerar que todo lo anterior está completado.
    lastCompletedFrame = frameIndex;
}

void D3D12Module::resize()
{
    unsigned width, height;
    getWindowSize(width, height);

    if (width == 0 || height == 0 ||
        (width == windowWidth && height == windowHeight))
        return;

    windowWidth = width;
    windowHeight = height;

    flush();

    for (UINT i = 0; i < kBufferCount; ++i)
    {
        backBuffers[i].Reset();
        drawFenceValues[i] = 0;
        frameValues[i] = 0;
    }

    rtDescriptorHeap.Reset();
    depthStencilBuffer.Reset();
    dsDescriptorHeap.Reset();

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    bool ok = SUCCEEDED(swapChain->GetDesc(&swapDesc));
    if (ok)
    {
        ok = SUCCEEDED(swapChain->ResizeBuffers(
            kBufferCount,
            windowWidth,
            windowHeight,
            swapDesc.BufferDesc.Format,
            swapDesc.Flags
        ));
    }

    if (ok)
    {
        ok = createRenderTargets();
        ok = ok && createDepthStencil();
    }

    if (ok)
        currentBackBufferIdx = swapChain->GetCurrentBackBufferIndex();
}

// -------------------- helpers creación --------------------

void D3D12Module::enableDebugLayer()
{
    ComPtr<ID3D12Debug> debugInterface;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
        debugInterface->EnableDebugLayer();
}

bool D3D12Module::createFactory()
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    return SUCCEEDED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)));
}

bool D3D12Module::createDevice(bool useWarp)
{
    bool ok = true;

    if (useWarp)
    {
        ComPtr<IDXGIAdapter1> adapter;
        ok = ok && SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
        ok = ok && SUCCEEDED(D3D12CreateDevice(
            adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    }
    else
    {
        ComPtr<IDXGIAdapter4> adapter;
        factory->EnumAdapterByGpuPreference(
            0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
        ok = SUCCEEDED(D3D12CreateDevice(
            adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    }

    return ok;
}

bool D3D12Module::setupInfoQueue()
{
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    bool ok = SUCCEEDED(device.As(&pInfoQueue));
    if (ok)
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    }
    return ok;
}

bool D3D12Module::createDrawCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    return SUCCEEDED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&drawCommandQueue)));
}

bool D3D12Module::createSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = windowWidth;
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    bool ok = SUCCEEDED(factory->CreateSwapChainForHwnd(
        drawCommandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

    ok = ok && SUCCEEDED(swapChain1.As(&swapChain));
    ok = ok && SUCCEEDED(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    return ok;
}

bool D3D12Module::createRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = kBufferCount;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    bool ok = SUCCEEDED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtDescriptorHeap)));

    if (ok)
    {
        UINT rtvDescriptorSize =
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (UINT i = 0; ok && i < kBufferCount; ++i)
        {
            ok = SUCCEEDED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])));

            if (ok)
            {
                backBuffers[i]->SetName(L"BackBuffer");
                device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle);
            }

            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    return ok;
}

bool D3D12Module::createDepthStencil()
{
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC desc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            windowWidth,
            windowHeight,
            1,
            0,
            1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
        );

    bool ok = SUCCEEDED(
        device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&depthStencilBuffer)
        )
    );

    if (ok)
    {
        depthStencilBuffer->SetName(L"Depth/Stencil Texture");

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ok = SUCCEEDED(
            device->CreateDescriptorHeap(
                &dsvHeapDesc,
                IID_PPV_ARGS(&dsDescriptorHeap)
            )
        );

        if (ok)
            dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");
    }

    if (ok)
    {
        device->CreateDepthStencilView(
            depthStencilBuffer.Get(),
            nullptr,
            dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
        );
    }

    return ok;
}

bool D3D12Module::createCommandList()
{
    bool ok = true;

    for (UINT i = 0; ok && i < kBufferCount; ++i)
        ok = SUCCEEDED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i])));

    ok = ok && SUCCEEDED(device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList)));

    ok = ok && SUCCEEDED(commandList->Close());
    return ok;
}

bool D3D12Module::createDrawFence()
{
    bool ok = SUCCEEDED(device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&drawFence)));

    if (ok)
    {
        drawEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        ok = drawEvent != nullptr;
    }
    return ok;
}

void D3D12Module::getWindowSize(unsigned& width, unsigned& height)
{
    RECT clientRect = {};
    GetClientRect(hWnd, &clientRect);

    width = unsigned(clientRect.right - clientRect.left);
    height = unsigned(clientRect.bottom - clientRect.top);
}

void D3D12Module::bindShaderVisibleHeaps(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList)
        return;

    ID3D12DescriptorHeap* heaps[2] = {};
    UINT count = 0;

    if (app && app->getShaderDescriptors() && app->getShaderDescriptors()->getHeap())
        heaps[count++] = app->getShaderDescriptors()->getHeap();

    if (app && app->getSamplers() && app->getSamplers()->getHeap())
        heaps[count++] = app->getSamplers()->getHeap();

    if (count > 0)
        cmdList->SetDescriptorHeaps(count, heaps);
}


// -------------------- getters --------------------

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Module::getRenderTargetDescriptor()
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        rtDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        currentBackBufferIdx,
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Module::getDepthStencilDescriptor()
{
    return dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}
