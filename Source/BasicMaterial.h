#pragma once

#include "Globals.h"
#include "ShaderTableDesc.h"

#include <string>
#include <array>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

// Matches Exercise6.hlsli packing
struct PhongMaterialData
{
    Vector4 diffuseColour = Vector4(1, 1, 1, 1);   // Cd + alpha
    Vector3 specularColour = Vector3(0.04f, 0.04f, 0.04f); // F0
    float   shininess = 64.0f;                     // n

    BOOL    hasDiffuseTex = FALSE;                 // 4 bytes
    float   _pad0[3] = { 0,0,0 };                  // padding to 16-byte
};

class BasicMaterial
{
public:
    enum MaterialType
    {
        PHONG = 0,
        UNKNOWN
    };

    // We currently only bind one texture in the exercises (t0 diffuse)
    static constexpr uint32_t SLOT_DIFFUSE = 0;
    static constexpr uint32_t SLOT_COUNT = 1;

public:
    BasicMaterial() = default;
    ~BasicMaterial();

    BasicMaterial(const BasicMaterial&) = default;
    BasicMaterial& operator=(const BasicMaterial&) = default;

    BasicMaterial(BasicMaterial&&) noexcept = default;
    BasicMaterial& operator=(BasicMaterial&&) noexcept = default;

    void reset();

    // Metadata
    void setName(const char* n) { name = (n ? n : "material"); }
    const std::string& getName() const { return name; }

    void setMaterialType(MaterialType t) { type = t; }
    MaterialType getMaterialType() const { return type; }

    // Phong
    const PhongMaterialData& getPhongMaterial() const { return phong; }
    void setPhongMaterial(const PhongMaterialData& d) { phong = d; }

    // Texture resource management (owned by material)
    void setDiffuseTexture(ComPtr<ID3D12Resource> tex);
    ID3D12Resource* getDiffuseTexture() const { return textures[SLOT_DIFFUSE].Get(); }

    // Descriptor table (shader-visible SRVs)
    void ensureTextureTable();     // alloc if needed
    void updateDescriptors();      // write SRVs/nulls for current textures

    D3D12_GPU_DESCRIPTOR_HANDLE getTexturesTableGPU() const { return textureTable.getGPUHandle(0); }
    const ShaderTableDesc& getTexturesTable() const { return textureTable; }

private:
    std::string name = "material";
    MaterialType type = PHONG;

    PhongMaterialData phong;

    std::array<ComPtr<ID3D12Resource>, SLOT_COUNT> textures{};

    // One table per material (8 slots available, we use SLOT_COUNT)
    ShaderTableDesc textureTable;
};
