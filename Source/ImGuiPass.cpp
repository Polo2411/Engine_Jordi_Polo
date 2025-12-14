#include "Globals.h"
#include "ImGuiPass.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

ImGuiPass::ImGuiPass(ID3D12Device* device, HWND hwnd)
{
    // 1) Contexto de ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // 2) Fuentes -> IMPORTANTE para evitar la aserción del atlas
    //    (puedes cambiarlo luego por una fuente TTF como hace el profe)
    io.Fonts->AddFontDefault();
    io.Fonts->Build();       // <-- ESTO ES LO QUE TE FALTABA

    // 3) Heap de descriptores para la textura de fuentes
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap));
    if (FAILED(hr))
    {
        OutputDebugStringA("ImGuiPass: FAILED to create SRV descriptor heap\n");
    }

    // 4) Backends
    ImGui_ImplWin32_Init(hwnd);

    auto cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_Init(
        device,
        FRAMES_IN_FLIGHT,                   // mismo que kBufferCount
        DXGI_FORMAT_R8G8B8A8_UNORM,         // formato del swapchain
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
    // 🔹 OBLIGATORIO ANTES DE CUALQUIER ImGui::Begin(...)
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiPass::record(ID3D12GraphicsCommandList* cmdList)
{
    // Activar heap de SRVs donde está la textura de fuentes
    ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}
