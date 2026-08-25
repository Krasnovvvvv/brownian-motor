#ifndef BROWNIAN_MOTOR_SINUSOIDALMODULATION_H
#define BROWNIAN_MOTOR_SINUSOIDALMODULATION_H
#pragma once

#include <cmath>

struct SinusoidalModulation {
    double amplitude{};
    double omega{};
    double dt{};
    double time{0.0};
    double factor{0.0};

    explicit SinusoidalModulation(double amplitude,
                                  double omega,
                                  double dt,
                                  double initial_time = 0.0)
    : amplitude{amplitude}
    , omega{omega}
    , dt{dt}
    , time{initial_time}
    , factor{amplitude * std::sin(omega * initial_time)}
    {}

    double step() noexcept {
        factor = amplitude * std::sin(omega * time);
        time += dt;
        return factor;
    }

    [[nodiscard]] double current_factor() const noexcept {
        return factor;
    }
};
#endif //BROWNIAN_MOTOR_SINUSOIDALMODULATION_H