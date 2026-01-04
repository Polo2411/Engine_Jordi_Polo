#pragma once

#include <string>
#include <array>
#include <wrl/client.h>
#include <d3d12.h>

namespace tinygltf { class Model; struct Material; }

struct BasicMaterialData
{
    XMFLOAT4 baseColour;
    BOOL     hasColourTexture;
    UINT     padding[3] = { 0, 0, 0 };
};

struct PhongMaterialData
{
    XMFLOAT4 diffuseColour;
    float    Kd;
    float    Ks;
    float    shininess;
    BOOL     hasDiffuseTex;
    UINT     padding[3] = { 0, 0, 0 };
};

struct PBRPhongMaterialData
{
    XMFLOAT3 diffuseColour;
    BOOL     hasDiffuseTex;

    XMFLOAT3 specularColour;
    float    shininess;
};

struct MetallicRoughnessMaterialData
{
    XMFLOAT4 baseColour;
    float    metallicFactor;
    float    roughnessFactor;
    float    occlusionStrength;
    float    normalScale;
    XMFLOAT3 emissiveFactor;

    BOOL     hasBaseColourTex;
    BOOL     hasMetallicRoughnessTex;
    BOOL     hasOcclusionTex;
    BOOL     hasNormalMap;
    BOOL     hasEmissive;
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

    // Slot-based SRV access (Exercise 5 uses SLOT_BASECOLOUR only)
    D3D12_GPU_DESCRIPTOR_HANDLE getTextureHandle(TextureSlot slot) const { return textureGpu[slot]; }

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

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, SLOT_COUNT> textures;
    std::array<uint32_t, SLOT_COUNT> srvIndex = {};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, SLOT_COUNT> textureGpu = {};

    std::string name;
};
