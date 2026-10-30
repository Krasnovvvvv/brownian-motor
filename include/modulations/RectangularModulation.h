#ifndef BROWNIAN_MOTOR_RECTANGULARMODULATION_H
#define BROWNIAN_MOTOR_RECTANGULARMODULATION_H
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "modulations/ModulationSettings.h"

class RectangularModulation final {
public:
    explicit RectangularModulation(
        const ModulationSettings& settings,
        double dt
    )
        : factor_a_{settings.factor_a}
        , factor_b_{settings.factor_b}
        , duration_a_{settings.rectangular_duration_a}
        , duration_b_{settings.rectangular_duration_b}
        , dt_{dt}
        , current_factor_{settings.factor_a}
    {
        settings.validate();

        if (settings.kind !=
            ModulationKind::RectangularPeriodic) {
            throw std::invalid_argument(
                "RectangularModulation requires "
                "RectangularPeriodic settings"
            );
        }

        if (!std::isfinite(dt) || dt <= 0.0) {
            throw std::invalid_argument(
                "Modulation dt must be finite and positive"
            );
        }

        period_ = duration_a_ + duration_b_;
    }

    /*
     * Samples the prescribed waveform at t_n = n * dt.
     *
     * A: [k * period, k * period + duration_a)
     * B: [k * period + duration_a, (k + 1) * period)
     */
    [[nodiscard]] double step() noexcept {
        const long double time =
            static_cast<long double>(step_index_) *
            dt_;

        long double phase =
            std::fmod(time, period_);

        /*
         * Snap values numerically indistinguishable
         * from switching boundaries.
         */
        const long double input_roundoff =
            8.0L *
            std::numeric_limits<double>::epsilon() *
            period_;

        const long double time_roundoff =
            8.0L *
            std::numeric_limits<long double>::epsilon() *
            std::abs(time);

        const long double tolerance =
            std::min(
                input_roundoff + time_roundoff,
                1.0e-6L *
                    std::min(duration_a_, duration_b_)
            );

        if (
            phase <= tolerance ||
            period_ - phase <= tolerance
        ) {
            phase = 0.0L;
        } else if (
            std::abs(phase - duration_a_) <= tolerance
        ) {
            phase = duration_a_;
        }

        current_factor_ =
            phase < duration_a_
                ? factor_a_
                : factor_b_;

        ++step_index_;

        return current_factor_;
    }

    [[nodiscard]] double current_factor() const noexcept {
        return current_factor_;
    }

private:
    double factor_a_;
    double factor_b_;

    long double duration_a_;
    long double duration_b_;
    long double dt_;
    long double period_{};

    std::uint64_t step_index_{0};

    double current_factor_;
};

#endif