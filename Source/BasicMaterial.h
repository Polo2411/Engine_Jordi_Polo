// BasicMaterial.h
#pragma once

#include <string>
#include <array>
#include <algorithm> // std::clamp
#include <cstdint>

#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

namespace tinygltf { class Model; struct Material; }

using namespace DirectX;

struct BasicMaterialData
{
    XMFLOAT4 baseColour = XMFLOAT4(1, 1, 1, 1);
    BOOL     hasColourTexture = FALSE;
    UINT     padding[3] = { 0, 0, 0 };
};

struct PhongMaterialData
{
    XMFLOAT4 diffuseColour = XMFLOAT4(1, 1, 1, 1);
    float    Kd = 0.85f;
    float    Ks = 0.35f;
    float    shininess = 32.0f;
    BOOL     hasDiffuseTex = FALSE;
    UINT     padding[3] = { 0, 0, 0 };
};

class BasicMaterial
{
public:
    enum Type
    {
        BASIC = 0,
        PHONG
    };

    // Only t0 is used in Exercise 6
    enum TextureSlot
    {
        SLOT_BASECOLOUR = 0,
        SLOT_COUNT = 1
    };

public:
    BasicMaterial() = default;
    ~BasicMaterial() = default;

    void load(const tinygltf::Model& model, const tinygltf::Material& material, Type materialType, const char* basePath);

    Type getMaterialType() const { return materialType; }

    const BasicMaterialData& getBasicMaterial() const { _ASSERTE(materialType == BASIC); return materialData.basic; }
    const PhongMaterialData& getPhongMaterial() const { _ASSERTE(materialType == PHONG); return materialData.phong; }

    // UI helper (used by Exercise6Module)
    void setPhongMaterial(const PhongMaterialData& phong);

    const char* getName() const { return name.c_str(); }

    // Descriptor table start handle (t0 contiguous)
    D3D12_GPU_DESCRIPTOR_HANDLE getTexturesTableGPU() const { return texturesTableGpu; }

private:
    static std::wstring toWStringUTF8(const std::string& s);
    static std::wstring makeTexturePathW(const char* basePath, const std::string& uri);

    void initNullTable();
    bool loadTextureIntoSlot(const tinygltf::Model& model, int textureIndex, const char* basePath, TextureSlot slot);

    bool hasTexture(TextureSlot slot) const { return textures[(size_t)slot] != nullptr; }

private:
    union
    {
        BasicMaterialData basic;
        PhongMaterialData phong;
    } materialData = {};

    Type materialType = BASIC;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, SLOT_COUNT> textures;
    std::string name;

    uint32_t tableStartIndex = UINT32_MAX;
    D3D12_GPU_DESCRIPTOR_HANDLE texturesTableGpu{ 0 };
};
