#include "Globals.h"
#include "Exercise1Module.h"

#include "Application.h"
#include "D3D12Module.h"
#include "ImGuiPass.h"

#include <d3d12.h>
#include "d3dx12.h"

void Exercise1Module::render()
{
    D3D12Module* d3d12 = app->getD3D12Module();
    if (!d3d12) return;

    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    // Reset command list using the allocator for the current backbuffer
    commandList->Reset(d3d12->getCommandAllocator(), nullptr);

    // Transition backbuffer: PRESENT -> RENDER_TARGET
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            d3d12->getBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    // Clear render target
    float clearColor[] = { 0.0f, 0.0f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(
        d3d12->getRenderTargetDescriptor(), clearColor, 0, nullptr);

    // Bind render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // Record ImGui draw commands into this command list
    if (ImGuiPass* imgui = d3d12->getImGuiPass())
    {
        imgui->record(commandList);
    }

    // Transition backbuffer: RENDER_TARGET -> PRESENT
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);

    // Close and execute command list
    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(
        UINT(std::size(commandLists)), commandLists);
}
