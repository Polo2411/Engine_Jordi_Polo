// BasicMaterial.cpp
#include "Globals.h"
#include "BasicMaterial.h"

#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267)
#include "tiny_gltf.h"
#pragma warning(pop)

std::wstring BasicMaterial::toWStringUTF8(const std::string& s)
{
    if (s.empty())
        return {};

    const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (sizeNeeded <= 0)
        return {};

    std::wstring w;
    w.resize(sizeNeeded);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), sizeNeeded);
    return w;
}

std::wstring BasicMaterial::makeTexturePathW(const char* basePath, const std::string& uri)
{
    std::string base = basePath ? basePath : "";
    return toWStringUTF8(base + uri);
}

void BasicMaterial::initNullTable()
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

    // Allocate a contiguous block for t0..t4
    tableStartIndex = descriptors->allocateRange(SLOT_COUNT);
    _ASSERTE(tableStartIndex != UINT32_MAX);

    texturesTableGpu = descriptors->getGPUHandle(tableStartIndex);

    // Fill with null SRVs by default
    for (uint32_t i = 0; i < SLOT_COUNT; ++i)
    {
        descriptors->writeNullTexture2DSRV(tableStartIndex + i);
        textures[i].Reset();
    }
}

bool BasicMaterial::loadTextureIntoSlot(const tinygltf::Model& model, int textureIndex, const char* basePath, TextureSlot slot)
{
    if (textureIndex < 0 || textureIndex >= (int)model.textures.size())
        return false;

    const tinygltf::Texture& tex = model.textures[textureIndex];
    const int src = tex.source;
    if (src < 0 || src >= (int)model.images.size())
        return false;

    const tinygltf::Image& img = model.images[src];
    if (img.uri.empty())
        return false;

    ModuleResources* resources = app->getResources();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

    const std::wstring pathW = makeTexturePathW(basePath, img.uri);
    textures[(size_t)slot] = resources->createTextureFromFile(pathW, nullptr);
    if (!textures[(size_t)slot])
        return false;

    descriptors->writeSRV(tableStartIndex + (uint32_t)slot, textures[(size_t)slot].Get());
    return true;
}

void BasicMaterial::load(const tinygltf::Model& model, const tinygltf::Material& material, Type type, const char* basePath)
{
    name = material.name;
    materialType = type;

    materialData = {};
    initNullTable();

    Vector4 baseColour = Vector4::One;
    const auto& pbr = material.pbrMetallicRoughness;

    if (pbr.baseColorFactor.size() == 4)
    {
        baseColour = Vector4(
            float(pbr.baseColorFactor[0]),
            float(pbr.baseColorFactor[1]),
            float(pbr.baseColorFactor[2]),
            float(pbr.baseColorFactor[3]));
    }

    const bool hasColourTex = loadTextureIntoSlot(model, pbr.baseColorTexture.index, basePath, SLOT_BASECOLOUR);

    if (materialType == BASIC)
    {
        materialData.basic.baseColour = XMFLOAT4(baseColour.x, baseColour.y, baseColour.z, baseColour.w);
        materialData.basic.hasColourTexture = hasColourTex ? TRUE : FALSE;
    }
    else if (materialType == PHONG)
    {
        materialData.phong.diffuseColour = XMFLOAT4(baseColour.x, baseColour.y, baseColour.z, baseColour.w);
        materialData.phong.Kd = 0.85f;
        materialData.phong.Ks = 0.35f;
        materialData.phong.shininess = 32.0f;
        materialData.phong.hasDiffuseTex = hasColourTex ? TRUE : FALSE;
    }
    else if (materialType == PBR_PHONG)
    {
        materialData.pbrPhong.diffuseColour = XMFLOAT3(baseColour.x, baseColour.y, baseColour.z);
        materialData.pbrPhong.hasDiffuseTex = hasColourTex ? TRUE : FALSE;
        materialData.pbrPhong.specularColour = XMFLOAT3(0.015f, 0.015f, 0.015f);
        materialData.pbrPhong.shininess = 64.0f;
    }
    else if (materialType == METALLIC_ROUGHNESS)
    {
        const bool hasMR = loadTextureIntoSlot(model, pbr.metallicRoughnessTexture.index, basePath, SLOT_METALLIC_ROUGHNESS);
        const bool hasOcc = loadTextureIntoSlot(model, material.occlusionTexture.index, basePath, SLOT_OCCLUSION);
        const bool hasEmi = loadTextureIntoSlot(model, material.emissiveTexture.index, basePath, SLOT_EMISSIVE);
        const bool hasNrm = loadTextureIntoSlot(model, material.normalTexture.index, basePath, SLOT_NORMAL);

        Vector3 emissiveFactor = Vector3::Zero;
        if (material.emissiveFactor.size() >= 3)
        {
            emissiveFactor = Vector3(
                float(material.emissiveFactor[0]),
                float(material.emissiveFactor[1]),
                float(material.emissiveFactor[2]));
        }

        materialData.metallicRoughness.baseColour = XMFLOAT4(baseColour.x, baseColour.y, baseColour.z, baseColour.w);
        materialData.metallicRoughness.metallicFactor = float(pbr.metallicFactor);
        materialData.metallicRoughness.roughnessFactor = float(pbr.roughnessFactor);
        materialData.metallicRoughness.occlusionStrength = float(material.occlusionTexture.strength);
        materialData.metallicRoughness.normalScale = float(material.normalTexture.scale);
        materialData.metallicRoughness.emissiveFactor = XMFLOAT3(emissiveFactor.x, emissiveFactor.y, emissiveFactor.z);

        materialData.metallicRoughness.hasBaseColourTex = hasColourTex ? TRUE : FALSE;
        materialData.metallicRoughness.hasMetallicRoughnessTex = hasMR ? TRUE : FALSE;
        materialData.metallicRoughness.hasOcclusionTex = hasOcc ? TRUE : FALSE;
        materialData.metallicRoughness.hasEmissive = hasEmi ? TRUE : FALSE;
        materialData.metallicRoughness.hasNormalMap = hasNrm ? TRUE : FALSE;
    }
}

void BasicMaterial::setPhongMaterial(const PhongMaterialData& phong)
{
    if (materialType != PHONG)
        return;

    PhongMaterialData out = phong;

    // Basic sanity clamps
    out.Kd = std::clamp(out.Kd, 0.0f, 1.0f);
    out.Ks = std::clamp(out.Ks, 0.0f, 1.0f);
    out.shininess = (out.shininess < 1.0f) ? 1.0f : out.shininess;

    // If we don't have a diffuse texture, we cannot enable it from UI.
    if (!hasTexture(SLOT_BASECOLOUR))
        out.hasDiffuseTex = FALSE;

    materialData.phong = out;
}

void BasicMaterial::setPBRPhongMaterial(const PBRPhongMaterialData& pbr)
{
    if (materialType != PBR_PHONG)
        return;

    PBRPhongMaterialData out = pbr;

    out.shininess = (out.shininess < 1.0f) ? 1.0f : out.shininess;

    if (!hasTexture(SLOT_BASECOLOUR))
        out.hasDiffuseTex = FALSE;

    materialData.pbrPhong = out;
}

void BasicMaterial::setMetallicRoughnessMaterial(const MetallicRoughnessMaterialData& mr)
{
    if (materialType != METALLIC_ROUGHNESS)
        return;

    MetallicRoughnessMaterialData out = mr;

    out.metallicFactor = std::clamp(out.metallicFactor, 0.0f, 1.0f);
    out.roughnessFactor = std::clamp(out.roughnessFactor, 0.0f, 1.0f);
    out.occlusionStrength = std::clamp(out.occlusionStrength, 0.0f, 1.0f);
    out.normalScale = (out.normalScale < 0.0f) ? 0.0f : out.normalScale;

    // If the slot is missing, force the corresponding flag off.
    if (!hasTexture(SLOT_BASECOLOUR))          out.hasBaseColourTex = FALSE;
    if (!hasTexture(SLOT_METALLIC_ROUGHNESS))  out.hasMetallicRoughnessTex = FALSE;
    if (!hasTexture(SLOT_OCCLUSION))           out.hasOcclusionTex = FALSE;
    if (!hasTexture(SLOT_EMISSIVE))            out.hasEmissive = FALSE;
    if (!hasTexture(SLOT_NORMAL))              out.hasNormalMap = FALSE;

    materialData.metallicRoughness = out;
}
