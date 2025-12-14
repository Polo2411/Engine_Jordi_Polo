#pragma once

#include "Module.h"
#include "DebugDrawPass.h"

class Exercise3Module : public Module
{
public:
    bool init() override;
    void render() override;
    bool cleanUp() override { return true; }

private:
    bool createVertexBuffer(void* bufferData, unsigned bufferSize, unsigned stride);
    bool createRootSignature();
    bool createPSO();

private:
    ComPtr<ID3D12Resource>          vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW        vertexBufferView{};
    ComPtr<ID3D12RootSignature>     rootSignature;
    ComPtr<ID3D12PipelineState>     pso;
    std::unique_ptr<DebugDrawPass>  debugDrawPass;

    Matrix                 mvp;
};
