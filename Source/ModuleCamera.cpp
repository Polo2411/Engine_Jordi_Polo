#include "Globals.h"
#include "ModuleCamera.h"

#include "Application.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "imgui.h"

#include <algorithm> // std::max

namespace
{
    constexpr float MAX_PITCH = XMConvertToRadians(89.0f);
    constexpr float MOUSE_SENS = 0.20f; // grados por pixel
}

// ---------------------------------------------------------
bool ModuleCamera::init()
{
    setHorizontalFov(XM_PIDIV4);
    setPlaneDistances(0.1f, 200.0f);

    // Pivot inicial: lo que se haya seteado como bounds (por defecto Zero)
    orbitPivot = focusCenter;
    orbitDistance = (position - orbitPivot).Length();

    // Arranque Unity-like: mirar al pivot y alinear posición con la orientación resultante
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

    // Focus edge
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
// a) While Right-clicking, WASD fps-like movement and free look around must be enabled.
// e) Holding SHIFT duplicates movement speed.
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
// Flechas (auxiliar): Left/Right corregido para que no vaya invertido
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
// b) The mouse wheel should zoom in and out.
// c) Alt+Left click should orbit the object.
// a) RMB free look around.
void ModuleCamera::handleMouse(float dt)
{
    (void)dt;

    Mouse& m = Mouse::Get();
    auto ms = m.GetState();

    // Wheel zoom (dolly along view direction)
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

    // Alt + LMB => orbit
    if (ks.LeftAlt && ms.leftButton)
    {
        applyOrbitPixels(dx, dy);
        return;
    }

    // RMB => freelook (rotation)
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
// Wheel zoom: dolly forward/back (towards where you're looking)
void ModuleCamera::applyWheelZoomTicks(float ticks)
{
    float dist = (position - orbitPivot).Length();
    dist = clampf(dist, 0.2f, 500.0f);

    float scale = wheelZoomSpeed * (0.15f * dist + 0.5f);

    // ticks > 0 => zoom in
    position += front() * (ticks * scale);

    orbitDistance = (position - orbitPivot).Length();
    orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);

    viewDirty = true;
}

// ---------------------------------------------------------
// Orbit: Alt+LMB
void ModuleCamera::applyOrbitPixels(float dx, float dy)
{
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
}

// ---------------------------------------------------------
// d) Pressing “f” should focus the camera on the geometry.
// Versión con lookAt() + garantía de centrado exacto (sin drift).
void ModuleCamera::focus()
{
    orbitPivot = focusCenter;

    // 1) Distancia para encuadrar esfera de radio focusRadius con el FOV vertical
    float r = clampf(focusRadius, 0.01f, 100000.0f);
    float dist = r / tanf(vFovRad * 0.5f);

    // Margen tipo Unity
    orbitDistance = clampf(dist * 1.25f, 0.2f, 500.0f);

    // 2) Mantén la “dirección” desde la que estabas mirando (Unity feel)
    Vector3 dir = position - orbitPivot;
    if (dir.LengthSquared() < 1e-8f)
        dir = Vector3(0, 0, 1);
    dir.Normalize();

    // 3) Coloca cámara en esa dirección a la distancia correcta
    position = orbitPivot + dir * orbitDistance;

    // 4) Fija rotación mirando al pivot
    lookAt(orbitPivot);

    // 5) GARANTÍA: re-colocar con front() resultante de lookAt
    //    (esto asegura que el pivot cae EXACTAMENTE en el centro)
    position = orbitPivot - front() * orbitDistance;

    viewDirty = true;
}

// ---------------------------------------------------------
void ModuleCamera::lookAt(const Vector3& target)
{
    Vector3 d = target - position;
    if (d.LengthSquared() < 1e-8f) return;

    d.Normalize();

    // Manteniendo convención: vector forward base = (0,0,-1)
    yawRad = atan2f(d.x, -d.z);
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

    // View estable: la cámara mira en su dirección front() actual.
    // Usamos up() para consistencia con la orientación (roll=0).
    view = Matrix::CreateLookAt(position, position + front(), up());

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
