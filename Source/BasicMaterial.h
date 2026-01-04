#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

namespace tinygltf { class Model; struct Material; }

using Microsoft::WRL::ComPtr;
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

struct PBRPhongMaterialData
{
    XMFLOAT3 diffuseColour = XMFLOAT3(1, 1, 1);
    BOOL     hasDiffuseTex = FALSE;

    XMFLOAT3 specularColour = XMFLOAT3(0.015f, 0.015f, 0.015f);
    float    shininess = 64.0f;
};

struct MetallicRoughnessMaterialData
{
    XMFLOAT4 baseColour = XMFLOAT4(1, 1, 1, 1);
    float    metallicFactor = 1.0f;
    float    roughnessFactor = 1.0f;
    float    occlusionStrength = 1.0f;
    float    normalScale = 1.0f;
    XMFLOAT3 emissiveFactor = XMFLOAT3(0, 0, 0);

    BOOL     hasBaseColourTex = FALSE;
    BOOL     hasMetallicRoughnessTex = FALSE;
    BOOL     hasOcclusionTex = FALSE;
    BOOL     hasNormalMap = FALSE;
    BOOL     hasEmissive = FALSE;
};

class BasicMaterial
{
public:
    enum Type
    {
        BASIC = 0,
        PHONG,
        PBR_PHONG,
        METALLIC_ROUGHNESS
    };

    enum TextureSlot
    {
        SLOT_BASECOLOUR = 0,
        SLOT_METALLIC_ROUGHNESS = 1,
        SLOT_OCCLUSION = 2,
        SLOT_EMISSIVE = 3,
        SLOT_NORMAL = 4,
        SLOT_COUNT = 5
    };

public:
    BasicMaterial() = default;
    ~BasicMaterial() = default;

    void load(const tinygltf::Model& model, const tinygltf::Material& material, Type materialType, const char* basePath);

    Type getMaterialType() const { return materialType; }

    const BasicMaterialData& getBasicMaterial() const { _ASSERTE(materialType == BASIC); return materialData.basic; }
    const PhongMaterialData& getPhongMaterial() const { _ASSERTE(materialType == PHONG); return materialData.phong; }
    const PBRPhongMaterialData& getPBRPhongMaterial() const { _ASSERTE(materialType == PBR_PHONG); return materialData.pbrPhong; }
    const MetallicRoughnessMaterialData& getMetallicRoughnessMaterial() const { _ASSERTE(materialType == METALLIC_ROUGHNESS); return materialData.metallicRoughness; }

    const char* getName() const { return name.c_str(); }

    D3D12_GPU_DESCRIPTOR_HANDLE getTextureHandle(TextureSlot slot) const { return textureGpu[slot]; }
    D3D12_GPU_DESCRIPTOR_HANDLE getBaseColourSRV() const { return textureGpu[SLOT_BASECOLOUR]; }

private:
    static std::wstring toWStringUTF8(const std::string& s);
    static std::wstring makeTexturePathW(const char* basePath, const std::string& uri);

    bool loadTextureSRV(const tinygltf::Model& model, int textureIndex, const char* basePath, TextureSlot slot);

private:
    union
    {
        BasicMaterialData             basic;
        PhongMaterialData             phong;
        PBRPhongMaterialData          pbrPhong;
        MetallicRoughnessMaterialData metallicRoughness;
    } materialData = {};

    Type materialType = BASIC;

    std::array<ComPtr<ID3D12Resource>, SLOT_COUNT> textures;
    std::array<uint32_t, SLOT_COUNT> srvIndex = {};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, SLOT_COUNT> textureGpu = {};

    std::string name;
};
