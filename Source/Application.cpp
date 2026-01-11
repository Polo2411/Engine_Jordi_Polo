// Application.cpp
#include "Globals.h"
#include "Application.h"

#include "ModuleInput.h"
#include "D3D12Module.h"

#include "Exercise1Module.h"
#include "Exercise2Module.h"
#include "Exercise3Module.h"
#include "Exercise4Module.h"
#include "Exercise5Module.h"
#include "Exercise6Module.h"
#include "Exercise7Module.h"
#include "Assignment1Module.h"
#include "Assignment2Module.h"

#include "TimeManager.h"
#include "ModuleResources.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleTargetDescriptors.h"
#include "ModuleSamplers.h"
#include "ModuleRingBuffer.h"

Application::Application(int /*argc*/, wchar_t** /*argv*/, void* hWnd)
{
    app = this;

    modules.push_back(new ModuleInput((HWND)hWnd));
    modules.push_back(d3d12 = new D3D12Module((HWND)hWnd));
    modules.push_back(timeManager = new TimeManager());

    modules.push_back(targetDescriptors = new ModuleTargetDescriptors());

    modules.push_back(shaderDescriptors = new ModuleShaderDescriptors());
    modules.push_back(samplers = new ModuleSamplers());
    modules.push_back(ringBuffer = new ModuleRingBuffer());
    modules.push_back(resources = new ModuleResources());
    modules.push_back(camera = new ModuleCamera());

    modules.push_back(new Assignment2Module());
}

Application::~Application()
{
    cleanUp();

    for (auto it = modules.rbegin(); it != modules.rend(); ++it)
        delete* it;

    modules.clear();

    d3d12 = nullptr;
    ui = nullptr;
    timeManager = nullptr;
    resources = nullptr;
    camera = nullptr;
    shaderDescriptors = nullptr;
    targetDescriptors = nullptr;
    samplers = nullptr;
    ringBuffer = nullptr;

    app = nullptr;
}

bool Application::init()
{
    bool ret = true;

    for (auto it = modules.begin(); it != modules.end() && ret; ++it)
        ret = (*it)->init();

    if (ret && d3d12)
        d3d12->initImGui();

    lastTime = std::chrono::steady_clock::now();
    return ret;
}

double Application::getAvgElapsedMs() const
{
    const double denom = double(MAX_FPS_TICKS);
    const double avgS = (denom > 0.0) ? (tickSum / denom) : 0.0;
    return avgS * 1000.0;
}

double Application::getFPS() const
{
    const double avgMs = getAvgElapsedMs();
    if (avgMs <= 0.0001)
        return 0.0;
    return 1000.0 / avgMs;
}

void Application::update()
{
    if (updating) return;
    updating = true;

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> dt = now - lastTime;
    lastTime = now;

    elapsedSeconds = dt.count();

    const double maxFrameS = 0.25;
    if (elapsedSeconds > maxFrameS) elapsedSeconds = maxFrameS;

    tickSum -= tickList[tickIndex];
    tickSum += elapsedSeconds;
    tickList[tickIndex] = elapsedSeconds;
    tickIndex = (tickIndex + 1) % MAX_FPS_TICKS;

    if (!paused)
    {
        for (auto& m : modules) m->update();

        if (d3d12) d3d12->preRender();
        for (auto& m : modules)
            if (m != d3d12) m->preRender();

        for (auto& m : modules)
            if (m != d3d12) m->render();

        for (auto& m : modules)
            if (m != d3d12) m->postRender();
        if (d3d12) d3d12->postRender();
    }

    updating = false;
}

bool Application::cleanUp()
{
    if (d3d12)
        d3d12->flush();

    // CRITICAL: release ImGui shader table while ShaderDescriptors still exist
    if (d3d12)
        d3d12->shutdownImGui();

    bool ret = true;
    for (auto it = modules.rbegin(); it != modules.rend() && ret; ++it)
        ret = (*it)->cleanUp();

    return ret;
}
