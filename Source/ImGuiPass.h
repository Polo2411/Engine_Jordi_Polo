#pragma once
#include "Globals.h"

#include "ShaderTableDesc.h"

#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

class ImGuiPass
{
public:
    ImGuiPass(ID3D12Device* device, HWND hwnd);
    ~ImGuiPass();

    void startFrame();
    void record(ID3D12GraphicsCommandList* cmdList);

private:
    // Shared engine heap (CBV/SRV/UAV). Must match handles used by materials/RenderTexture SRVs.
    ComPtr<ID3D12DescriptorHeap> srvHeap;

    // A table reserved for ImGui font SRV inside ModuleShaderDescriptors heap.
    ShaderTableDesc fontTable;
};
