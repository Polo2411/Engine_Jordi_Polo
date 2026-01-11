#include "Globals.h"
#include "BasicMaterial.h"

#include "Application.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleResources.h"

BasicMaterial::~BasicMaterial()
{
    reset();
}

void BasicMaterial::reset()
{
    // Release SRV table (deferred by ShaderTableDesc)
    textureTable.reset();

    // Release texture resources safely if ModuleResources supports deferred release
    if (app)
    {
        if (ModuleResources* res = app->getResources())
        {
            for (auto& t : textures)
            {
                if (t)
                {
                    // IMPORTANT: pass the ComPtr lvalue, not t.Get()
                    res->deferRelease(t);
                    t.Reset();
                }
            }
        }
        else
        {
            for (auto& t : textures)
                t.Reset();
        }
    }
    else
    {
        for (auto& t : textures)
            t.Reset();
    }

    // Reset phong defaults
    phong = PhongMaterialData{};
    type = PHONG;
    name = "material";
}

void BasicMaterial::setDiffuseTexture(ComPtr<ID3D12Resource> tex)
{
    textures[SLOT_DIFFUSE] = tex;
    phong.hasDiffuseTex = (tex != nullptr) ? TRUE : FALSE;

    // Keep descriptors in sync
    updateDescriptors();
}

void BasicMaterial::ensureTextureTable()
{
    if (textureTable)
        return;

    if (!app)
        return;

    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    if (!descs)
        return;

    textureTable = descs->allocTable();
}

void BasicMaterial::updateDescriptors()
{
    ensureTextureTable();

    if (!textureTable)
        return;

    for (uint32_t slot = 0; slot < SLOT_COUNT; ++slot)
    {
        ID3D12Resource* tex = textures[slot].Get();

        if (tex)
            textureTable.createTextureSRV(tex, (UINT8)slot);
        else
            textureTable.createNullTexture2DSRV((UINT8)slot);
    }

    phong.hasDiffuseTex = (textures[SLOT_DIFFUSE] != nullptr) ? TRUE : FALSE;
}
