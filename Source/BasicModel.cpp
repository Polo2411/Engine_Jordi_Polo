#include "Globals.h"
#include "BasicModel.h"

#include <string>

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267)
#include "tiny_gltf.h"
#pragma warning(pop)

namespace
{
    inline float degToRad(float deg)
    {
        return deg * (XM_PI / 180.0f);
    }
}

void BasicModel::load(const char* fileName, const char* basePath, BasicMaterial::Type materialType)
{
    tinygltf::TinyGLTF gltfContext;
    tinygltf::Model model;
    std::string error, warning;

    const bool loadOk = gltfContext.LoadASCIIFromFile(&model, &error, &warning, fileName);

    if (!warning.empty())
        LOG("glTF warning: %s", warning.c_str());

    if (!loadOk)
    {
        LOG("Error loading %s: %s", fileName ? fileName : "(null)", error.c_str());
        return;
    }

    srcFile = fileName ? fileName : "";

    materials.clear();
    meshes.clear();

    loadMaterials(model, basePath, materialType);
    loadMeshes(model);

    // Reset transform
    t = Vector3(0.0f, 0.0f, 0.0f);
    rDeg = Vector3(0.0f, 0.0f, 0.0f);
    s = Vector3(1.0f, 1.0f, 1.0f);
    dirtyTransform = true;
}

void BasicModel::loadMeshes(const tinygltf::Model& model)
{
    // One BasicMesh per glTF primitive
    size_t primitiveCount = 0;
    for (const tinygltf::Mesh& m : model.meshes)
        primitiveCount += m.primitives.size();

    meshes.reserve(primitiveCount);

    for (const tinygltf::Mesh& m : model.meshes)
    {
        const char* debugName = m.name.empty() ? "gltf_mesh" : m.name.c_str();

        for (const tinygltf::Primitive& p : m.primitives)
        {
            BasicMesh mesh;
            mesh.load(model, p, debugName);
            meshes.push_back(std::move(mesh));
        }
    }
}

void BasicModel::loadMaterials(const tinygltf::Model& model, const char* basePath, BasicMaterial::Type materialType)
{
    materials.resize(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        materials[i].load(model, model.materials[i], materialType, basePath);
    }
}

Vector3& BasicModel::translation()
{
    dirtyTransform = true;
    return t;
}

Vector3& BasicModel::rotationDeg()
{
    dirtyTransform = true;
    return rDeg;
}

Vector3& BasicModel::scale()
{
    dirtyTransform = true;
    return s;
}

const Matrix& BasicModel::getModelMatrix() const
{
    rebuildTransformIfNeeded();
    return modelMatrix;
}

void BasicModel::rebuildTransformIfNeeded() const
{
    if (!dirtyTransform)
        return;

    const Matrix S = Matrix::CreateScale(s);

    const Matrix R = Matrix::CreateFromYawPitchRoll(
        degToRad(rDeg.y),
        degToRad(rDeg.x),
        degToRad(rDeg.z));

    const Matrix T = Matrix::CreateTranslation(t);

    modelMatrix = S * R * T;
    dirtyTransform = false;
}
