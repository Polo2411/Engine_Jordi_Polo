#pragma once

#include <vector>
#include <string>

#include "MathUtils.h"
#include "BasicMesh.h"
#include "BasicMaterial.h"

namespace tinygltf { class Model; }

class BasicModel
{
public:
    BasicModel() = default;
    ~BasicModel() = default;

    void load(const char* fileName, const char* basePath, BasicMaterial::Type materialType);

    uint32_t getNumMeshes() const { return (uint32_t)meshes.size(); }
    uint32_t getNumMaterials() const { return (uint32_t)materials.size(); }

    const std::vector<BasicMesh>& getMeshes() const { return meshes; }
    std::vector<BasicMaterial>& getMaterials() { return materials; }
    const std::vector<BasicMaterial>& getMaterials() const { return materials; }

    // TRS (degrees)
    Vector3& translation();
    Vector3& rotationDeg();   // Euler degrees
    Vector3& scale();

    const Matrix& getModelMatrix() const;

    // Needed for ImGuizmo workflow
    void setModelMatrix(const Matrix& m);

    const std::string& getSrcFile() const { return srcFile; }

private:
    void loadMeshes(const tinygltf::Model& model);
    void loadMaterials(const tinygltf::Model& model, const char* basePath, BasicMaterial::Type materialType);

    void rebuildTransformIfNeeded() const;

private:
    std::vector<BasicMaterial> materials;
    std::vector<BasicMesh>     meshes;

    std::string srcFile;

    Vector3 t = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 rDeg = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 s = Vector3(1.0f, 1.0f, 1.0f);

    mutable bool   dirtyTransform = true;
    mutable Matrix modelMatrix = Matrix::Identity;
};
