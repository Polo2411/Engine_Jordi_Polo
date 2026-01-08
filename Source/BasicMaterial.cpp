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

    // Allocate a contiguous block for t0
    tableStartIndex = descriptors->allocateRange(SLOT_COUNT);
    _ASSERTE(tableStartIndex != UINT32_MAX);

    texturesTableGpu = descriptors->getGPUHandle(tableStartIndex);

    // Fill with null SRV by default
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

    const bool hasBaseTex = loadTextureIntoSlot(model, pbr.baseColorTexture.index, basePath, SLOT_BASECOLOUR);

    if (materialType == BASIC)
    {
        materialData.basic.baseColour = XMFLOAT4(baseColour.x, baseColour.y, baseColour.z, baseColour.w);
        materialData.basic.hasColourTexture = hasBaseTex ? TRUE : FALSE;
    }
    else // PHONG
    {
        materialData.phong.diffuseColour = XMFLOAT4(baseColour.x, baseColour.y, baseColour.z, baseColour.w);
        materialData.phong.Kd = 0.85f;
        materialData.phong.Ks = 0.35f;
        materialData.phong.shininess = 32.0f;
        materialData.phong.hasDiffuseTex = hasBaseTex ? TRUE : FALSE;
    }
}

void BasicMaterial::setPhongMaterial(const PhongMaterialData& phong)
{
    if (materialType != PHONG)
        return;

    PhongMaterialData out = phong;

    out.Kd = std::clamp(out.Kd, 0.0f, 1.0f);
    out.Ks = std::clamp(out.Ks, 0.0f, 1.0f);
    out.shininess = (out.shininess < 1.0f) ? 1.0f : out.shininess;

    // If no texture was loaded, UI cannot enable it.
    if (!hasTexture(SLOT_BASECOLOUR))
        out.hasDiffuseTex = FALSE;

    materialData.phong = out;
}
