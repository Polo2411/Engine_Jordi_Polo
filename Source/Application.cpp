#include "Globals.h"
#include "Application.h"

#include "ModuleInput.h"
#include "D3D12Module.h"
#include "UIModule.h"

#include "Exercise1Module.h"
#include "Exercise2Module.h"
#include "Exercise3Module.h"
#include "Exercise4Module.h"
#include "Exercise5Module.h" // Added: Exercise 5
#include "Assignment1Module.h"

#include "TimeManager.h"
#include "ModuleResources.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplers.h"

Application::Application(int argc, wchar_t** argv, void* hWnd)
{
    app = this;

    // Core engine modules (order matters)
    modules.push_back(new ModuleInput((HWND)hWnd));
    modules.push_back(d3d12 = new D3D12Module((HWND)hWnd));
    modules.push_back(timeManager = new TimeManager());
    modules.push_back(ui = new UIModule());

    // Rendering-related helpers
    modules.push_back(resources = new ModuleResources());
    modules.push_back(camera = new ModuleCamera());
    modules.push_back(samplers = new ModuleSamplers());
    modules.push_back(shaderDescriptors = new ModuleShaderDescriptors());

    // Exercises / assignments
    //modules.push_back(new Exercise1Module());
    //modules.push_back(new Exercise2Module());
    //modules.push_back(new Exercise3Module());
    //modules.push_back(new Exercise4Module());
    modules.push_back(new Exercise5Module()); // Added: run Exercise 5
    //modules.push_back(new Assignment1Module());
}

Application::~Application()
{
    // 1) Notify modules to release GPU/CPU resources
    cleanUp();

    // 2) Destroy modules in reverse order
    for (auto it = modules.rbegin(); it != modules.rend(); ++it)
    {
        delete* it;
    }
    modules.clear();

    resources = nullptr;
    d3d12 = nullptr;
    shaderDescriptors = nullptr;

    app = nullptr;
}

bool Application::init()
{
    bool ret = true;

    // Initialize all modules in order
    for (auto it = modules.begin(); it != modules.end() && ret; ++it)
        ret = (*it)->init();

    // Note: ModuleResources is already part of 'modules', do not init twice.

    lastTime = std::chrono::steady_clock::now();
    return ret;
}

void Application::update()
{
    if (updating) return;
    updating = true;

    // Frame timing
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> dt = now - lastTime;
    lastTime = now;

    elapsedSeconds = dt.count();

    const double maxFrameS = 0.25;
    if (elapsedSeconds > maxFrameS) elapsedSeconds = maxFrameS;

    // FPS history (moving average)
    tickSum -= tickList[tickIndex];
    tickSum += elapsedSeconds;
    tickList[tickIndex] = elapsedSeconds;
    tickIndex = (tickIndex + 1) % MAX_FPS_TICKS;

    if (!paused)
    {
        // Update phase
        for (auto& m : modules) m->update();

        // Pre-render phase (D3D12 first)
        if (d3d12) d3d12->preRender();
        for (auto& m : modules)
            if (m != d3d12) m->preRender();

        // Render phase
        for (auto& m : modules)
            if (m != d3d12) m->render();

        // Post-render phase (D3D12 last)
        for (auto& m : modules)
            if (m != d3d12) m->postRender();
        if (d3d12) d3d12->postRender();
    }

    updating = false;
}

bool Application::cleanUp()
{
    // Ensure GPU work is finished before shutdown
    if (d3d12)
        d3d12->flush();

    bool ret = true;
    for (auto it = modules.rbegin(); it != modules.rend() && ret; ++it)
    {
        ret = (*it)->cleanUp();
    }

    return ret;
}
