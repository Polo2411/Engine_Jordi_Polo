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
class ModuleShaderDescriptors;
class ModuleSamplers;

// Central application class that owns and drives all engine modules
class Application
{
public:
    Application(int argc, wchar_t** argv, void* hWnd);
    ~Application();

    // Application lifecycle
    bool init();
    void update();
    bool cleanUp();

    bool isPaused() const { return paused; }
    bool setPaused(bool p) { paused = p; return paused; }

    // Core module accessors
    D3D12Module* getD3D12Module()   const { return d3d12; }
    UIModule* getUIModule()         const { return ui; }
    TimeManager* getTimeManager()   const { return timeManager; }
    ModuleResources* getResources() const { return resources; }

    // Delta time in seconds (current frame)
    double getDeltaTimeSeconds() const { return elapsedSeconds; }

    // Legacy time accessor (milliseconds)
    uint64_t getElapsedMilis() const { return uint64_t(elapsedSeconds * 1000.0); }

    // Rendering-related helpers
    ModuleCamera* getCamera() const { return camera; }
    ModuleShaderDescriptors* getShaderDescriptors() const { return shaderDescriptors; }
    ModuleSamplers* getSamplers() const { return samplers; }

private:
    enum { MAX_FPS_TICKS = 30 };
    using TickList = std::array<double, MAX_FPS_TICKS>;

    // Owned modules (updated and rendered each frame)
    std::vector<Module*> modules;

    // Cached pointers to commonly used modules
    D3D12Module* d3d12 = nullptr;
    UIModule* ui = nullptr;
    TimeManager* timeManager = nullptr;
    ModuleResources* resources = nullptr;
    ModuleCamera* camera = nullptr;
    ModuleShaderDescriptors* shaderDescriptors = nullptr;
    ModuleSamplers* samplers = nullptr;

    // Timing state
    std::chrono::steady_clock::time_point lastTime;

    // FPS history (moving average)
    TickList  tickList = {};
    uint64_t  tickIndex = 0;
    double    tickSum = 0.0;

    double    elapsedSeconds = 0.0;

    bool      paused = false;
    bool      updating = false;
};

extern Application* app;
