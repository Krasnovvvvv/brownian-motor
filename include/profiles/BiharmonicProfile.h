#ifndef BROWNIAN_MOTOR_BIHARMONICPROFILE_H
#define BROWNIAN_MOTOR_BIHARMONICPROFILE_H
#pragma once

#include <cmath>
#include <numbers>

struct BiharmonicProfile final {
    double V1;
    double V2;

    [[nodiscard]] double operator()(double x) const noexcept {
        constexpr double two_pi = 2.0 * std::numbers::pi_v<double>;

        return V1 * std::sin(two_pi * x)
             + V2 * std::sin(2.0 * two_pi * x);
    }

    [[nodiscard]] double derivative(double x) const noexcept {
        return derivative_scaled(x, 1.0);
    }

    [[nodiscard]] double derivative_scaled(
        double x,
        double factor
    ) const noexcept {
        constexpr double two_pi = 2.0 * std::numbers::pi_v<double>;
        constexpr double four_pi = 4.0 * std::numbers::pi_v<double>;

        const double cosine_first_harmonic =
            std::cos(two_pi * x);

        const double cosine_second_harmonic =
            2.0 * cosine_first_harmonic *
            cosine_first_harmonic -
            1.0;

        return factor * (
            two_pi * V1 * cosine_first_harmonic +
            four_pi * V2 * cosine_second_harmonic
        );
    }
};

#endif // BROWNIAN_MOTOR_BIHARMONICPROFILE_H