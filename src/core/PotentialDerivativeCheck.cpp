#include "core/PotentialDerivativeCheck.h"

#include "core/ExpressionEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace {

[[nodiscard]] std::string format_number(
    double value
) {
    std::ostringstream stream;

    stream << std::setprecision(12)
           << value;

    return stream.str();
}

[[nodiscard]] bool approximately_equal(
    double left,
    double right
) {
    constexpr double absolute_tolerance =
        1.0e-6;

    constexpr double relative_tolerance =
        5.0e-3;

    const double scale = std::max(
        std::abs(left),
        std::abs(right)
    );

    return std::abs(left - right) <=
        absolute_tolerance +
        relative_tolerance * scale;
}

} // namespace

std::optional<std::string>
check_potential_derivative(
    const PotentialDefinition& definition,
    const ExpressionEvaluator& evaluator
) {
    constexpr std::array<double, 9>
        normalized_points{
            -0.83,
            -0.57,
            -0.31,
            -0.13,
             0.0,
             0.17,
             0.39,
             0.68,
             0.91
        };

    const double coordinate_scale =
        definition.profile.periodic
            ? definition.profile.period
            : 2.0;

    if (
        !std::isfinite(coordinate_scale) ||
        coordinate_scale <= 0.0
    ) {
        return std::string{
            "Derivative check: invalid "
            "coordinate scale."
        };
    }

    const double h =
        1.0e-5 * coordinate_scale;

    if (
        !std::isfinite(h) ||
        h <= 0.0
    ) {
        return std::string{
            "Derivative check: cannot choose "
            "a finite difference step."
        };
    }

    for (const double normalized :
         normalized_points) {
        const double coordinate =
            normalized * coordinate_scale;

        const double declared_derivative =
            evaluator.derivative(
                coordinate
            );

        const double left_value =
            evaluator.value(
                coordinate - h
            );

        const double right_value =
            evaluator.value(
                coordinate + h
            );

        if (
            !std::isfinite(
                declared_derivative
            ) ||
            !std::isfinite(left_value) ||
            !std::isfinite(right_value)
        ) {
            return
                "Derivative check: non-finite "
                "value near " +
                definition.coordinate.symbol +
                " = " +
                format_number(coordinate) +
                ".";
        }

        /*
         * U'(x) ≈ [U(x + h) - U(x - h)] / (2h)
         */
        const double estimated_derivative =
            (right_value - left_value) /
            (2.0 * h);

        if (
            !std::isfinite(
                estimated_derivative
            )
        ) {
            return
                "Derivative check: non-finite "
                "numerical derivative near " +
                definition.coordinate.symbol +
                " = " +
                format_number(coordinate) +
                ".";
        }

        if (
            !approximately_equal(
                declared_derivative,
                estimated_derivative
            )
        ) {
            return
                "Derivative mismatch at " +
                definition.coordinate.symbol +
                " = " +
                format_number(coordinate) +
                ": declared derivative = " +
                format_number(
                    declared_derivative
                ) +
                ", estimated derivative = " +
                format_number(
                    estimated_derivative
                ) +
                ". Check "
                "profile.derivative_expression.";
        }
    }

    return std::nullopt;
}