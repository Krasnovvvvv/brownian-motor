#ifndef BROWNIAN_MOTOR_DICHOTOMICTRANSITION_H
#define BROWNIAN_MOTOR_DICHOTOMICTRANSITION_H
#pragma once

#include <cmath>
#include <stdexcept>

#include "DichotomicParams.h"

struct DichotomicTransition final {
    double p_aa{};
    double p_bb{};
    double p_a_stationary{};

    [[nodiscard]] static DichotomicTransition from(
        const DichotomicParams& params
    ) {
        params.validate();

        const double gamma = params.total_rate();
        const double decay = std::exp(-gamma * params.dt);

        DichotomicTransition transition{
            .p_aa = params.gamma_b / gamma +
                    (params.gamma_a / gamma) * decay,

            .p_bb = params.gamma_a / gamma +
                    (params.gamma_b / gamma) * decay,

            .p_a_stationary = params.gamma_b / gamma
        };

        transition.validate();
        return transition;
    }

    void validate() const {
        if (!std::isfinite(p_aa) || !std::isfinite(p_bb) ||
            !std::isfinite(p_a_stationary)) {
            throw std::invalid_argument(
                "dichotomic transition probabilities must be finite"
            );
            }

        if (p_aa < 0.0 || p_aa > 1.0 ||
            p_bb < 0.0 || p_bb > 1.0 ||
            p_a_stationary < 0.0 || p_a_stationary > 1.0) {
            throw std::invalid_argument(
                "dichotomic transition probabilities must lie in [0, 1]"
            );
            }
    }
};

#endif // BROWNIAN_MOTOR_DICHOTOMICTRANSITION_H