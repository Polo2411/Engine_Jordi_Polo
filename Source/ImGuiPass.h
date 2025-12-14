#pragma once
#include "Globals.h"

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
    ComPtr<ID3D12DescriptorHeap> srvHeap;
};
