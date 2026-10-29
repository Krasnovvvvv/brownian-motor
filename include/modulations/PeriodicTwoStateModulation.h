#ifndef BROWNIAN_MOTOR_PERIODICTWOSTATEMODULATION_H
#define BROWNIAN_MOTOR_PERIODICTWOSTATEMODULATION_H
#pragma once

#include <cstddef>

#include "modulations/ModulationSettings.h"

class PeriodicTwoStateModulation final {
public:
    explicit PeriodicTwoStateModulation(
        const ModulationSettings& settings
    )
        : factor_a_{settings.factor_a}
    , factor_b_{settings.factor_b}
    , steps_a_{settings.steps_a}
    , steps_b_{settings.steps_b}
    {
        settings.validate();
    }

    /*
     * First steps_a calls return factor_a.
     * Next steps_b calls return factor_b.
     * Then the cycle repeats.
     */
    [[nodiscard]] double step() noexcept {
        const double result =
            state_is_a_ ? factor_a_ : factor_b_;

        ++steps_in_state_;

        const std::size_t dwell_steps =
            state_is_a_ ? steps_a_ : steps_b_;

        if (steps_in_state_ == dwell_steps) {
            steps_in_state_ = 0;
            state_is_a_ = !state_is_a_;
        }

        return result;
    }

private:
    double factor_a_;
    double factor_b_;

    std::size_t steps_a_;
    std::size_t steps_b_;

    std::size_t steps_in_state_{0};

    bool state_is_a_{true};
};

#endif