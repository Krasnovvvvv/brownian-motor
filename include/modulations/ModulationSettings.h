#ifndef BROWNIAN_MOTOR_MODULATIONSETTINGS_H
#define BROWNIAN_MOTOR_MODULATIONSETTINGS_H
#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>

enum class ModulationKind {
    LegacySymmetric = 0,
    MarkovTwoState = 1,
    PeriodicTwoState = 2,
    Constant = 3,
    Sinusoidal = 4
};

struct ModulationSettings final {
    ModulationKind kind{ModulationKind::LegacySymmetric};
    double factor_a{1.0};
    double factor_b{-1.0 / 3.0};
    double gamma_a{1.0};
    double gamma_b{1.0};
    std::size_t steps_a{150};
    std::size_t steps_b{150};

    double constant_factor{1.0};
    double sinusoidal_offset{0.0};
    double sinusoidal_amplitude{1.0};
    double sinusoidal_period{1.0};
    double sinusoidal_phase{0.0}; // radians

    void validate() const {
        switch (kind) {
        case ModulationKind::LegacySymmetric:
            return;
        case ModulationKind::MarkovTwoState:
            validate_factors_();
            if (!std::isfinite(gamma_a) || !std::isfinite(gamma_b) ||
                gamma_a <= 0.0 || gamma_b <= 0.0 ||
                !std::isfinite(gamma_a + gamma_b)) {
                throw std::invalid_argument(
                    "Transition rates and their sum must be finite and positive"
                );
            }
            return;
        case ModulationKind::PeriodicTwoState:
            validate_factors_();
            if (steps_a == 0 || steps_b == 0) {
                throw std::invalid_argument("Dwell steps must be positive");
            }
            return;
        case ModulationKind::Constant:
            if (!std::isfinite(constant_factor)) {
                throw std::invalid_argument("Constant factor must be finite");
            }
            return;
        case ModulationKind::Sinusoidal:
            if (!std::isfinite(sinusoidal_offset) ||
                !std::isfinite(sinusoidal_amplitude) ||
                sinusoidal_amplitude < 0.0 ||
                !std::isfinite(sinusoidal_period) || sinusoidal_period <= 0.0 ||
                !std::isfinite(sinusoidal_phase) ||
                !std::isfinite(sinusoidal_offset + sinusoidal_amplitude) ||
                !std::isfinite(sinusoidal_offset - sinusoidal_amplitude) ||
                !std::isfinite(2.0 * std::numbers::pi_v<double> / sinusoidal_period)) {
                throw std::invalid_argument("Invalid sinusoidal modulation parameters");
            }
            return;
        }
        throw std::invalid_argument("Unknown modulation kind");
    }

private:
    void validate_factors_() const {
        if (!std::isfinite(factor_a) || !std::isfinite(factor_b)) {
            throw std::invalid_argument("Modulation factors must be finite");
        }
    }
};
#endif
