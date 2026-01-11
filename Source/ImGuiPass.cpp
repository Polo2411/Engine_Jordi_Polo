#include "Globals.h"
#include "ImGuiPass.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ModuleShaderDescriptors.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

ImGuiPass::ImGuiPass(ID3D12Device* device, HWND hwnd)
{
    // 1) Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // 2) Fonts
    io.Fonts->AddFontDefault();
    io.Fonts->Build();

    // 3) Platform backend
    ImGui_ImplWin32_Init(hwnd);

    // 4) Use the engine's global CBV/SRV/UAV shader-visible heap
    ModuleShaderDescriptors* shaderDescs = (app) ? app->getShaderDescriptors() : nullptr;
    ID3D12DescriptorHeap* engineHeap = (shaderDescs) ? shaderDescs->getHeap() : nullptr;

    if (!engineHeap)
    {
        OutputDebugStringA("ImGuiPass: Engine shader descriptor heap is null.\n");
        // Hard fail is better than silent mismatch later.
        // If you prefer to keep running, you'd need a copy-to-imgui-heap path.
        _ASSERTE(engineHeap && "ImGuiPass requires engine shader-visible heap (ModuleShaderDescriptors).");
        return;
    }

    // Hold a ref to the engine heap so it's alive while ImGui is alive.
    srvHeap = engineHeap;

    // Reserve a descriptor for ImGui font SRV inside the engine heap.
    // This avoids having a separate ImGui heap (which breaks ImGui::Image with engine SRVs).
    fontTable = shaderDescs->allocTable();

    // IMPORTANT: Pass the *exact descriptor location* ImGui will use for its font texture.
    // ImGui backend will create the SRV into cpuHandle and use gpuHandle for rendering.
    const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = fontTable.getCPUHandle(0);
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = fontTable.getGPUHandle(0);

    // 5) Renderer backend (DX12)
    ImGui_ImplDX12_Init(
        device,
        FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        srvHeap.Get(),
        cpuHandle,
        gpuHandle
    );
}

ImGuiPass::~ImGuiPass()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Release the reserved descriptor table for the font SRV.
    fontTable.reset();
    srvHeap.Reset();
}

void ImGuiPass::startFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiPass::record(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList)
        return;

    // Bind engine heaps (CBV/SRV/UAV + Samplers) before rendering ImGui.
    // The DX12 backend will also bind srvHeap internally, but this keeps the command list consistent.
    if (app)
    {
        if (D3D12Module* d3d12 = app->getD3D12Module())
            d3d12->bindShaderVisibleHeaps(cmdList);
        else
        {
            ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
            cmdList->SetDescriptorHeaps(1, heaps);
        }
    }

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}
