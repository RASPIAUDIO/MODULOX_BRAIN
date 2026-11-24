#pragma once

#include "parameter.h"

class Mixer {
public:
    ModuleParameters controls(const char* name = "Mix") { return {name, parameters_}; }

    void update_parameters()
    {
        for (unsigned i = 0; i < 3; ++i) {
            gains_[i] = parameters_[i].get() * (0.01f / 3);
        }
    }

    float sum(float a, float b, float c) const
    {
        return a * gains_[0] + b * gains_[1] + c * gains_[2];
    }

private:
    Parameter parameters_[3] = {
        {"In 1", 0, 100, 1, 100, "%"},
        {"In 2", 0, 100, 1, 100, "%"},
        {"In 3", 0, 100, 1, 100, "%"},
    };
    float gains_[3]{};
};
