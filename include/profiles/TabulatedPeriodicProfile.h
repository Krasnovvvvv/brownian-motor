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

template <SpatialProfile ExactProfile>
class TabulatedPeriodicProfile final {
public:
    explicit TabulatedPeriodicProfile(
        ExactProfile exact_profile,
        double period,
        std::size_t intervals = 16'384
    )
        : exact_profile_{std::move(exact_profile)}
        , table_{
            build_table_(
                exact_profile_,
                period,
                intervals
            )
        }
    {}

    [[nodiscard]] double operator()(
        double x
    ) const noexcept {
        return exact_profile_(x);
    }

    [[nodiscard]] double derivative(
        double x
    ) const noexcept {
        const Table& table = *table_;

        if (!std::isfinite(x)) {
            return invalid_value_();
        }

        /*
         * Solver normally supplies a wrapped coordinate.
         * Keep a safe fallback for direct calls and rounding
         * at the periodic boundary.
         */
        double phase = x;

        if (phase < 0.0 || phase >= table.period) {
            phase = std::fmod(
                phase,
                table.period
            );

            if (phase < 0.0) {
                phase += table.period;
            }

            if (phase >= table.period) {
                phase = 0.0;
            }
        }

        const double grid_position =
            phase * table.inverse_step;

        /*
         * Multiplication can round a position very close
         * to the right boundary to exactly intervals.
         */
        if (
            grid_position >=
            static_cast<double>(table.intervals)
        ) {
            return table.samples.front();
        }

        const auto index =
            static_cast<std::size_t>(
                grid_position
            );

        const double fraction =
            grid_position -
            static_cast<double>(index);

        return std::lerp(
            table.samples[index],
            table.samples[index + 1],
            fraction
        );
    }

    [[nodiscard]] double derivative_scaled(
        double x,
        double factor
    ) const noexcept {
        return factor * derivative(x);
    }

    [[nodiscard]] double period() const noexcept {
        return table_->period;
    }

    [[nodiscard]] std::size_t intervals() const noexcept {
        return table_->intervals;
    }

    [[nodiscard]] std::size_t table_bytes() const noexcept {
        return table_->samples.size() * sizeof(double);
    }

private:
    struct Table final {
        double period{};
        double inverse_step{};

        std::size_t intervals{};

        std::vector<double> samples;
    };

    [[nodiscard]] static double invalid_value_() noexcept {
        return std::numeric_limits<double>::quiet_NaN();
    }

    [[nodiscard]] static std::shared_ptr<const Table>
    build_table_(
        const ExactProfile& exact_profile,
        double period,
        std::size_t intervals
    ) {
        if (
            !std::isfinite(period) ||
            period <= 0.0
        ) {
            throw std::invalid_argument(
                "Lookup period must be finite and positive"
            );
        }

        constexpr std::size_t max_intervals =
            1'048'576;

        if (
            intervals < 2 ||
            intervals > max_intervals
        ) {
            throw std::invalid_argument(
                "Lookup interval count must be "
                "between 2 and 1048576"
            );
        }

        const double inverse_step =
            static_cast<double>(intervals) / period;

        if (
            !std::isfinite(inverse_step) ||
            inverse_step <= 0.0
        ) {
            throw std::invalid_argument(
                "Lookup grid scale is not representable"
            );
        }

        auto table = std::make_shared<Table>();

        table->period = period;
        table->inverse_step = inverse_step;
        table->intervals = intervals;

        table->samples.reserve(intervals + 1);

        for (
            std::size_t index = 0;
            index < intervals;
            ++index
        ) {
            const double x =
                period *
                (
                    static_cast<double>(index) /
                    static_cast<double>(intervals)
                );

            const double value =
                exact_profile.derivative(x);

            if (!std::isfinite(value)) {
                throw std::domain_error(
                    "Non-finite derivative while "
                    "building lookup table"
                );
            }

            table->samples.push_back(value);
        }

        /*
         * A basic seam check, not a proof of periodicity.
         * Periodicity must be explicitly declared by
         * the potential definition.
         */
        const double endpoint =
            exact_profile.derivative(period);

        const double first =
            table->samples.front();

        if (!std::isfinite(endpoint)) {
            throw std::domain_error(
                "Non-finite derivative at "
                "the lookup period boundary"
            );
        }

        const double scale = std::max({
            1.0,
            std::abs(first),
            std::abs(endpoint)
        });

        const double normalized_difference =
            std::abs(
                first / scale -
                endpoint / scale
            );

        if (normalized_difference > 1.0e-9) {
            throw std::domain_error(
                "Derivative values do not match "
                "at the periodic lookup boundary"
            );
        }

        table->samples.push_back(first);

        return table;
    }

    ExactProfile exact_profile_;
    std::shared_ptr<const Table> table_;
};

#endif // BROWNIAN_MOTOR_TABULATEDPERIODICPROFILE_H