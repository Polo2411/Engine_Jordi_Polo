#include "Globals.h"
#include "Application.h"
#include "ModuleInput.h"
#include "D3D12Module.h"
#include "UIModule.h"
#include "Exercise1Module.h"
#include "Exercise2Module.h"
#include "Exercise3Module.h"
#include "Exercise4Module.h"
#include "Assignment1Module.h"
#include "TimeManager.h"
#include "ModuleResources.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplers.h"

Application::Application(int argc, wchar_t** argv, void* hWnd)
{
    app = this;

    modules.push_back(new ModuleInput((HWND)hWnd));
    modules.push_back(d3d12 = new D3D12Module((HWND)hWnd));
    modules.push_back(timeManager = new TimeManager());
    modules.push_back(ui = new UIModule());

    // ModuleResources ES un Module, así que puede ir en el vector
    modules.push_back(resources = new ModuleResources());
    modules.push_back(camera = new ModuleCamera());
    modules.push_back(samplers = new ModuleSamplers());
    modules.push_back(shaderDescriptors = new ModuleShaderDescriptors());

    // Exercices
    //modules.push_back(new Exercise1Module());
    //modules.push_back(new Exercise2Module());
    //modules.push_back(new Exercise3Module());
    //modules.push_back(new Exercise4Module());
    modules.push_back(new Assignment1Module);

}

Application::~Application()
{
    // 1) Avisar a los módulos para que suelten recursos de GPU, etc.
    cleanUp();

    // 2) Destruir TODOS los módulos exactamente una vez
    for (auto it = modules.rbegin(); it != modules.rend(); ++it)
    {
        delete* it;
    }
    modules.clear();

    // 3) No borrar resources ni d3d12 aquí: ya se han borrado en el bucle anterior.
    resources = nullptr;
    d3d12 = nullptr;
    shaderDescriptors = nullptr;

    app = nullptr;
}


bool Application::init()
{
    bool ret = true;

    // 1) inicializar módulos (incluye D3D12Module)
    for (auto it = modules.begin(); it != modules.end() && ret; ++it)
        ret = (*it)->init();

    // 2) inicializar ModuleResources después de que D3D12 esté listo
    if (ret && resources)
        ret = resources->init();    // 👈 aquí dentro cogerá device + queue de d3d12

    lastTime = std::chrono::steady_clock::now();


    return ret;
}

void Application::update()
{
    using namespace std::chrono_literals;

    if (updating) return;
    updating = true;

    // timing
    auto now = std::chrono::steady_clock::now();
    elapsedMilis = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
    lastTime = now;

    const uint64_t maxFrameMs = 250; // 0.25s
    if (elapsedMilis > maxFrameMs) elapsedMilis = maxFrameMs;

    tickSum -= tickList[tickIndex];
    tickSum += elapsedMilis;
    tickList[tickIndex] = elapsedMilis;
    tickIndex = (tickIndex + 1) % MAX_FPS_TICKS;

    if (!paused)
    {
        for (auto& m : modules) m->update();

        // 1) preRender: D3D12 primero (arranca ImGui frame)
        if (d3d12) d3d12->preRender();
        for (auto& m : modules)
            if (m != d3d12) m->preRender();

        // 2) render: ejercicios / UI primero, D3D12 no dibuja
        for (auto& m : modules)
            if (m != d3d12) m->render();

        // 3) postRender: primero módulos, luego D3D12 (present)
        for (auto& m : modules)
            if (m != d3d12) m->postRender();
        if (d3d12) d3d12->postRender();
    }

    updating = false;
}

bool Application::cleanUp()
{
    // flush de la cola de dibujo antes de liberar nada
    if (d3d12)
        d3d12->flush();

    bool ret = true;
    for (auto it = modules.rbegin(); it != modules.rend() && ret; ++it)
    {
        ret = (*it)->cleanUp();
    }

    return ret;
}


