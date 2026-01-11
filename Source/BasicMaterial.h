// BasicMaterial.h
#pragma once

#include "Globals.h"
#include "ShaderTableDesc.h"

#include <d3d12.h>
#include <wrl.h>

#include <array>
#include <string>
#include <cstdint>

namespace tinygltf { class Model; struct Material; }

class BasicMaterial
{
public:
    // Keep legacy naming used across the project (BasicModel, Exercises, etc.)
    enum Type : uint32_t
    {
        BASIC = 0,
        PHONG = 1
    };

    // We currently bind only one SRV: base/diffuse colour
    static constexpr uint32_t SLOT_COUNT = 1;

    // Matches Exercise6/Exercise7 .hlsli layout (16-byte aligned packing)
    struct PhongMaterialData
    {
        Vector4 diffuseColour = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // Cd (rgb) + alpha
        Vector3 specularColour = Vector3(0.04f, 0.04f, 0.04f);   // F0 (rgb)
        float   shininess = 64.0f;                               // n

        uint32_t hasDiffuseTex = 0;                              // BOOL-like (4 bytes)
        Vector3  _pad0 = Vector3::Zero;                          // padding to 16-byte
    };

public:
    BasicMaterial() = default;
    ~BasicMaterial();

    BasicMaterial(const BasicMaterial&) = delete;
    BasicMaterial& operator=(const BasicMaterial&) = delete;

    BasicMaterial(BasicMaterial&&) noexcept = default;
    BasicMaterial& operator=(BasicMaterial&&) noexcept = default;

    void load(const tinygltf::Model& model,
        const tinygltf::Material& material,
        Type type,
        const char* basePath);

    void releaseResources();

    const std::string& getName() const { return name; }
    Type getMaterialType() const { return materialType; }

    // Phong accessors (used by Exercise6/7 UI)
    PhongMaterialData getPhongMaterial() const { return phong; }
    void setPhongMaterial(const PhongMaterialData& p);

    // SRV table handle (used by Exercise6/7 root param t0)
    D3D12_GPU_DESCRIPTOR_HANDLE getTexturesTableGPU() const { return texturesTable.getGPUHandle(); }

private:
    void rebuildDescriptorTable();
    void enforceTextureFlags();

private:
    std::string name = "material";
    Type materialType = PHONG;

    // Slot 0 = diffuse/baseColor
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, SLOT_COUNT> textures = {};

    ShaderTableDesc texturesTable; // allocated from ModuleShaderDescriptors

    PhongMaterialData phong = {};
};
