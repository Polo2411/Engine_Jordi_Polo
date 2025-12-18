#pragma once

#include "Globals.h"
#include "Module.h"
#include "ModuleSamplers.h" // ModuleSamplers::Type

class UIModule : public Module
{
public:
    UIModule() = default;
    ~UIModule() override = default;

    void preRender() override;
    void render() override {}

    // --- Assignment UI state getters (Assignment1Module will read these) ---
    bool getShowGrid() const { return showGrid; }
    bool getShowAxis() const { return showAxis; }
    ModuleSamplers::Type getSelectedSampler() const { return selectedSampler; }

private:
    bool showGrid = true;
    bool showAxis = true;

    // Only 4 options required by the assignment
    ModuleSamplers::Type selectedSampler = ModuleSamplers::Type::Linear_Wrap;
};
