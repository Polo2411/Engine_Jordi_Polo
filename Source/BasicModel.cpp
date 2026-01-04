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
    inline float degToRad(float deg) { return deg * (XM_PI / 180.0f); }
}

void BasicModel::load(const char* fileName, const char* basePath, BasicMaterial::Type materialType)
{
    tinygltf::TinyGLTF gltf;
    tinygltf::Model gltfModel;

    std::string error, warning;
    const bool ok = gltf.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName);

    if (!warning.empty())
        LOG("glTF warning: %s", warning.c_str());

    if (!ok)
    {
        LOG("glTF error loading %s: %s", fileName ? fileName : "(null)", error.c_str());
        return;
    }

    srcFile = fileName ? fileName : "";

    materials.clear();
    meshes.clear();

    loadMaterials(gltfModel, basePath, materialType);
    loadMeshes(gltfModel);

    // reset transform
    t = Vector3::Zero;
    rDeg = Vector3::Zero;
    s = Vector3::One;
    dirtyTransform = true;
}

void BasicModel::loadMeshes(const tinygltf::Model& model)
{
    size_t primitiveCount = 0;
    for (const tinygltf::Mesh& m : model.meshes)
        primitiveCount += m.primitives.size();

    meshes.clear();
    meshes.reserve(primitiveCount);

    for (const tinygltf::Mesh& m : model.meshes)
    {
        for (const tinygltf::Primitive& p : m.primitives)
        {
            BasicMesh bm;
            bm.load(model, m, p);
            meshes.push_back(std::move(bm));
        }
    }
}

void BasicModel::loadMaterials(const tinygltf::Model& model, const char* basePath, BasicMaterial::Type materialType)
{
    materials.clear();
    materials.resize(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i)
        materials[i].load(model, model.materials[i], materialType, basePath);
}

Vector3& BasicModel::translation() { dirtyTransform = true; return t; }
Vector3& BasicModel::rotationDeg() { dirtyTransform = true; return rDeg; }
Vector3& BasicModel::scale() { dirtyTransform = true; return s; }

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
