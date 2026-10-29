#ifndef BROWNIAN_MOTOR_MODULATIONSETTINGS_H
#define BROWNIAN_MOTOR_MODULATIONSETTINGS_H
#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>

enum class ModulationKind {
    LegacySymmetric,
    MarkovTwoState,
    PeriodicTwoState
};

struct ModulationSettings final {
    ModulationKind kind{
        ModulationKind::LegacySymmetric
    };

    // Actual multipliers of U(x).
    double factor_a{1.0};
    double factor_b{-1.0 / 3.0};

    // Continuous-time transition rates.
    double gamma_a{1.0};
    double gamma_b{1.0};

    // Deterministic dwell times measured in solver steps.
    std::size_t steps_a{150};
    std::size_t steps_b{150};

    void validate() const {
        switch (kind) {
            case ModulationKind::LegacySymmetric:
                return;

            case ModulationKind::MarkovTwoState:
                validate_factors_();

                if (
                    !std::isfinite(gamma_a) ||
                    !std::isfinite(gamma_b) ||
                    gamma_a <= 0.0 ||
                    gamma_b <= 0.0 ||
                    !std::isfinite(gamma_a + gamma_b)
                ) {
                    throw std::invalid_argument(
                        "Transition rates and their sum "
                        "must be finite and positive"
                    );
                }

                return;

            case ModulationKind::PeriodicTwoState:
                validate_factors_();

                if (steps_a == 0 || steps_b == 0) {
                    throw std::invalid_argument(
                        "Periodic dwell steps must be positive"
                    );
                }

                return;
        }

        throw std::invalid_argument(
            "Unknown modulation kind"
        );
    }

private:
    void validate_factors_() const {
        if (
            !std::isfinite(factor_a) ||
            !std::isfinite(factor_b)
        ) {
            throw std::invalid_argument(
                "Modulation factors must be finite"
            );
        }
    }
};

#endif