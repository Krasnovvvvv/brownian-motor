#ifndef BROWNIAN_MOTOR_DICHOTOMICMODULATION_H
#define BROWNIAN_MOTOR_DICHOTOMICMODULATION_H
#pragma once

#include <random>
#include <utility>

#include "DichotomicParams.h"
#include "DichotomicTransition.h"

class DichotomicModulation final {
public:
    explicit DichotomicModulation(
        DichotomicParams params,
        unsigned int seed = std::random_device{}()
    )
        : params_{validated_(std::move(params))}
    , transition_{DichotomicTransition::from(params_)}
    , rng_{seed}
    , uniform_{0.0, 1.0}
    , state_is_a_{sample_stationary_state_()}
    {}

    /*
     * Делает один временной шаг дихотомического марковского процесса
     * и возвращает factor f(t) для потенциала.
     *
     * Для high-temperature ratchet:
     *   sigma >= 0 -> f(t) = 1
     *   sigma < 0  -> f(t) = alpha
     */
    [[nodiscard]] double step() noexcept {
        const double random_value = uniform_(rng_);

        if (state_is_a_) {
            state_is_a_ = random_value < transition_.p_aa;
        } else {
            state_is_a_ = random_value >= transition_.p_bb;
        }

        return current_factor();
    }

    [[nodiscard]] double current_sigma() const noexcept {
        return state_is_a_
            ? params_.sigma_a
            : params_.sigma_b;
    }

    [[nodiscard]] double current_factor() const noexcept {
        return params_.factor_for_sigma(current_sigma());
    }

private:
    [[nodiscard]] static DichotomicParams validated_(
        DichotomicParams params
    ) {
        params.validate();
        return params;
    }

    [[nodiscard]] bool sample_stationary_state_() noexcept {
        return uniform_(rng_) < transition_.p_a_stationary;
    }

    DichotomicParams params_;
    DichotomicTransition transition_;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_;

    bool state_is_a_{true};
};

#endif // BROWNIAN_MOTOR_DICHOTOMICMODULATION_H