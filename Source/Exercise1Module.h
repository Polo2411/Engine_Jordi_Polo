#pragma once

#include "Module.h"

// Exercise 1: clears the screen and renders ImGui on the backbuffer
class Exercise1Module : public Module
{
public:
    bool init() override { return true; }
    void render() override;
};
