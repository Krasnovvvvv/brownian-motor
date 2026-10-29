#ifndef BROWNIAN_MOTOR_SINUSOIDALMODULATION_H
#define BROWNIAN_MOTOR_SINUSOIDALMODULATION_H
#pragma once

#include <cmath>
#include <stdexcept>

struct SinusoidalModulation {
    double amplitude{};
    double omega{};
    double dt{};
    double time{0.0};
    double factor{0.0};
    double offset{0.0};
    double phase{0.0};

    explicit SinusoidalModulation(
        double amplitude,
        double omega,
        double dt,
        double initial_time = 0.0,
        double offset = 0.0,
        double phase = 0.0
    ) : amplitude{amplitude}, omega{omega}, dt{dt}, time{initial_time},
        offset{offset}, phase{phase} {
        if (!std::isfinite(amplitude) || !std::isfinite(omega) ||
            !std::isfinite(dt) || dt <= 0.0 ||
            !std::isfinite(initial_time) || !std::isfinite(offset) ||
            !std::isfinite(phase) ||
            !std::isfinite(offset + std::abs(amplitude)) ||
            !std::isfinite(offset - std::abs(amplitude)) ||
            !std::isfinite(omega * initial_time + phase)) {
            throw std::invalid_argument("Invalid sinusoidal modulation parameters");
            }
        factor = offset + amplitude * std::sin(omega * time + phase);
    }

    [[nodiscard]] double step() noexcept {
        factor = offset + amplitude * std::sin(omega * time + phase);
        time += dt;
        return factor;
    }

    [[nodiscard]] double current_factor() const noexcept {
        return factor;
    }
};
#endif