#ifndef BROWNIAN_MOTOR_POTENTIAL_H
#define BROWNIAN_MOTOR_POTENTIAL_H
#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

template <typename P>
concept SpatialProfile =
    std::invocable<P, double> &&
    std::convertible_to<std::invoke_result_t<P, double>, double> &&
    requires(const P& profile, double x) {
    { profile.derivative(x) } -> std::convertible_to<double>;
    };

template <SpatialProfile Profile>
class Potential final {
    Profile profile_;

public:
    explicit Potential(Profile profile)
        : profile_{std::move(profile)}
    {}

    [[nodiscard]] double value(
        double x,
        double factor
    ) const noexcept {
        return factor * profile_(x);
    }

    [[nodiscard]] double profile_value(
        double x
    ) const noexcept {
        return profile_(x);
    }

    [[nodiscard]] double derivative_value(
        double x,
        double factor
    ) const noexcept {
        if constexpr (
            requires(const Profile& profile, double position, double scale) {
                {
                    profile.derivative_scaled(position, scale)
                } -> std::convertible_to<double>;
            }
        ) {
            return profile_.derivative_scaled(x, factor);
        } else {
            return factor * profile_.derivative(x);
        }
    }

    [[nodiscard]] double force_value(
        double x,
        double factor
    ) const noexcept {
        return -derivative_value(x, factor);
    }

    [[nodiscard]] const Profile& profile() const noexcept {
        return profile_;
    }
};

#endif // BROWNIAN_MOTOR_POTENTIAL_H