#ifndef BROWNIAN_MOTOR_TWOSTATEFACTORMODULATION_H
#define BROWNIAN_MOTOR_TWOSTATEFACTORMODULATION_H
#pragma once

#include "modulations/DichotomicModulation.h"
#include "modulations/ModulationSettings.h"

class TwoStateFactorModulation final {
public:
    explicit TwoStateFactorModulation(
        const ModulationSettings& settings,
        double dt,
        unsigned int seed
    )
        : process_{
        make_params_(settings, dt),
        seed
    }
    , factor_a_{settings.factor_a}
    , factor_b_{settings.factor_b}
    {}

    [[nodiscard]] double step() noexcept {
        (void)process_.step();

        return current_factor();
    }

    [[nodiscard]] double current_factor() const noexcept {
        return process_.current_sigma() >= 0.0
            ? factor_a_
            : factor_b_;
    }

private:
    [[nodiscard]] static DichotomicParams make_params_(
        const ModulationSettings& settings,
        double dt
    ) {
        settings.validate();

        DichotomicParams params{
            .sigma_a = 1.0,
            .sigma_b = -1.0,
            .gamma_a = settings.gamma_a,
            .gamma_b = settings.gamma_b,
            .dt = dt,
            .alpha = -1.0
        };

        params.validate();

        return params;
    }

    DichotomicModulation process_;

    double factor_a_;
    double factor_b_;
};

#endif