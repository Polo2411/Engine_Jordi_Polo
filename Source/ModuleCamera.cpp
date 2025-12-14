#include "Globals.h"
#include "ModuleCamera.h"

#include "Application.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "imgui.h"

namespace
{
    constexpr float MAX_PITCH = XMConvertToRadians(89.0f);
    constexpr float MOUSE_SENS = 0.20f;
}

// ---------------------------------------------------------
bool ModuleCamera::init()
{
    setHorizontalFov(XM_PIDIV4);
    setPlaneDistances(0.1f, 200.0f);

    lookAt(Vector3::Zero);

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
    float dt = app->getElapsedMilis() * 0.001f;
    dt = clampf(dt, 0.00001f, 0.25f);

    Keyboard& kb = Keyboard::Get();
    auto ks = kb.GetState();

    if (ks.F && !prevKeyF)
        focus();
    prevKeyF = ks.F;

    if (!ImGui::GetIO().WantCaptureMouse)
        handleMouse();

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
    if (ks.Q) move.y += 1;
    if (ks.E) move.y -= 1;

    if (move.LengthSquared() < 1e-6f) return;

    move.Normalize();

    float speed = moveSpeed * (ks.LeftShift ? shiftMultiplier : 1.0f);
    position += (right() * move.x + front() * -move.z + Vector3::Up * move.y) * speed * dt;

    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::handleArrowRotation(float dt)
{
    Keyboard& kb = Keyboard::Get();
    auto ks = kb.GetState();

    Vector2 r = Vector2::Zero;
    if (ks.Left) r.x -= 1;
    if (ks.Right) r.x += 1;
    if (ks.Up) r.y += 1;
    if (ks.Down) r.y -= 1;

    if (r.LengthSquared() < 1e-6f) return;

    yawRad += r.x * XMConvertToRadians(rotateSpeedDeg) * dt;
    pitchRad += r.y * XMConvertToRadians(rotateSpeedDeg) * dt;
    pitchRad = clampf(pitchRad, -MAX_PITCH, MAX_PITCH);

    orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);
    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::handleMouse()
{
    Mouse& m = Mouse::Get();
    auto ms = m.GetState();

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
        orbitDistance *= powf(0.9f, ticks);
        orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);
        position = orbitPivot - front() * orbitDistance;
        viewDirty = true;
    }

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

    if (ks.LeftAlt && ms.leftButton)
    {
        yawRad += XMConvertToRadians(MOUSE_SENS * dx);
        pitchRad += XMConvertToRadians(MOUSE_SENS * dy);
        pitchRad = clampf(pitchRad, -MAX_PITCH, MAX_PITCH);

        orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);
        position = orbitPivot - front() * orbitDistance;
        viewDirty = true;
        return;
    }

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
void ModuleCamera::focus()
{
    orbitPivot = focusCenter;
    float dist = focusRadius / tanf(vFovRad * 0.5f);
    orbitDistance = dist * 1.25f;
    position = orbitPivot - front() * orbitDistance;
    lookAt(orbitPivot);
}

// ---------------------------------------------------------
void ModuleCamera::setFocusBounds(const Vector3& center, float radius)
{
    focusCenter = center;
    focusRadius = max(radius, 0.01f);
    orbitPivot = focusCenter;
}

// ---------------------------------------------------------
void ModuleCamera::lookAt(const Vector3& target)
{
    Vector3 d = target - position;
    d.Normalize();

    yawRad = atan2f(d.x, -d.z);
    pitchRad = asinf(clampf(d.y, -1, 1));

    orientation = Quaternion::CreateFromYawPitchRoll(yawRad, pitchRad, 0);
    viewDirty = true;
}

// ---------------------------------------------------------
Vector3 ModuleCamera::front() const { return Vector3::Transform(Vector3(0, 0, -1), orientation); }
Vector3 ModuleCamera::right() const { return Vector3::Transform(Vector3(1, 0, 0), orientation); }
Vector3 ModuleCamera::up() const { return Vector3::Transform(Vector3(0, 1, 0), orientation); }

// ---------------------------------------------------------
void ModuleCamera::recalcProjectionIfNeeded()
{
    if (!projDirty) return;
    proj = Matrix::CreatePerspectiveFieldOfView(vFovRad, aspect, nearPlane, farPlane);
    projDirty = false;
}

void ModuleCamera::recalcViewIfNeeded()
{
    if (!viewDirty) return;
    Matrix world = Matrix::CreateWorld(position, front(), up());
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
