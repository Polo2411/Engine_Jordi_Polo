#include "Globals.h"
#include "BasicModel.h"

#include <string>
#include <cmath>
#include <cfloat>
#include <algorithm>

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

        return Vector3(radToDeg(pitch), radToDeg(yaw), radToDeg(roll));
    }

    static bool readAccessorMinMaxVec3(const tinygltf::Accessor& acc, Vector3& outMin, Vector3& outMax)
    {
        if (acc.minValues.size() >= 3 && acc.maxValues.size() >= 3)
        {
            outMin = Vector3(
                float(acc.minValues[0]),
                float(acc.minValues[1]),
                float(acc.minValues[2]));

            outMax = Vector3(
                float(acc.maxValues[0]),
                float(acc.maxValues[1]),
                float(acc.maxValues[2]));

            return true;
        }
        return false;
    }

    static bool computeAccessorMinMaxVec3FromBuffer(const tinygltf::Model& model, const tinygltf::Accessor& acc, Vector3& outMin, Vector3& outMax)
    {
        if (acc.type != TINYGLTF_TYPE_VEC3)
            return false;
        if (acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
            return false;
        if (acc.bufferView < 0 || acc.bufferView >= (int)model.bufferViews.size())
            return false;

        const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
        if (bv.buffer < 0 || bv.buffer >= (int)model.buffers.size())
            return false;

        const tinygltf::Buffer& buf = model.buffers[bv.buffer];
        const size_t byteStride = (bv.byteStride > 0) ? size_t(bv.byteStride) : sizeof(float) * 3;

        const size_t start = size_t(bv.byteOffset) + size_t(acc.byteOffset);
        const size_t needed = start + (size_t(acc.count) - 1) * byteStride + sizeof(float) * 3;
        if (needed > buf.data.size())
            return false;

        const uint8_t* base = buf.data.data() + start;

        Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
        Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (size_t i = 0; i < size_t(acc.count); ++i)
        {
            const float* p = reinterpret_cast<const float*>(base + i * byteStride);
            const Vector3 v(p[0], p[1], p[2]);

            mn.x = std::min(mn.x, v.x);
            mn.y = std::min(mn.y, v.y);
            mn.z = std::min(mn.z, v.z);

            mx.x = std::max(mx.x, v.x);
            mx.y = std::max(mx.y, v.y);
            mx.z = std::max(mx.z, v.z);
        }

        outMin = mn;
        outMax = mx;
        return true;
    }

    static bool tryGetPrimitivePositionBounds(const tinygltf::Model& model, const tinygltf::Primitive& prim, Vector3& outMin, Vector3& outMax)
    {
        auto it = prim.attributes.find("POSITION");
        if (it == prim.attributes.end())
            return false;

        const int accessorIndex = it->second;
        if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size())
            return false;

        const tinygltf::Accessor& acc = model.accessors[accessorIndex];

        if (readAccessorMinMaxVec3(acc, outMin, outMax))
            return true;

        return computeAccessorMinMaxVec3FromBuffer(model, acc, outMin, outMax);
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

    // Reset bounds
    hasBounds = false;
    localBoundsMin = Vector3(0.0f, 0.0f, 0.0f);
    localBoundsMax = Vector3(0.0f, 0.0f, 0.0f);
    localBoundsCenter = Vector3(0.0f, 0.0f, 0.0f);
    localBoundsRadius = 1.0f;

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
    size_t primitiveCount = 0;
    for (const tinygltf::Mesh& m : model.meshes)
        primitiveCount += m.primitives.size();

    meshes.reserve(primitiveCount);

    Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool any = false;

    for (const tinygltf::Mesh& m : model.meshes)
    {
        for (const tinygltf::Primitive& p : m.primitives)
        {
            // Build mesh
            BasicMesh mesh;
            mesh.load(model, m, p);
            meshes.push_back(std::move(mesh));

            // Accumulate local bounds from POSITION
            Vector3 pMin, pMax;
            if (tryGetPrimitivePositionBounds(model, p, pMin, pMax))
            {
                any = true;

                mn.x = std::min(mn.x, pMin.x);
                mn.y = std::min(mn.y, pMin.y);
                mn.z = std::min(mn.z, pMin.z);

                mx.x = std::max(mx.x, pMax.x);
                mx.y = std::max(mx.y, pMax.y);
                mx.z = std::max(mx.z, pMax.z);
            }
        }
    }

    if (any)
    {
        hasBounds = true;
        localBoundsMin = mn;
        localBoundsMax = mx;

        localBoundsCenter = (mn + mx) * 0.5f;
        const Vector3 extents = (mx - mn) * 0.5f;
        localBoundsRadius = extents.Length();
        if (localBoundsRadius < 0.0001f)
            localBoundsRadius = 0.0001f;
    }
    else
    {
        hasBounds = false;
        localBoundsMin = Vector3(0.0f, 0.0f, 0.0f);
        localBoundsMax = Vector3(0.0f, 0.0f, 0.0f);
        localBoundsCenter = Vector3(0.0f, 0.0f, 0.0f);
        localBoundsRadius = 1.0f;
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
