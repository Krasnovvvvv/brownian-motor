#ifndef BROWNIAN_MOTOR_TABULATEDPERIODICPROFILE_H
#define BROWNIAN_MOTOR_TABULATEDPERIODICPROFILE_H
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include "core/Potential.h"

template<SpatialProfile ExactProfile>
class TabulatedPeriodicProfile final {
public:
    explicit TabulatedPeriodicProfile(
        ExactProfile exact_profile, double period, std::size_t intervals = 16'384
    ) : exact_profile_{std::move(exact_profile)},
        table_{build_table_(exact_profile_, period, intervals)} {}

    [[nodiscard]] double operator()(double x) const noexcept { return exact_profile_(x); }

    [[nodiscard]] double derivative(double x) const noexcept {
        if (!std::isfinite(x)) return invalid_value_();
        const Table &table = *table_;
        double phase = x;
        if (phase < 0.0 || phase >= table.period) {
            phase = std::fmod(phase, table.period);
            if (phase < 0.0) phase += table.period;
            if (phase >= table.period) phase = 0.0;
        }

        if (!table.piecewise) {
            const double position = phase * table.inverse_step;
            if (position >= static_cast<double>(table.intervals)) return table.samples.front();
            const auto i = static_cast<std::size_t>(position);
            return std::lerp(table.samples[i], table.samples[i + 1],
                             position - static_cast<double>(i));
        }

        // Equality selects the right interval. No interpolation crosses a jump.
        const auto it = std::upper_bound(table.breakpoints.begin(), table.breakpoints.end(), phase);
        const auto index = static_cast<std::size_t>(it - table.breakpoints.begin());
        const Segment &s = table.segments[index];
        if (s.constant) return s.samples.front();
        const double position = (phase - s.left) * s.inverse_step;
        if (position <= 0.0) return s.samples.front();
        if (position >= static_cast<double>(s.intervals)) return s.samples.back();
        const auto i = static_cast<std::size_t>(position);
        return std::lerp(s.samples[i], s.samples[i + 1], position - static_cast<double>(i));
    }

    [[nodiscard]] double derivative_scaled(double x, double factor) const noexcept {
        return factor * derivative(x);
    }
    [[nodiscard]] double period() const noexcept { return table_->period; }
    [[nodiscard]] std::size_t intervals() const noexcept { return table_->intervals; }
    // Sample payload only, matching the previous meaning of this method.
    [[nodiscard]] std::size_t table_bytes() const noexcept {
        std::size_t count = table_->samples.size();
        for (const auto &s : table_->segments) count += s.samples.size();
        return count * sizeof(double);
    }

private:
    struct Segment final {
        double left{};
        double inverse_step{};
        std::size_t intervals{};
        bool constant{false};
        std::vector<double> samples;
    };
    struct Table final {
        double period{};
        double inverse_step{};
        std::size_t intervals{};
        bool piecewise{false};
        std::vector<double> samples;
        std::vector<double> breakpoints;
        std::vector<Segment> segments;
    };
    static double invalid_value_() noexcept { return std::numeric_limits<double>::quiet_NaN(); }

    static double sample_(const ExactProfile &profile, double x) {
        const double y = profile.derivative(x);
        if (!std::isfinite(y)) throw std::domain_error("Non-finite derivative while building lookup table");
        return y;
    }

    static std::shared_ptr<const Table> build_table_(
        const ExactProfile &profile, double period, std::size_t intervals
    ) {
        constexpr std::size_t maximum = 1'048'576;
        if (!std::isfinite(period) || period <= 0.0) {
            throw std::invalid_argument("Lookup period must be finite and positive");
        }
        if (intervals < 2 || intervals > maximum) {
            throw std::invalid_argument("Lookup interval count must be between 2 and 1048576");
        }
        auto table = std::make_shared<Table>();
        table->period = period;
        table->intervals = intervals;
        if constexpr (requires { profile.piecewise(); profile.breakpoints(); }) {
            table->piecewise = profile.piecewise();
            if (table->piecewise) {
                const auto &boundaries = profile.breakpoints();
                table->breakpoints.assign(boundaries.begin(), boundaries.end());
                if constexpr (requires { profile.definition().profile.period; }) {
                    if (profile.definition().profile.period != period) {
                        throw std::invalid_argument("Evaluator and lookup periods must match");
                    }
                }
            }
        }

        if (!table->piecewise) {
            table->inverse_step = static_cast<double>(intervals) / period;
            if (!std::isfinite(table->inverse_step) || table->inverse_step <= 0.0) {
                throw std::invalid_argument("Lookup grid scale is not representable");
            }
            table->samples.reserve(intervals + 1);
            for (std::size_t i = 0; i < intervals; ++i) {
                table->samples.push_back(sample_(profile, period *
                    (static_cast<double>(i) / static_cast<double>(intervals))));
            }
            const double endpoint = sample_(profile, period);
            const double first = table->samples.front();
            const double scale = std::max({1.0, std::abs(first), std::abs(endpoint)});
            if (std::abs(first / scale - endpoint / scale) > 1.0e-9) {
                throw std::domain_error("Derivative values do not match at the periodic lookup boundary");
            }
            table->samples.push_back(first);
            return table;
        }

        // Validate independently: ExactProfile need not be ExpressionEvaluator.
        std::vector<double> edges{0.0};
        for (const double b : table->breakpoints) {
            if (!std::isfinite(b) || b <= edges.back() || b >= period) {
                throw std::invalid_argument("Invalid lookup breakpoints");
            }
            edges.push_back(b);
        }
        edges.push_back(period);
        const std::size_t count = edges.size() - 1;
        if (count > maximum / 2) throw std::invalid_argument("Too many lookup segments");
        // At least two interpolation intervals per segment; preserve total budget otherwise.
        table->intervals = std::max(intervals, 2 * count);
        const std::size_t free = table->intervals - 2 * count;
        std::vector<std::size_t> counts(count, 2);
        std::size_t allocated = 2 * count;
        for (std::size_t i = 0; i < count; ++i) {
            const double share = static_cast<double>(free) * ((edges[i + 1] - edges[i]) / period);
            const auto extra = static_cast<std::size_t>(std::floor(share));
            counts[i] += extra;
            allocated += extra;
        }
        if (allocated > table->intervals) throw std::invalid_argument("Lookup budget rounding error");
        for (std::size_t i = 0; allocated < table->intervals; ++i, ++allocated) ++counts[i % count];
        table->segments.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            const double lo = edges[i];
            const double hi = edges[i + 1];
            Segment s;
            s.left = lo;
            s.intervals = counts[i];
            s.inverse_step = static_cast<double>(s.intervals) / (hi - lo);
            if (!std::isfinite(s.inverse_step) || s.inverse_step <= 0.0) {
                throw std::invalid_argument("Segment grid scale is not representable");
            }
            const double left_inside = std::nextafter(lo, hi);
            const double right_inside = std::nextafter(hi, lo);
            if (!(lo < left_inside && left_inside < right_inside && right_inside < hi)) {
                throw std::invalid_argument("Lookup segment is too narrow");
            }
            s.samples.reserve(s.intervals + 1);
            for (std::size_t j = 0; j <= s.intervals; ++j) {
                const double t = static_cast<double>(j) / static_cast<double>(s.intervals);
                const double x = std::clamp(std::lerp(lo, hi, t), left_inside, right_inside);
                s.samples.push_back(sample_(profile, x));
            }
            // Lossless compression of this sampled table, not a symbolic proof
            // that an arbitrary analytic expression is constant everywhere.
            s.constant = std::all_of(s.samples.begin(), s.samples.end(),
                [&](double y) { return y == s.samples.front(); });
            if (s.constant) {
                std::vector<double> compact{s.samples.front()};
                s.samples.swap(compact);
            }
            table->segments.push_back(std::move(s));
        }
        return table;
    }

    ExactProfile exact_profile_;
    std::shared_ptr<const Table> table_;
};

#endif