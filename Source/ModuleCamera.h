#pragma once

#include "Module.h"
#include "SimpleMath.h"           // Vector2, Vector3, Matrix, Quaternion
#include "DirectXCollision.h"     // BoundingFrustum

using namespace DirectX;
using namespace DirectX::SimpleMath;

class ModuleCamera : public Module
{
public:
    ModuleCamera() = default;
    ~ModuleCamera() override = default;

    bool init() override;
    void update() override;

    // --- Enable / Settings ---
    void setEnabled(bool v) { enabled = v; }
    bool isEnabled() const { return enabled; }

    void setMoveSpeed(float v) { moveSpeed = v; }
    void setRotateSpeed(float v) { rotateSpeedDeg = v; }
    void setShiftMultiplier(float v) { shiftMultiplier = v; }

    // Unity-like: movement only while RMB is pressed
    void setRequireRMBToMove(bool v) { requireRMBToMove = v; }
    bool getRequireRMBToMove() const { return requireRMBToMove; }

    void setClampPitch(bool v) { clampPitch = v; }
    bool getClampPitch() const { return clampPitch; }

    // --- Camera configuration (PowerPoint API) ---
    // SetFOV: sets HORIZONTAL FOV keeping aspect ratio (HOR+ source-of-truth)
    void setHorizontalFov(float fovRadians);
    float getHorizontalFov() const { return hFovRad; }

    // SetAspectRatio: changes VERTICAL FOV to meet new aspect ratio (HOR+)
    void setAspectRatio(float newAspect);
    float getAspectRatio() const { return aspect; }

    void setPlaneDistances(float newNear, float newFar);
    float getNearPlane() const { return nearPlane; }
    float getFarPlane()  const { return farPlane; }

    // Transform
    void setPosition(const Vector3& p);
    const Vector3& getPosition() const { return position; }

    void setOrientation(const Quaternion& q);
    const Quaternion& getOrientation() const { return orientation; }

    void lookAt(const Vector3& target, const Vector3& worldUp = Vector3::Up);

    // --- Orbit/Focus helpers ---
    void setFocusBounds(const Vector3& center, float radius); // lo llamas desde Exercise3
    void focus();                                             // tecla F

    void setOrbitEnabled(bool v) { orbitEnabled = v; }
    bool isOrbitEnabled() const { return orbitEnabled; }


    // Matrices
    const Matrix& getViewMatrix() const { return view; }
    const Matrix& getProjectionMatrix() const { return proj; }

    // Frustum helpers
    BoundingFrustum getFrustum() const;
    void getFrustumPlanes(Vector4 planesOut[6], bool normalize = true) const;

    // Basis vectors (from orientation)
    Vector3 front() const;       // forward
    Vector3 up() const;
    Vector3 right() const;

private:
    // --- Internal update pipeline ---
    void handleKeyboard(float dt);
    void handleMouse(float dt);
    void handleArrowRotation(float dt);

    void recalcProjectionIfNeeded();
    void recalcViewIfNeeded();

    static float computeVerticalFovFromHorizontal(float hFov, float aspect);
    static float clampf(float v, float lo, float hi);

private:
    // State
    bool enabled = true;

    // Camera params
    float nearPlane = 0.1f;
    float farPlane = 200.0f;

    // HOR+ setup: horizontal is “main”
    float hFovRad = XM_PIDIV4;
    float vFovRad = XM_PIDIV4; // derived
    float aspect = 16.0f / 9.0f;

    // Transform
    Vector3 position = Vector3(0.0f, 2.0f, 10.0f);
    float yawRad = 0.0f;   // around world Y
    float pitchRad = 0.0f;   // around camera local X (approx)
    Quaternion orientation = Quaternion::Identity;

    // Matrices
    Matrix view = Matrix::Identity;
    Matrix proj = Matrix::Identity;

    // Dirty flags (only recompute when needed)
    mutable bool projDirty = true;
    mutable bool viewDirty = true;

    // Input settings
    float moveSpeed = 4.0f;         // units/sec
    float rotateSpeedDeg = 120.0f;  // deg/sec (arrows)
    float shiftMultiplier = 3.0f;

    bool requireRMBToMove = true; // Unity-like
    bool clampPitch = true;

    // Mouse tracking
    bool hasPrevMouse = false;
    int  prevMouseX = 0;
    int  prevMouseY = 0;
    bool mouseEverUsedForRotation = false;
    // ✅ FIX latigazo: track RMB edge properly as MEMBER (NOT static local)
    bool wasRightMouseDown = false;

    // --- Wheel zoom ---
    int  prevWheel = 0;
    bool hasPrevWheel = false;
    float zoomSpeed = 0.01f;     // unidades por "wheel tick" (ajusta)
    float minDistance = 0.25f;
    float maxDistance = 500.0f;

    // --- Orbit / focus ---
    bool  orbitEnabled = true;
    Vector3 orbitPivot = Vector3::Zero;
    float orbitDistance = 10.0f; // distancia al pivot
    float focusRadius = 1.0f;    // radio estimado de la geometría

    // Para detectar flancos (edge) de F
    bool prevKeyF = false;

    // --- Focus / Orbit state ---
    Vector3 focusCenter = Vector3::Zero;   // centro del objeto enfocado
    float   focusRadius = 1.0f;             // tamaño aproximado

    Vector3 orbitPivot = Vector3::Zero;     // punto alrededor del que orbitamos
    float   orbitDistance = 10.0f;           // distancia cámara ↔ pivote


};
