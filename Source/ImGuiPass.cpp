#include "Globals.h"
#include "ImGuiPass.h"

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

    // 2) Fonts (required to build the font atlas)
    io.Fonts->AddFontDefault();
    io.Fonts->Build();

    // 3) Descriptor heap for the font texture
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap));
    if (FAILED(hr))
    {
        OutputDebugStringA("ImGuiPass: FAILED to create SRV descriptor heap\n");
    }

    // 4) Platform and renderer backends
    ImGui_ImplWin32_Init(hwnd);

    auto cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

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
}

void ImGuiPass::startFrame()
{
    // Must be called before any ImGui::Begin(...)
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiPass::record(ID3D12GraphicsCommandList* cmdList)
{
    // Bind SRV heap containing the font texture
    ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}
