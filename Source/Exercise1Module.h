#pragma once
#include "Module.h"

class Exercise1Module : public Module
{
public:
    bool init() override { return true; }
    void render() override;
};
