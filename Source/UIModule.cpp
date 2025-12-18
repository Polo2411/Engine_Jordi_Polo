#include "Globals.h"
#include "UIModule.h"

#include "Application.h"
#include "TimeManager.h"

#include "imgui.h"

void UIModule::preRender()
{
    TimeManager* tm = app ? app->getTimeManager() : nullptr;

    // Single window with exactly what the assignment requires
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("Rendering Settings", nullptr, flags))
    {
        // a) FPS
        if (tm)
        {
            ImGui::Text("FPS: %.1f", tm->getFPS());
            ImGui::Text("Frame Time: %.2f ms", tm->getAvgFrameMs());
        }
        else
        {
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        }

        ImGui::Separator();

        // b) Options to show/hide the grid and the axis
        ImGui::Checkbox("Show Grid", &showGrid);
        ImGui::Checkbox("Show Axis", &showAxis);

        ImGui::Separator();

        // c) 4 required sampler modes
        static const char* modes[] =
        {
            "Wrap + Bilinear",
            "Clamp + Bilinear",
            "Wrap + Point",
            "Clamp + Point"
        };

        int mode = 0;
        switch (selectedSampler)
        {
        case ModuleSamplers::Type::Linear_Wrap:  mode = 0; break;
        case ModuleSamplers::Type::Linear_Clamp: mode = 1; break;
        case ModuleSamplers::Type::Point_Wrap:   mode = 2; break;
        case ModuleSamplers::Type::Point_Clamp:  mode = 3; break;
        default: mode = 0; break;
        }

        if (ImGui::Combo("Texture Sampling", &mode, modes, IM_ARRAYSIZE(modes)))
        {
            switch (mode)
            {
            case 0: selectedSampler = ModuleSamplers::Type::Linear_Wrap;  break;
            case 1: selectedSampler = ModuleSamplers::Type::Linear_Clamp; break;
            case 2: selectedSampler = ModuleSamplers::Type::Point_Wrap;   break;
            case 3: selectedSampler = ModuleSamplers::Type::Point_Clamp;  break;
            default: selectedSampler = ModuleSamplers::Type::Linear_Wrap; break;
            }
        }
    }
    ImGui::End();
}
