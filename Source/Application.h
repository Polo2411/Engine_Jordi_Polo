#pragma once

#include "Globals.h"
#include <array>
#include <vector>
#include <chrono>

class Module;
class D3D12Module;
class UIModule;
class TimeManager;
class ModuleResources;
class ModuleCamera;

class Application
{
public:
    Application(int argc, wchar_t** argv, void* hWnd);
    ~Application();

    bool init();
    void update();
    bool cleanUp();

    bool isPaused() const { return paused; }
    bool setPaused(bool p) { paused = p; return paused; }

    D3D12Module* getD3D12Module()   const { return d3d12; }
    UIModule* getUIModule()      const { return ui; }
    TimeManager* getTimeManager()   const { return timeManager; }
    ModuleResources* getResources()   const { return resources; }
    uint64_t getElapsedMilis() const { return elapsedMilis; }     // ms del último frame
    float    getDeltaTimeSeconds() const { return float(elapsedMilis) * 0.001f; } // dt en segundos
    ModuleCamera* getCamera() const { return camera; }

private:
    enum { MAX_FPS_TICKS = 30 };
    using TickList = std::array<uint64_t, MAX_FPS_TICKS>;

    std::vector<Module*> modules;
    D3D12Module* d3d12 = nullptr;
    UIModule* ui = nullptr;
    TimeManager* timeManager = nullptr;
    ModuleResources* resources = nullptr;
    ModuleCamera* camera = nullptr;

    std::chrono::steady_clock::time_point lastTime;
    TickList  tickList = {};
    uint64_t  tickIndex = 0;
    uint64_t  tickSum = 0;
    uint64_t  elapsedMilis = 0;
    bool      paused = false;
    bool      updating = false;
};

extern Application* app;
