#include "Globals.h"
#include "BasicModel.h"

#include <string>
#include <cmath>

#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267)
#include "tiny_gltf.h"
#pragma warning(pop)

using namespace DirectX;

namespace
{
    inline float degToRad(float deg) { return deg * (XM_PI / 180.0f); }
    inline float radToDeg(float rad) { return rad * (180.0f / XM_PI); }

    static Vector3 quatToEulerDeg(const XMFLOAT4& qf)
    {
        // Standard yaw/pitch/roll from quaternion
        const float x = qf.x;
        const float y = qf.y;
        const float z = qf.z;
        const float w = qf.w;

        // roll (Z)
        const float sinr_cosp = 2.0f * (w * x + y * z);
        const float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        const float roll = std::atan2(sinr_cosp, cosr_cosp);

        // pitch (X)
        const float sinp = 2.0f * (w * y - z * x);
        float pitch = 0.0f;
        if (std::fabs(sinp) >= 1.0f)
            pitch = std::copysign(XM_PIDIV2, sinp);
        else
            pitch = std::asin(sinp);

        // yaw (Y)
        const float siny_cosp = 2.0f * (w * z + x * y);
        const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        const float yaw = std::atan2(siny_cosp, cosy_cosp);

        // Map to your convention: rDeg.x=pitch, rDeg.y=yaw, rDeg.z=roll
        return Vector3(radToDeg(pitch), radToDeg(yaw), radToDeg(roll));
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

    // Reset TRS
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
        for (const tinygltf::Primitive& p : m.primitives)
        {
            BasicMesh mesh;
            mesh.load(model, m, p);
            meshes.push_back(std::move(mesh));
        }
    }
}

void BasicModel::loadMaterials(const tinygltf::Model& model, const char* basePath, BasicMaterial::Type materialType)
{
    materials.resize(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i)
        materials[i].load(model, model.materials[i], materialType, basePath);
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

void BasicModel::setModelMatrix(const Matrix& m)
{
    modelMatrix = m;
    dirtyTransform = false;

    // Keep TRS in sync for the ImGui fields
    XMVECTOR scaleV, rotQ, transV;
    const XMMATRIX xm = (XMMATRIX)m;

    if (XMMatrixDecompose(&scaleV, &rotQ, &transV, xm))
    {
        XMFLOAT3 sc; XMStoreFloat3(&sc, scaleV);
        XMFLOAT3 tr; XMStoreFloat3(&tr, transV);
        XMFLOAT4 rq; XMStoreFloat4(&rq, rotQ);

        s = Vector3(sc.x, sc.y, sc.z);
        t = Vector3(tr.x, tr.y, tr.z);
        rDeg = quatToEulerDeg(rq);
    }
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
