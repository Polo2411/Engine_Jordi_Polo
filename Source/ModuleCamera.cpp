#include "Globals.h"
#include "ModuleCamera.h"

#include "Application.h"
#include "Mouse.h"
#include "Keyboard.h"
#include "MathUtils.h"

// Si lo tienes en el proyecto, usamos ImGui para no pelear con el UI.
// Si te diera problemas, comenta estas 2 líneas.
#include "imgui.h"

namespace
{
    constexpr float kMaxPitchDeg = 89.0f;   // evitar flip
    constexpr float kMouseSensitivity = 0.20f;   // grados por pixel (ajusta a gusto)
}

// ---------------------------------------------------------
// Init
// ---------------------------------------------------------
bool ModuleCamera::init()
{
    setHorizontalFov(XM_PIDIV4);
    setPlaneDistances(0.1f, 200.0f);

    // Estado inicial
    position = Vector3(0.0f, 2.0f, 10.0f);
    yawRad = 0.0f;
    pitchRad = 0.0f;

    // Arranca mirando al origen (esto sincroniza yaw/pitch y orientation)
    lookAt(Vector3::Zero);

    // Sincroniza mouse inicial (evita deltas basura)
    Mouse& mouse = Mouse::Get();
    const Mouse::State& ms = mouse.GetState();
    prevMouseX = ms.x;
    prevMouseY = ms.y;
    hasPrevMouse = true;

    wasRightMouseDown = false;

    // Fuerza recálculo inicial
    projDirty = true;
    viewDirty = true;

    return true;
}

// ---------------------------------------------------------
// Update
// ---------------------------------------------------------
void ModuleCamera::update()
{
    float dt = app->getElapsedMilis() * 0.001f;

    // clamp razonable
    if (dt < 0.000001f) dt = 0.000001f;
    if (dt > 0.25f)     dt = 0.25f;

    // --- FOCUS (tecla F, con flanco) ---
    {
        Keyboard& keyboard = Keyboard::Get();
        const Keyboard::State& ks = keyboard.GetState();

        const bool keyF = ks.F;   // <- si tu Keyboard::State usa otro nombre, cámbialo aquí
        if (keyF && !prevKeyF)
        {
            focus();
        }
        prevKeyF = keyF;
    }

    if (enabled)
    {
        // Si ImGui existe: no mover cámara cuando UI quiera input
        if (ImGui::GetCurrentContext())
        {
            ImGuiIO& io = ImGui::GetIO();

            if (!io.WantCaptureKeyboard)
                handleArrowRotation(dt);

            if (!io.WantCaptureMouse)
                handleMouse(dt);

            if (!io.WantCaptureKeyboard)
                handleKeyboard(dt);
        }
        else
        {
            handleArrowRotation(dt);
            handleMouse(dt);
            handleKeyboard(dt);
        }
    }

    recalcProjectionIfNeeded();
    recalcViewIfNeeded();
}


// ---------------------------------------------------------
// PowerPoint API: FOV / Aspect / Planes
// ---------------------------------------------------------
void ModuleCamera::setHorizontalFov(float fovRadians)
{
    hFovRad = clampf(fovRadians, XMConvertToRadians(10.0f), XMConvertToRadians(170.0f));
    vFovRad = computeVerticalFovFromHorizontal(hFovRad, aspect);
    projDirty = true;
}

void ModuleCamera::setAspectRatio(float newAspect)
{
    aspect = (newAspect > 0.0001f) ? newAspect : (16.0f / 9.0f);
    vFovRad = computeVerticalFovFromHorizontal(hFovRad, aspect);
    projDirty = true;
}

void ModuleCamera::setPlaneDistances(float newNear, float newFar)
{
    nearPlane = (newNear < 0.001f) ? 0.001f : newNear;
    farPlane = (newFar <= nearPlane + 0.001f) ? (nearPlane + 100.0f) : newFar;
    projDirty = true;
}

// ---------------------------------------------------------
// Transform
// ---------------------------------------------------------
void ModuleCamera::setPosition(const Vector3& p)
{
    position = p;
    viewDirty = true;
}

void ModuleCamera::setOrientation(const Quaternion& q)
{
    // OJO: si quieres que yaw/pitch sean la única fuente de verdad,
    // este setter NO debería usarse a lo loco.
    orientation = q;
    orientation.Normalize();
    viewDirty = true;
}

void ModuleCamera::lookAt(const Vector3& target, const Vector3& /*worldUp*/)
{
    Vector3 dir = (target - position);
    if (dir.LengthSquared() < 0.00001f)
        return;

    dir.Normalize();

    // DirectX / SimpleMath:
    // forward view-space = (0,0,-1)
    // Queremos yaw/pitch tal que front() apunte hacia dir.
    yawRad = atan2f(dir.x, -dir.z);
    pitchRad = asinf(clampf(dir.y, -1.0f, 1.0f));

    // Clamp pitch
    if (clampPitch)
    {
        const float maxPitch = XMConvertToRadians(kMaxPitchDeg);
        pitchRad = clampf(pitchRad, -maxPitch, +maxPitch);
    }

    // Reconstruir orientación DESDE yaw/pitch (única fuente de verdad)
    Quaternion qYaw = Quaternion::CreateFromAxisAngle(Vector3::Up, yawRad);
    Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, pitchRad);

    orientation = qPitch * qYaw;
    orientation.Normalize();

    viewDirty = true;
}

// ---------------------------------------------------------
// Basis vectors (CONVENCIÓN CORRECTA)
// ---------------------------------------------------------
Vector3 ModuleCamera::front() const
{
    // Forward DirectX: -Z
    return Vector3::Transform(Vector3(0, 0, -1), orientation);
}

Vector3 ModuleCamera::up() const
{
    return Vector3::Transform(Vector3(0, 1, 0), orientation);
}

Vector3 ModuleCamera::right() const
{
    return Vector3::Transform(Vector3(1, 0, 0), orientation);
}

// ---------------------------------------------------------
// Input: Keyboard (Unity-ish WASD + QE, speed with SHIFT)
// ---------------------------------------------------------
void ModuleCamera::handleKeyboard(float dt)
{
    Keyboard& keyboard = Keyboard::Get();
    const Keyboard::State& ks = keyboard.GetState();

    Mouse& mouse = Mouse::Get();
    const Mouse::State& ms = mouse.GetState();

    if (requireRMBToMove && !ms.rightButton)
        return;

    Vector3 move = Vector3::Zero;

    // vertical absoluto (Q/E)
    if (ks.Q) move.y += 1.0f;
    if (ks.E) move.y -= 1.0f;

    // local (forward is -Z)
    if (ks.W) move.z -= 1.0f;
    if (ks.S) move.z += 1.0f;
    if (ks.A) move.x -= 1.0f;
    if (ks.D) move.x += 1.0f;

    if (move.LengthSquared() < 1e-6f)
        return;

    move.Normalize();

    Vector3 worldMove = Vector3::Zero;
    worldMove += right() * move.x;
    worldMove += front() * (-move.z); // -move.z porque W hace move.z = -1
    worldMove.y += move.y;

    float speed = moveSpeed;
    if (ks.LeftShift || ks.RightShift)
        speed *= shiftMultiplier;

    position += worldMove * speed * dt;
    viewDirty = true;
}

// ---------------------------------------------------------
// Input: Arrow rotation (Pitch/Yaw)
// ---------------------------------------------------------
void ModuleCamera::handleArrowRotation(float dt)
{
    Keyboard& keyboard = Keyboard::Get();
    const Keyboard::State& ks = keyboard.GetState();

    Vector2 rot = Vector2::Zero;

    if (ks.Up)    rot.y += 1.0f;
    if (ks.Down)  rot.y -= 1.0f;
    if (ks.Left)  rot.x -= 1.0f;
    if (ks.Right) rot.x += 1.0f;

    if (rot.LengthSquared() < 1e-6f)
        return;

    const float rotSpeedRad = XMConvertToRadians(rotateSpeedDeg) * dt;

    yawRad += rot.x * rotSpeedRad;
    pitchRad += rot.y * rotSpeedRad;

    if (clampPitch)
    {
        const float maxPitch = XMConvertToRadians(kMaxPitchDeg);
        pitchRad = clampf(pitchRad, -maxPitch, +maxPitch);
    }

    Quaternion qYaw = Quaternion::CreateFromAxisAngle(Vector3::Up, yawRad);
    Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, pitchRad);

    orientation = qPitch * qYaw;
    orientation.Normalize();

    viewDirty = true;
}

// ---------------------------------------------------------
// Input: Mouse (RMB rotate like Unity)  <-- FIX LATIGAZO AQUÍ
// ---------------------------------------------------------
void ModuleCamera::handleMouse(float /*dt*/)
{
    Mouse& mouse = Mouse::Get();
    const Mouse::State& ms = mouse.GetState();

    // --- Prev mouse init ---
    if (!hasPrevMouse)
    {
        prevMouseX = ms.x;
        prevMouseY = ms.y;
        hasPrevMouse = true;
    }

    // --- Wheel delta (accumulative in DXTK) ---
    if (!hasPrevWheel)
    {
        prevWheel = ms.scrollWheelValue;
        hasPrevWheel = true;
    }
    const int wheelDelta = ms.scrollWheelValue - prevWheel;
    prevWheel = ms.scrollWheelValue;

    // --- Read keyboard alt state ---
    Keyboard& keyboard = Keyboard::Get();
    const Keyboard::State& ks = keyboard.GetState();

    const bool altDown = (ks.LeftAlt || ks.RightAlt);

    // Mouse delta (solo si estamos en algún modo que lo use)
    const int dx = ms.x - prevMouseX;
    const int dy = ms.y - prevMouseY;

    // Actualiza prev SIEMPRE al final (para que nunca haya latigazo)
    prevMouseX = ms.x;
    prevMouseY = ms.y;

    // ---------------------------------------------------------
    // 1) ALT + LMB = ORBIT (Unity-like)
    // ---------------------------------------------------------
    if (orbitEnabled && altDown && ms.leftButton)
    {
        const float yawDeltaRad = XMConvertToRadians(kMouseSensitivity * float(dx));
        const float pitchDeltaRad = XMConvertToRadians(kMouseSensitivity * float(dy));

        yawRad += yawDeltaRad;
        pitchRad += pitchDeltaRad;

        if (clampPitch)
        {
            const float maxPitch = XMConvertToRadians(kMaxPitchDeg);
            pitchRad = clampf(pitchRad, -maxPitch, +maxPitch);
        }

        Quaternion qYaw = Quaternion::CreateFromAxisAngle(Vector3::Up, yawRad);
        Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, pitchRad);

        orientation = qPitch * qYaw;
        orientation.Normalize();

        // mantener distancia al pivot
        position = orbitPivot - front() * orbitDistance;

        viewDirty = true;
    }

    // --- Mouse wheel: dolly (acercar/alejar al pivote) ---
    static int prevWheel = 0;
    if (!hasPrevWheel)
    {
        prevWheel = ms.scrollWheelValue;
        hasPrevWheel = true;
    }

    int wheelDelta = ms.scrollWheelValue - prevWheel;
    prevWheel = ms.scrollWheelValue;

    if (wheelDelta != 0)
    {
        // DirectXTK suele dar 120 por “tick”
        const float ticks = float(wheelDelta) / 120.0f;

        // Ajusta sensibilidad a gusto
        const float zoomFactorPerTick = 0.9f; // < 1 = más rápido acercando
        float factor = powf(zoomFactorPerTick, ticks);

        orbitDistance *= factor;

        // Clamps para que no atraviese el pivote ni se vaya infinito
        orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);

        // Mantén la cámara mirando al pivote
        position = orbitPivot - front() * orbitDistance;
        viewDirty = true;
    }


    // ---------------------------------------------------------
    // 3) RMB = FREE LOOK (tu comportamiento actual)
    // ---------------------------------------------------------
    if (!ms.rightButton)
    {
        wasRightMouseDown = false;
        return;
    }

    // flanco RMB: engancha y no rotar este frame (evita latigazo)
    if (!wasRightMouseDown)
    {
        wasRightMouseDown = true;
        return;
    }

    // RMB mantenido
    if (dx == 0 && dy == 0) return;

    const float yawDeltaRad = XMConvertToRadians(kMouseSensitivity * float(dx));
    const float pitchDeltaRad = XMConvertToRadians(kMouseSensitivity * float(dy));

    yawRad += yawDeltaRad;
    pitchRad += pitchDeltaRad;

    if (clampPitch)
    {
        const float maxPitch = XMConvertToRadians(kMaxPitchDeg);
        pitchRad = clampf(pitchRad, -maxPitch, +maxPitch);
    }

    Quaternion qYaw = Quaternion::CreateFromAxisAngle(Vector3::Up, yawRad);
    Quaternion qPitch = Quaternion::CreateFromAxisAngle(Vector3::Right, pitchRad);

    orientation = qPitch * qYaw;
    orientation.Normalize();

    viewDirty = true;
}


// ---------------------------------------------------------
// Recalc matrices only when needed
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

    const Vector3 f = front();
    const Vector3 u = up();

    Matrix world = Matrix::CreateWorld(position, f, u);
    view = world.Invert();

    viewDirty = false;
}

// ---------------------------------------------------------
// Frustum helpers
// ---------------------------------------------------------
BoundingFrustum ModuleCamera::getFrustum() const
{
    if (projDirty) const_cast<ModuleCamera*>(this)->recalcProjectionIfNeeded();
    if (viewDirty) const_cast<ModuleCamera*>(this)->recalcViewIfNeeded();

    BoundingFrustum fr;
    BoundingFrustum::CreateFromMatrix(fr, proj, true);
    fr.Origin = position;
    fr.Orientation = orientation;
    return fr;
}

void ModuleCamera::getFrustumPlanes(Vector4 planesOut[6], bool normalize) const
{
    if (projDirty) const_cast<ModuleCamera*>(this)->recalcProjectionIfNeeded();
    if (viewDirty) const_cast<ModuleCamera*>(this)->recalcViewIfNeeded();

    Matrix vp = view * proj;

    // En tu proyecto: esto viene de MathUtils (tipo getPlanes del profe)
    ExtractFrustumPlanes(planesOut, vp, normalize);
}

void ModuleCamera::setFocusBounds(const Vector3& center, float radius)
{
    focusCenter = center;
    focusRadius = (radius > 0.01f) ? radius : 0.01f;

    orbitPivot = focusCenter;
}


void ModuleCamera::focus()
{
    // 1) Pivote = centro del objeto
    orbitPivot = focusCenter;

    // 2) Dirección actual desde el pivote hacia la cámara
    Vector3 dir = position - orbitPivot;

    if (dir.LengthSquared() < 1e-6f)
        dir = Vector3(0, 0, 1); // fallback seguro

    dir.Normalize();

    // 3) Calcula distancia necesaria según FOV vertical
    const float halfV = 0.5f * vFovRad;
    float dist = focusRadius / tanf(halfV);

    // Margen visual (como Unity)
    orbitDistance = dist * 1.25f;

    orbitDistance = clampf(orbitDistance, 0.2f, 500.0f);

    // 4) Reposiciona cámara manteniendo dirección
    position = orbitPivot + dir * orbitDistance;

    // 5) Mira al pivote
    lookAt(orbitPivot);

    viewDirty = true;
}


// ---------------------------------------------------------
// Utils
// ---------------------------------------------------------
float ModuleCamera::computeVerticalFovFromHorizontal(float hFov, float aspectRatio)
{
    const float halfH = 0.5f * hFov;
    const float t = tanf(halfH);
    const float halfV = atanf(t / aspectRatio);
    return 2.0f * halfV;
}

float ModuleCamera::clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
