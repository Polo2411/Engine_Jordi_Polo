#include "Globals.h"
#include "ModuleCamera.h"

#include "Application.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "imgui.h"

#include <algorithm>

namespace
{
    constexpr float MAX_PITCH = XMConvertToRadians(89.0f);
    constexpr float MOUSE_SENS = 0.20f; // degrees per pixel
}

// ---------------------------------------------------------
bool ModuleCamera::init()
{
    setHorizontalFov(XM_PIDIV4);
    setPlaneDistances(0.1f, 200.0f);

    orbitPivot = focusCenter;
    orbitDistance = (position - orbitPivot).Length();

    lookAt(orbitPivot);
    position = orbitPivot - front() * orbitDistance;
    viewDirty = true;

    Mouse& m = Mouse::Get();
    auto ms = m.GetState();
    prevMouseX = ms.x;
    prevMouseY = ms.y;
    hasPrevMouse = true;

    return true;
}

// ---------------------------------------------------------
void ModuleCamera::update()
{
    float dt = (float)app->getDeltaTimeSeconds();
    dt = clampf(dt, 0.00001f, 0.25f);

    Keyboard& kb = Keyboard::Get();
    auto ks = kb.GetState();

    if (ks.F && !prevKeyF)
        focus();
    prevKeyF = ks.F;

    if (!ImGui::GetIO().WantCaptureMouse)
        handleMouse(dt);

    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        handleKeyboard(dt);
        handleArrowRotation(dt);
    }

    recalcProjectionIfNeeded();
    recalcViewIfNeeded();
}

// ---------------------------------------------------------
void ModuleCamera::handleKeyboard(float dt)
{
    Keyboard& kb = Keyboard::Get();
    auto ks = kb.GetState();

    Mouse& m = Mouse::Get();
    auto ms = m.GetState();

    if (!ms.rightButton) return;

    Vector3 move = Vector3::Zero;
    if (ks.W) move.z -= 1;
    if (ks.S) move.z += 1;
    if (ks.A) move.x -= 1;
    if (ks.D) move.x += 1;

    if (move.LengthSquared() < 1e-6f) return;
    move.Normalize();

    float speed = moveSpeed * (ks.LeftShift ? shiftMultiplier : 1.0f);

    position += (right() * move.x + front() * -move.z) * speed * dt;

    orbitDistance = (position - orbitPivot).Length();
    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::handleArrowRotation(float dt)
{
    Keyboard& kb = Keyboard::Get();
    auto ks = kb.GetState();

    Vector2 r = Vector2::Zero;
    if (ks.Left)  r.x -= 1;
    if (ks.Right) r.x += 1;
    if (ks.Up)    r.y += 1;
    if (ks.Down)  r.y -= 1;

    if (r.LengthSquared() < 1e-6f) return;

    yawRad += (-r.x) * XMConvertToRadians(rotateSpeedDeg) * dt;
    pitchRad += (r.y) * XMConvertToRadians(rotateSpeedDeg) * dt;
    pitchRad = clampf(pitchRad, -MAX_PITCH, MAX_PITCH);

    orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);
    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::handleMouse(float dt)
{
    (void)dt;

    Mouse& m = Mouse::Get();
    auto ms = m.GetState();

    // Wheel zoom
    if (!hasPrevWheel)
    {
        prevWheel = ms.scrollWheelValue;
        hasPrevWheel = true;
    }

    int wheelDelta = ms.scrollWheelValue - prevWheel;
    prevWheel = ms.scrollWheelValue;

    if (wheelDelta != 0)
    {
        float ticks = wheelDelta / 120.0f;
        applyWheelZoomTicks(ticks);
    }

    // Mouse delta
    if (!hasPrevMouse)
    {
        prevMouseX = ms.x;
        prevMouseY = ms.y;
        hasPrevMouse = true;
        return;
    }

    int dx = ms.x - prevMouseX;
    int dy = ms.y - prevMouseY;
    prevMouseX = ms.x;
    prevMouseY = ms.y;

    Keyboard& kb = Keyboard::Get();
    auto ks = kb.GetState();

    // Alt + LMB => orbit around focusCenter
    if (ks.LeftAlt && ms.leftButton)
    {
        if (!wasOrbiting)
        {
            orbitPivot = focusCenter;
            orbitDistance = (position - orbitPivot).Length();
            orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);
            wasOrbiting = true;
        }

        applyOrbitPixels((float)dx, (float)dy);
        return;
    }

    wasOrbiting = false;

    // RMB => free look
    if (!ms.rightButton)
    {
        wasRightMouseDown = false;
        return;
    }

    if (!wasRightMouseDown)
    {
        wasRightMouseDown = true;
        return;
    }

    yawRad += XMConvertToRadians(MOUSE_SENS * dx);
    pitchRad += XMConvertToRadians(MOUSE_SENS * dy);
    pitchRad = clampf(pitchRad, -MAX_PITCH, MAX_PITCH);

    orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);
    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::applyWheelZoomTicks(float ticks)
{
    orbitPivot = focusCenter;

    float dist = (position - orbitPivot).Length();
    dist = clampf(dist, 0.2f, 500.0f);

    float scale = wheelZoomSpeed * (0.15f * dist + 0.5f);

    position += front() * (ticks * scale);

    orbitDistance = (position - orbitPivot).Length();
    orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);

    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::applyOrbitPixels(float dx, float dy)
{
    orbitPivot = focusCenter;

    yawRad += XMConvertToRadians(MOUSE_SENS * dx);
    pitchRad += XMConvertToRadians(MOUSE_SENS * dy);
    pitchRad = clampf(pitchRad, -MAX_PITCH, MAX_PITCH);

    orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);

    orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);
    position = orbitPivot - front() * orbitDistance;

    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::setFocusBounds(const Vector3& center, float radius)
{
    focusCenter = center;
    focusRadius = std::max(radius, 0.01f);

    orbitPivot = focusCenter;
    orbitDistance = (position - orbitPivot).Length();
    orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);
}

// ---------------------------------------------------------
void ModuleCamera::focus()
{
    orbitPivot = focusCenter;

    float r = clampf(focusRadius, 0.01f, 100000.0f);
    // Use the limiting FOV (Unity-like framing)
    const float effectiveFov = std::min(vFovRad, hFovRad);
    float dist = r / tanf(effectiveFov * 0.5f);

    // A bit more margin than before (prevents "too close")
    orbitDistance = clampf(dist * 1.25f, 0.2f, 500.0f);


    // Keep current viewing direction; if degenerate, use backward.
    Vector3 dir = position - orbitPivot;
    if (dir.LengthSquared() < 1e-8f)
        dir = Vector3(0, 0, 1);
    dir.Normalize();

    position = orbitPivot + dir * orbitDistance;

    lookAt(orbitPivot);
    position = orbitPivot - front() * orbitDistance;

    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::lookAt(const Vector3& target)
{
    Vector3 d = target - position;
    if (d.LengthSquared() < 1e-8f) return;

    d.Normalize();

    // Convention: base forward vector = (0,0,-1)
    // Fix: yaw sign so that front() matches the target direction
    yawRad = atan2f(-d.x, -d.z);

    pitchRad = asinf(clampf(d.y, -1.0f, 1.0f));
    pitchRad = clampf(pitchRad, -MAX_PITCH, MAX_PITCH);

    orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);
    viewDirty = true;
}


// ---------------------------------------------------------
Vector3 ModuleCamera::front() const { return Vector3::Transform(Vector3(0, 0, -1), orientation); }
Vector3 ModuleCamera::right() const { return Vector3::Transform(Vector3(1, 0, 0), orientation); }
Vector3 ModuleCamera::up() const { return Vector3::Transform(Vector3(0, 1, 0), orientation); }

// ---------------------------------------------------------
void ModuleCamera::setHorizontalFov(float fovRadians)
{
    hFovRad = clampf(fovRadians, XMConvertToRadians(10.0f), XMConvertToRadians(170.0f));
    vFovRad = computeVerticalFovFromHorizontal(hFovRad, aspect);
    projDirty = true;
}

void ModuleCamera::setAspectRatio(float newAspect)
{
    aspect = std::max(newAspect, 0.01f);
    vFovRad = computeVerticalFovFromHorizontal(hFovRad, aspect);
    projDirty = true;
}

void ModuleCamera::setPlaneDistances(float newNear, float newFar)
{
    nearPlane = std::max(newNear, 0.0001f);
    farPlane = std::max(newFar, nearPlane + 0.001f);
    projDirty = true;
}

void ModuleCamera::setPosition(const Vector3& p)
{
    position = p;
    orbitDistance = (position - orbitPivot).Length();
    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::recalcProjectionIfNeeded()
{
    if (!projDirty) return;
    proj = Matrix::CreatePerspectiveFieldOfView(vFovRad, aspect, nearPlane, farPlane);
    projDirty = false;
}

// ---------------------------------------------------------
void ModuleCamera::recalcViewIfNeeded()
{
    if (!viewDirty) return;

    // Camera world matrix (row-vector convention): World = R * T
    const Matrix R = Matrix::CreateFromQuaternion(orientation);
    const Matrix T = Matrix::CreateTranslation(position);
    const Matrix world = R * T;

    // View = inverse(world)
    view = world.Invert();

    viewDirty = false;
}

// ---------------------------------------------------------
float ModuleCamera::computeVerticalFovFromHorizontal(float hFov, float aspect)
{
    return 2.0f * atanf(tanf(hFov * 0.5f) / aspect);
}

float ModuleCamera::clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
