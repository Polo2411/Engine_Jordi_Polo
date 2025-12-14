#pragma once

#include "Module.h"
#include "Timer.h"

#include <array>
#include <cstdint>

class TimeManager : public Module
{
public:
    TimeManager() = default;
    ~TimeManager() override = default;

    bool init() override;
    void update() override;
    bool cleanUp() override { return true; }

    // ---- API de "Game Time" (tipo Unity::Time) ----
    uint64_t getFrameCount() const { return frameCount; }

    // Tiempo de juego (afectado por timeScale / pausa)
    float getTime() const { return static_cast<float>(gameTimeSeconds); }
    float getDeltaTime() const { return static_cast<float>(gameDeltaTimeSeconds); }
    float getTimeScale() const { return static_cast<float>(timeScale); }

    // Tiempo real (no afectado por timeScale)
    float getRealTimeSinceStartup() const { return static_cast<float>(realTimeSinceStartupSeconds); }
    float getRealDeltaTime() const { return static_cast<float>(realDeltaTimeSeconds); }

    // FPS y ms
    float getFPS() const { return fps; }
    float getAvgFrameMs() const { return avgFrameMs; }

    // Control del tiempo de juego
    void  setTimeScale(float scale);
    void  setPaused(bool p);
    void  togglePaused();

    bool  isPaused() const { return paused; }

    // Datos para los gráficos en ImGui
    static constexpr size_t kHistorySize = 120;
    const float* getFrameMsHistory() const { return frameMsHistory.data(); }

private:
    Timer  appTimer;       // reloj desde que arranca el engine
    bool   firstFrame = true;

    // Reloj real
    double realTimeSinceStartupSeconds = 0.0;
    double realDeltaTimeSeconds = 0.0;

    // Reloj de juego
    double timeScale = 1.0;
    bool   paused = false;
    double gameTimeSeconds = 0.0;
    double gameDeltaTimeSeconds = 0.0;

    // Contadores
    uint64_t frameCount = 0;

    // Historial para gráficas
    std::array<float, kHistorySize> frameMsHistory{};
    size_t historyIndex = 0;
    float  fps = 0.0f;
    float  avgFrameMs = 0.0f;
};
