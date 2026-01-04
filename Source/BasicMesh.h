#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

namespace tinygltf { class Model; struct Primitive; }

class BasicMesh
{
public:
    struct Vertex
    {
        Vector3 position = Vector3::Zero;
        Vector2 texCoord0 = Vector2::Zero;
        Vector3 normal = Vector3::UnitZ;
        Vector3 tangent = Vector3::UnitX;
    };

public:
    BasicMesh() = default;
    ~BasicMesh() = default;

    void load(const tinygltf::Model& model, const tinygltf::Primitive& primitive, const char* debugName = nullptr);

    const std::string& getName() const { return name; }

    uint32_t getNumVertices() const { return numVertices; }
    uint32_t getNumIndices() const { return numIndices; }
    bool hasIndices() const { return indexBuffer != nullptr; }

    int32_t getMaterialIndex() const { return materialIndex; }

    void draw(ID3D12GraphicsCommandList* commandList) const;

    static const D3D12_INPUT_LAYOUT_DESC& getInputLayoutDesc() { return inputLayoutDesc; }

private:
    BasicMesh(const BasicMesh&) = delete;
    BasicMesh& operator=(const BasicMesh&) = delete;

private:
    std::string name;

    uint32_t numVertices = 0;
    uint32_t numIndices = 0;
    uint32_t indexElementSize = 0;
    int32_t  materialIndex = -1;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};

    static const uint32_t numVertexAttribs = 4;
    static const D3D12_INPUT_ELEMENT_DESC inputLayout[numVertexAttribs];
    static const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
};
