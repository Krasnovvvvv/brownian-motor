#ifndef BROWNIAN_MOTOR_INITIALPOSITIONDISTRIBUTION_H
#define BROWNIAN_MOTOR_INITIALPOSITIONDISTRIBUTION_H
#pragma once

#include "core/SeedFactory.h"
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <vector>

enum class InitialPositionKind {
    Point,
    Uniform,
    Gaussian
};

struct InitialPositionDistribution final {
    InitialPositionKind kind{InitialPositionKind::Point};
    double lower{0.0};
    double upper{1.0};
    double mean{0.0};
    double standard_deviation{0.1};

    [[nodiscard]] const char *name() const noexcept {
        switch (kind) {
            case InitialPositionKind::Point: return "point";
            case InitialPositionKind::Uniform: return "uniform";
            case InitialPositionKind::Gaussian: return "gaussian";
        }
        return "unknown";
    }

    void validate(double legacy_x0) const {
        switch (kind) {
            case InitialPositionKind::Point:
                if (!std::isfinite(legacy_x0)) {
                    throw std::invalid_argument("Initial position must be finite");
                }
                return;
            case InitialPositionKind::Uniform:
                if (!std::isfinite(lower) || !std::isfinite(upper) ||
                    !(lower < upper) || !std::isfinite(upper - lower)) {
                    throw std::invalid_argument("Uniform initialization requires finite From < To and a finite width");
                }
                return;
            case InitialPositionKind::Gaussian:
                if (!std::isfinite(mean) || !std::isfinite(standard_deviation) ||
                    standard_deviation <= 0.0) {
                    throw std::invalid_argument("Gaussian initialization requires a finite mean and finite standard deviation > 0");
                }
                return;
        }
        throw std::invalid_argument("Unknown initial position distribution");
    }

    [[nodiscard]] double sample(
        double legacy_x0,
        const SeedFactory &seeds,
        std::size_t particle
    ) const {
        // Point consumes no random numbers and preserves the original path.
        if (kind == InitialPositionKind::Point) return legacy_x0;
        auto engine = seeds.make_mt19937(
            particle,
            SeedFactory::Stream::InitialPosition
        );
        double x{};
        switch (kind) {
            case InitialPositionKind::Uniform: {
                std::uniform_real_distribution<double> distribution{lower, upper};
                x = distribution(engine);
                break;
            }
            case InitialPositionKind::Gaussian: {
                std::normal_distribution<double> distribution{mean, standard_deviation};
                x = distribution(engine);
                break;
            }
            case InitialPositionKind::Point:
                return legacy_x0;
            default:
                throw std::invalid_argument("Unknown initial position distribution");
        }
        if (!std::isfinite(x)) {
            throw std::runtime_error("Non-finite sampled initial position; reduce the initialization scale");
        }
        return x;
    }
};

struct InitialPositionSummary final {
    double mean{};
    // Actual finite-ensemble population variance, denominator N.
    double variance{};
};

[[nodiscard]] inline InitialPositionSummary summarize_initial_positions(
    const std::vector<double> &positions
) {
    if (positions.empty()) throw std::invalid_argument("Initial ensemble is empty");
    long double mean=0.0L, m2=0.0L;
    std::size_t count=0;
    for (const double x : positions) {
        if (!std::isfinite(x)) throw std::invalid_argument("Non-finite initial position");
        ++count;
        const long double delta=static_cast<long double>(x)-mean;
        mean += delta/static_cast<long double>(count);
        m2 += delta*(static_cast<long double>(x)-mean);
    }
    const double result_mean=static_cast<double>(mean);
    const double result_variance=static_cast<double>(m2/static_cast<long double>(count));
    if (!std::isfinite(result_mean) || !std::isfinite(result_variance)) {
        throw std::runtime_error("Initial ensemble statistics overflow; reduce the initialization scale");
    }
    return {result_mean, result_variance < 0.0 ? 0.0 : result_variance};
}

#endif
