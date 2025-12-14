#pragma once
#include "Globals.h"
#include "Module.h"
#include "imgui.h"

class UIModule : public Module
{
public:
    UIModule() = default;
    ~UIModule() override = default;

    void preRender() override;
    void render() override {}

    int getSelectedExercise() const { return selectedExercise; }

private:
    int  selectedExercise = 1;
    bool showAbout = true;
    bool showTimeStats = true;
};
