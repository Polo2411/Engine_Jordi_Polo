#include "Globals.h"
#include "Application.h"
#include "UIModule.h"
#include "TimeManager.h"
#include "imgui.h"

void UIModule::preRender()
{
    ImGuiIO& io = ImGui::GetIO();

    // ---------- ABOUT WINDOW (arriba izquierda, movible, con barra y X) ----------
    if (showAbout)
    {
        // Solo la primera vez que se crea esta ventana
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(550, 0), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags aboutFlags = ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("About This Engine", &showAbout, aboutFlags))
        {
            ImGui::Text("Master AAA Engine");
            ImGui::Separator();
            ImGui::TextWrapped("Educational project for the AAA master's degree.");
            ImGui::Text("Author: Jordi Polo Tormo");
            ImGui::Separator();
            ImGui::Text("License: UPC School Engine");
            ImGui::Separator();
            ImGui::Text("Libraries:");
            ImGui::BulletText("DirectX 12");
            ImGui::BulletText("Dear ImGui %s", IMGUI_VERSION);
            ImGui::BulletText("DirectX Tool Kit");
        }
        ImGui::End();
    }

    // ---------- TIME & FPS WINDOW (arriba derecha, movible, con barra y X) ----------
    if (!showTimeStats)
        return;

    TimeManager* time = app->getTimeManager();
    if (!time)
        return;

    // Tamaño que queremos aproximadamente
    const float statsWidth = 360.0f;
    const float statsHeight = 320.0f;

    // Tamaño inicial (solo la primera vez que aparece)
    ImGui::SetNextWindowSize(ImVec2(statsWidth, statsHeight), ImGuiCond_FirstUseEver);

    // Posición inicial: esquina superior derecha (solo la primera vez)
    // x = ancho ventana - statsWidth - margen
    const float margin = 10.0f;
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - statsWidth - margin, margin),
        ImGuiCond_FirstUseEver
    );

    ImGui::SetNextWindowBgAlpha(0.85f);

    // MISMO ESTILO DE FLAGS QUE ABOUT: solo NoDocking
    ImGuiWindowFlags statsFlags = ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("Time & FPS Stats", &showTimeStats, statsFlags))
    {
        ImGui::Text("Real Time: %.3f s", time->getRealTimeSinceStartup());
        ImGui::Text("Game Time: %.3f s", time->getTime());
        ImGui::Separator();

        ImGui::Text("FPS: %.1f", time->getFPS());
        ImGui::Text("Frame Time: %.3f ms", time->getAvgFrameMs());

        ImGui::Separator();
        ImGui::Text("Delta Times:");
        ImGui::BulletText("Real dt: %.3f ms", time->getRealDeltaTime() * 1000.0f);
        ImGui::BulletText("Game dt: %.3f ms", time->getDeltaTime() * 1000.0f);

        ImGui::Separator();
        ImGui::Text("Frame Time History:");
        ImGui::PlotLines(
            "##FrameHistory",
            time->getFrameMsHistory(),
            (int)TimeManager::kHistorySize,
            0,
            nullptr,
            0.0f,
            40.0f,
            ImVec2(0, 80)
        );

        ImGui::Separator();

        // ------ GAME CLOCK CONTROLS ------
        bool paused = time->isPaused();
        if (ImGui::Checkbox("Paused", &paused))
        {
            time->setPaused(paused);
            OutputDebugStringA(paused ? "TimeManager: Paused ON\n" : "TimeManager: Paused OFF\n");
        }

        float scale = time->getTimeScale();
        if (ImGui::SliderFloat("Time Scale", &scale, 0.0f, 4.0f))
        {
            time->setTimeScale(scale);
            OutputDebugStringA("TimeManager: TimeScale changed\n");
        }
    }
    ImGui::End();
}
