#include "Globals.h"
#include "ImGuiPass.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

ImGuiPass::ImGuiPass(
    ID3D12Device* inDevice,
    HWND inHwnd,
    ID3D12DescriptorHeap* inSrvHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle,
    DXGI_FORMAT rtvFormat,
    uint32_t framesInFlight)
    : device(inDevice)
    , hwnd(inHwnd)
    , srvHeap(inSrvHeap)
    , format(rtvFormat)
    , frames(framesInFlight)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);

    if (device && srvHeap && fontCpuHandle.ptr != 0 && fontGpuHandle.ptr != 0)
    {
        ImGui_ImplDX12_Init(
            device,
            (int)frames,
            format,
            srvHeap,
            fontCpuHandle,
            fontGpuHandle);

        initialized = true;
    }

    io.Fonts->AddFontDefault();
    io.Fonts->Build();
}

ImGuiPass::~ImGuiPass()
{
    if (initialized)
        ImGui_ImplDX12_Shutdown();

    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    initialized = false;
    device = nullptr;
    hwnd = nullptr;
    srvHeap = nullptr;
}

void ImGuiPass::startFrame()
{
    ImGui_ImplWin32_NewFrame();

    if (initialized)
        ImGui_ImplDX12_NewFrame();

    ImGui::NewFrame();
}

void ImGuiPass::record(ID3D12GraphicsCommandList* cmdList)
{
    // If a frame started, it must end.
    if (!cmdList)
    {
        ImGui::EndFrame();
        return;
    }

    if (!initialized || !srvHeap)
    {
        ImGui::EndFrame();
        return;
    }

    ImGui::Render();

    ID3D12DescriptorHeap* heaps[] = { srvHeap };
    cmdList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}
