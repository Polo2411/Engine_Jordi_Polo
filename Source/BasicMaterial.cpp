// BasicMaterial.cpp
#include "Globals.h"
#include "BasicMaterial.h"

#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#include "tiny_gltf.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    bool TryGetBaseColorTextureURI(const tinygltf::Model& model, const tinygltf::Material& mat, std::string& outUri)
    {
        const int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIndex < 0 || texIndex >= (int)model.textures.size())
            return false;

        const tinygltf::Texture& tex = model.textures[(size_t)texIndex];
        if (tex.source < 0 || tex.source >= (int)model.images.size())
            return false;

        const tinygltf::Image& img = model.images[(size_t)tex.source];
        if (img.uri.empty())
            return false;

        outUri = img.uri;
        return true;
    }

    Vector4 GetBaseColorFactor(const tinygltf::Material& mat)
    {
        const auto& f = mat.pbrMetallicRoughness.baseColorFactor;
        if (f.size() == 4)
            return Vector4((float)f[0], (float)f[1], (float)f[2], (float)f[3]);
        return Vector4(1, 1, 1, 1);
    }
}

BasicMaterial::~BasicMaterial()
{
    releaseResources();
}

void BasicMaterial::releaseResources()
{
    // Release GPU resources safely via ModuleResources deferRelease
    if (app)
    {
        if (ModuleResources* res = app->getResources())
        {
            for (uint32_t i = 0; i < SLOT_COUNT; ++i)
            {
                if (textures[i])
                    res->deferRelease(textures[i]);

                textures[i].Reset();
            }
        }
        else
        {
            for (uint32_t i = 0; i < SLOT_COUNT; ++i)
                textures[i].Reset();
        }
    }
    else
    {
        for (uint32_t i = 0; i < SLOT_COUNT; ++i)
            textures[i].Reset();
    }

    texturesTable.reset();
}

void BasicMaterial::load(const tinygltf::Model& model,
    const tinygltf::Material& material,
    Type type,
    const char* basePath)
{
    releaseResources();

    materialType = type;
    name = material.name.empty() ? "gltf_material" : material.name;

    // Fill defaults from glTF baseColorFactor
    phong = {};
    phong.diffuseColour = GetBaseColorFactor(material);
    phong.specularColour = Vector3(0.04f, 0.04f, 0.04f);
    phong.shininess = 64.0f;

    // Try load baseColor texture
    std::string uri;
    if (app && TryGetBaseColorTextureURI(model, material, uri))
    {
        ModuleResources* res = app->getResources();

        fs::path fullPath;
        if (basePath && basePath[0] != '\0')
            fullPath = fs::path(basePath) / fs::path(uri);
        else
            fullPath = fs::path(uri);

        if (res)
        {
            const std::wstring w = fullPath.wstring();
            textures[0] = res->createTextureFromFile(w.c_str());
        }
    }

    // ✅ Default behavior: if texture exists, enable it by default.
    // UI can still disable it later; enforceTextureFlags() only forces OFF when missing.
    phong.hasDiffuseTex = textures[0] ? 1u : 0u;

    enforceTextureFlags();
    rebuildDescriptorTable();
}

void BasicMaterial::setPhongMaterial(const PhongMaterialData& p)
{
    phong = p;
    enforceTextureFlags();
}

void BasicMaterial::enforceTextureFlags()
{
    // Keep UI flags consistent with actual resources
    if (!textures[0])
        phong.hasDiffuseTex = 0u;
}

void BasicMaterial::rebuildDescriptorTable()
{
    if (!app)
        return;

    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    if (!descs)
        return;

    // Always allocate a table so the draw code can bind it unconditionally
    texturesTable = descs->allocTable();

    // Slot 0 only
    if (textures[0])
    {
        texturesTable.createTextureSRV(textures[0].Get(), 0);
    }
    else
    {
        // Robust: bind a valid null SRV
        texturesTable.createNullTexture2DSRV(0);
    }
}
