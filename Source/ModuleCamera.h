#pragma once

#include "Module.h"
#include "SimpleMath.h"
#include "DirectXCollision.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

class ModuleCamera : public Module
{
public:
    bool init() override;
    void update() override;

    // --- Config ---
    void setHorizontalFov(float fovRadians);
    void setAspectRatio(float newAspect);
    void setPlaneDistances(float newNear, float newFar);

    // --- Transform ---
    void setPosition(const Vector3& p);
    const Vector3& getPosition() const { return position; }

    void lookAt(const Vector3& target);

    // --- Focus / Orbit ---
    void setFocusBounds(const Vector3& center, float radius);
    void focus();

    // --- Matrices ---
    const Matrix& getViewMatrix() const { return view; }
    const Matrix& getProjectionMatrix() const { return proj; }

    // --- Basis ---
    Vector3 front() const;
    Vector3 right() const;
    Vector3 up() const;

private:
    // Update helpers
    void handleKeyboard(float dt);
    void handleMouse();
    void handleArrowRotation(float dt);

    void recalcProjectionIfNeeded();
    void recalcViewIfNeeded();

    static float clampf(float v, float lo, float hi);
    static float computeVerticalFovFromHorizontal(float hFov, float aspect);

private:
    // --- Camera state ---
    Vector3 position = Vector3(0, 2, 10);
    float yawRad = 0.0f;
    float pitchRad = 0.0f;
    Quaternion orientation = Quaternion::Identity;

    // --- Projection ---
    float hFovRad = XM_PIDIV4;
    float vFovRad = XM_PIDIV4;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 200.0f;

    // --- Matrices ---
    Matrix view = Matrix::Identity;
    Matrix proj = Matrix::Identity;
    bool viewDirty = true;
    bool projDirty = true;

    // --- Movement ---
    float moveSpeed = 4.0f;
    float rotateSpeedDeg = 120.0f;
    float shiftMultiplier = 3.0f;

    // --- Mouse ---
    bool hasPrevMouse = false;
    int prevMouseX = 0;
    int prevMouseY = 0;
    bool wasRightMouseDown = false;

    // --- Wheel ---
    bool hasPrevWheel = false;
    int prevWheel = 0;

    // --- Orbit / Focus ---
    Vector3 focusCenter = Vector3::Zero;
    float focusRadius = 1.0f;

    Vector3 orbitPivot = Vector3::Zero;
    float orbitDistance = 10.0f;

    // --- Key edges ---
    bool prevKeyF = false;
};
