#ifndef BROWNIAN_MOTOR_EXPONENTIALINERTIALSTEP_H
#define BROWNIAN_MOTOR_EXPONENTIALINERTIALSTEP_H
#pragma once
#include <cmath>
#include <stdexcept>

struct InertialStepResult final {
    double position{}, velocity{}, displacement{};
};

struct InertialControlResult final {
    double velocity_difference{}, displacement_difference{};
};

class ExponentialInertialStep final {
public:
    explicit ExponentialInertialStep(double h, double epsilon) {
        if (!std::isfinite(h) || h <= 0 || !std::isfinite(epsilon) || epsilon <= 0)
            throw std::invalid_argument("dt and inertia_ratio must be finite and positive");
        const long double hh = h, ee = epsilon, r = hh / ee;
        const long double root = std::sqrt(hh) * std::sqrt(2.0L);
        long double a, q, u, b, bv, bx;
        if (r < 0.01L) {
            b = r * (0.5L + r * (-1.0L / 6 + r * (1.0L / 24 + r * (-1.0L / 120 +
                                                                   r * (1.0L / 720 + r * (
                                                                            -1.0L / 5040 + r * (
                                                                                1.0L / 40320 - r / 362880)))))));
            u = 1 - b;
            q = -std::expm1(-r);
            a = std::exp(-r);
            const long double d = r * r * (1.0L / 12 + r * (-1.0L / 12 + r * (17.0L / 360 +
                                                                              r * (-7.0L / 360 + r * (
                                                                                      43.0L / 6720 + r * (
                                                                                          -107.0L / 60480 +
                                                                                          r * (769.0L / 1814400 - r *
                                                                                              163.0L / 1814400)))))));
            const long double rootd = std::sqrt(d);
            bv = (root / ee) * rootd;
            bx = -root * rootd;
            B_ = static_cast<double>(hh * u);
        } else {
            a = std::exp(-r);
            q = -std::expm1(-r);
            u = std::isinf(r) ? ee / hh : q / r;
            b = 1 - u;
            const long double residual = q * (2 - q) - 2 * q * u;
            if (residual < 0)throw std::runtime_error("Negative inertial covariance");
            bv = std::sqrt(residual) / std::sqrt(ee);
            bx = -std::sqrt(ee) * std::sqrt(residual);
            B_ = static_cast<double>(ee * q);
        }
        a_ = static_cast<double>(a);
        q_ = static_cast<double>(q);
        C_ = static_cast<double>(hh * b);
        vx_ = static_cast<double>((root / hh) * q);
        vy_ = static_cast<double>(bv);
        xx_ = static_cast<double>(root * b);
        xy_ = static_cast<double>(bx);
        if (!std::isfinite(a_) || !std::isfinite(q_) || !std::isfinite(B_) || !std::isfinite(C_) ||
            !std::isfinite(vx_) || !std::isfinite(vy_) || !std::isfinite(xx_) || !std::isfinite(xy_))
            throw std::overflow_error("Inertial coefficients are not representable");
    }

    [[nodiscard]] InertialStepResult advance(double x, double v, double F, double z1, double z2) const {
        if (!std::isfinite(x) || !std::isfinite(v) || !std::isfinite(F) || !std::isfinite(z1) || !std::isfinite(z2))
            throw std::invalid_argument("Non-finite inertial input");
        InertialStepResult result;
        result.displacement = B_ * v + C_ * F + xx_ * z1 + xy_ * z2;
        result.position = x + result.displacement;
        result.velocity = a_ * v + q_ * F + vx_ * z1 + vy_ * z2;
        if (!std::isfinite(result.position) || !std::isfinite(result.velocity) || !std::isfinite(result.displacement))
            throw std::overflow_error("Non-finite inertial result");
        return result;
    }

    [[nodiscard]] InertialControlResult advance_free_reference_control(double d, double F) const {
        InertialControlResult result{a_ * d + q_ * F, B_ * d + C_ * F};
        if (!std::isfinite(result.velocity_difference) || !std::isfinite(result.displacement_difference))
            throw std::overflow_error("Non-finite inertial CV result");
        return result;
    }

private:
    double a_{}, q_{}, B_{}, C_{}, vx_{}, vy_{}, xx_{}, xy_{};
};
#endif
