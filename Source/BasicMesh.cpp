#include "Globals.h"
#include "BasicMesh.h"

#include "Application.h"
#include "ModuleResources.h"
#include "gltf_utils.h"

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267)
#include "tiny_gltf.h"
#pragma warning(pop)

const D3D12_INPUT_ELEMENT_DESC BasicMesh::inputLayout[numVertexAttribs] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord0), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

const D3D12_INPUT_LAYOUT_DESC BasicMesh::inputLayoutDesc = { &inputLayout[0], UINT(std::size(inputLayout)) };

namespace
{
    DXGI_FORMAT indexFormatFromSize(uint32_t elemSize)
    {
        switch (elemSize)
        {
        case 1: return DXGI_FORMAT_R8_UINT;
        case 2: return DXGI_FORMAT_R16_UINT;
        case 4: return DXGI_FORMAT_R32_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
        }
    }
}

void BasicMesh::load(const tinygltf::Model& model, const tinygltf::Primitive& primitive, const char* debugName)
{
    name = (debugName && debugName[0] != '\0') ? debugName : "gltf_primitive";
    materialIndex = primitive.material;

    const auto itPos = primitive.attributes.find("POSITION");
    if (itPos == primitive.attributes.end())
    {
        LOG("BasicMesh::load: missing POSITION");
        return;
    }

    ModuleResources* resources = app->getResources();

    const tinygltf::Accessor& posAcc = model.accessors[itPos->second];
    numVertices = uint32_t(posAcc.count);

    std::unique_ptr<Vertex[]> vertices = std::make_unique<Vertex[]>(numVertices);
    uint8_t* vertexData = reinterpret_cast<uint8_t*>(vertices.get());

    loadAccessorData(vertexData + offsetof(Vertex, position), sizeof(Vector3), sizeof(Vertex), numVertices, model, itPos->second);
    loadAccessorData(vertexData + offsetof(Vertex, texCoord0), sizeof(Vector2), sizeof(Vertex), numVertices, model, primitive.attributes, "TEXCOORD_0");
    loadAccessorData(vertexData + offsetof(Vertex, normal), sizeof(Vector3), sizeof(Vertex), numVertices, model, primitive.attributes, "NORMAL");

    // Tangent can be vec3 or vec4 in glTF
    bool hasTangent = loadAccessorData(vertexData + offsetof(Vertex, tangent), sizeof(Vector3), sizeof(Vertex), numVertices, model, primitive.attributes, "TANGENT");
    if (!hasTangent)
    {
        std::vector<Vector4> tangents(numVertices);
        if (loadAccessorData(reinterpret_cast<uint8_t*>(tangents.data()), sizeof(Vector4), sizeof(Vector4), numVertices, model, primitive.attributes, "TANGENT"))
        {
            for (uint32_t i = 0; i < numVertices; ++i)
            {
                vertices[i].tangent.x = tangents[i].x;
                vertices[i].tangent.y = tangents[i].y;
                vertices[i].tangent.z = tangents[i].z * tangents[i].w;
            }
        }
    }

    vertexBuffer = resources->createDefaultBuffer(vertices.get(), size_t(numVertices) * sizeof(Vertex), name.c_str());
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = UINT(size_t(numVertices) * sizeof(Vertex));

    indexBuffer.Reset();
    indexBufferView = {};
    numIndices = 0;
    indexElementSize = 0;

    if (primitive.indices >= 0)
    {
        const tinygltf::Accessor& indAcc = model.accessors[primitive.indices];

        const int ct = indAcc.componentType;
        const bool supported =
            (ct == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) ||
            (ct == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) ||
            (ct == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE);

        if (supported)
        {
            indexElementSize = uint32_t(tinygltf::GetComponentSizeInBytes(indAcc.componentType));
            numIndices = uint32_t(indAcc.count);

            std::unique_ptr<uint8_t[]> indices = std::make_unique<uint8_t[]>(size_t(numIndices) * indexElementSize);
            loadAccessorData(indices.get(), indexElementSize, indexElementSize, numIndices, model, primitive.indices);

            indexBuffer = resources->createDefaultBuffer(indices.get(), size_t(numIndices) * indexElementSize, name.c_str());

            indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
            indexBufferView.Format = indexFormatFromSize(indexElementSize);
            indexBufferView.SizeInBytes = UINT(size_t(numIndices) * indexElementSize);
        }
        else
        {
            LOG("BasicMesh::load: unsupported index format");
        }
    }
}

void BasicMesh::draw(ID3D12GraphicsCommandList* commandList) const
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    if (indexBuffer)
    {
        commandList->IASetIndexBuffer(&indexBufferView);
        commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
    }
    else
    {
        commandList->DrawInstanced(numVertices, 1, 0, 0);
    }
}
