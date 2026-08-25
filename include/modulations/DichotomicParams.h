#ifndef BROWNIAN_MOTOR_DICHOTOMICPARAMS_H
#define BROWNIAN_MOTOR_DICHOTOMICPARAMS_H
#pragma once

#include <cmath>
#include <stdexcept>

struct DichotomicParams final {
    double sigma_a{};
    double sigma_b{};

    // sigma_a -> sigma_b
    double gamma_a{};

    // sigma_b -> sigma_a
    double gamma_b{};

    double dt{};
    double alpha{};

    void validate() const {
        if (!std::isfinite(sigma_a) || !std::isfinite(sigma_b)) {
            throw std::invalid_argument("dichotomic states must be finite");
        }

        if (sigma_a == sigma_b) {
            throw std::invalid_argument(
                "sigma_a and sigma_b must be different"
            );
        }

        if (!std::isfinite(gamma_a) || !std::isfinite(gamma_b) ||
            gamma_a <= 0.0 || gamma_b <= 0.0) {
            throw std::invalid_argument(
                "gamma_a and gamma_b must be finite and > 0"
            );
        }

        if (!std::isfinite(dt) || dt <= 0.0) {
            throw std::invalid_argument("dt must be finite and > 0");
        }

        if (!std::isfinite(alpha)) {
            throw std::invalid_argument("alpha must be finite");
        }
    }

    [[nodiscard]] double total_rate() const noexcept {
        return gamma_a + gamma_b;
    }

    [[nodiscard]] double tau_c() const noexcept {
        return 1.0 / total_rate();
    }

    [[nodiscard]] double mean_sigma() const noexcept {
        return (sigma_a * gamma_b + sigma_b * gamma_a) / total_rate();
    }

    [[nodiscard]] double second_moment() const noexcept {
        return (sigma_a * sigma_a * gamma_b +
                sigma_b * sigma_b * gamma_a) / total_rate();
    }

    [[nodiscard]] double variance_sigma() const noexcept {
        const double mean = mean_sigma();
        return second_moment() - mean * mean;
    }

    [[nodiscard]] double factor_for_sigma(double sigma) const noexcept {
        return sigma >= 0.0 ? 1.0 : alpha;
    }

    [[nodiscard]] static DichotomicParams symmetric(
        double amplitude,
        double tau_c,
        double dt,
        double alpha
    ) {
        if (!std::isfinite(amplitude) || amplitude == 0.0) {
            throw std::invalid_argument("amplitude must be finite and != 0");
        }

        if (!std::isfinite(tau_c) || tau_c <= 0.0) {
            throw std::invalid_argument("tau_c must be finite and > 0");
        }

        const double total_rate = 1.0 / tau_c;

        DichotomicParams params{
            .sigma_a = amplitude,
            .sigma_b = -amplitude,
            .gamma_a = 0.5 * total_rate,
            .gamma_b = 0.5 * total_rate,
            .dt = dt,
            .alpha = alpha
        };

        params.validate();
        return params;
    }

    [[nodiscard]] static DichotomicParams zero_mean_asymmetric(
        double sigma_a,
        double sigma_b,
        double gamma_b,
        double dt,
        double alpha
    ) {
        if (!std::isfinite(sigma_a) || !std::isfinite(sigma_b) ||
            sigma_b == 0.0) {
            throw std::invalid_argument(
                "sigma_a and non-zero sigma_b must be finite"
            );
        }

        if (!std::isfinite(gamma_b) || gamma_b <= 0.0) {
            throw std::invalid_argument("gamma_b must be finite and > 0");
        }

        const double gamma_a = -(sigma_a / sigma_b) * gamma_b;

        DichotomicParams params{
            .sigma_a = sigma_a,
            .sigma_b = sigma_b,
            .gamma_a = gamma_a,
            .gamma_b = gamma_b,
            .dt = dt,
            .alpha = alpha
        };

        params.validate();

        if (params.gamma_a <= 0.0) {
            throw std::invalid_argument(
                "zero-mean states must have opposite signs"
            );
        }

        return params;
    }
};

#endif // BROWNIAN_MOTOR_DICHOTOMICPARAMS_H