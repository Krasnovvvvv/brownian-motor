#include "core/ExpressionEvaluator.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool approximately_equal(
    double left,
    double right,
    double tolerance = 1.0e-12
) {
    return std::abs(left - right) <= tolerance;
}

int fail(
    const std::string& message
) {
    std::cerr << message << '\n';

    return 1;
}

} // namespace

int main() {
    try {
        const PotentialDefinition definition{
            .format_version = 1,

            .id = "exprtk-smoke-test",
            .name = "ExprTk smoke test",
            .description =
                "Checks powers, functions, whitespace "
                "and a custom coordinate symbol.",

            .coordinate = PotentialCoordinateDefinition{
                .symbol = "z",
                .unit = "dimensionless"
            },

            .profile = PotentialProfileDefinition{
                .expression =
                    " A  * pow( z , p )  + "
                    " B * sin( z ) ^ 2 + "
                    " C * cos( z ) + "
                    " D * exp( z ) + "
                    " E * sqrt( abs( z ) ) ",

                .derivative_expression =
                    " A * p * z ^ ( p - 1 ) + "
                    " 2 * B * sin( z ) * cos( z ) - "
                    " C * sin( z ) + "
                    " D * exp( z ) + "
                    " E * z / "
                    " ( 2 * abs( z ) * sqrt( abs( z ) ) ) ",

                .periodic = false,
                .period = 0.0
            },

            .parameters = {
                PotentialParameterDefinition{
                    .id = "A",
                    .label = "Amplitude A",
                    .default_value = 3.0,
                    .minimum = -100.0,
                    .maximum = 100.0,
                    .step = 0.1,
                    .decimals = 6
                },

                PotentialParameterDefinition{
                    .id = "B",
                    .label = "Coefficient B",
                    .default_value = 0.0,
                    .minimum = -100.0,
                    .maximum = 100.0,
                    .step = 0.1,
                    .decimals = 6
                },

                PotentialParameterDefinition{
                    .id = "C",
                    .label = "Coefficient C",
                    .default_value = 0.0,
                    .minimum = -100.0,
                    .maximum = 100.0,
                    .step = 0.1,
                    .decimals = 6
                },

                PotentialParameterDefinition{
                    .id = "D",
                    .label = "Coefficient D",
                    .default_value = 0.0,
                    .minimum = -100.0,
                    .maximum = 100.0,
                    .step = 0.1,
                    .decimals = 6
                },

                PotentialParameterDefinition{
                    .id = "E",
                    .label = "Coefficient E",
                    .default_value = 0.0,
                    .minimum = -100.0,
                    .maximum = 100.0,
                    .step = 0.1,
                    .decimals = 6
                },

                PotentialParameterDefinition{
                    .id = "p",
                    .label = "Power p",
                    .default_value = 2.0,
                    .minimum = 1.0,
                    .maximum = 10.0,
                    .step = 0.1,
                    .decimals = 3
                }
            }
        };

        const ExpressionEvaluator evaluator{
            definition
        };

        constexpr double z = 2.0;

        const double profile_value =
            evaluator.value(z);

        const double derivative_value =
            evaluator.derivative(z);

        /*
         * U(z) = A * z^p = 3 * 2^2 = 12.
         */
        if (
            !approximately_equal(
                profile_value,
                12.0
            )
        ) {
            return fail(
                "Unexpected expression value. "
                "Expected 12."
            );
        }

        /*
         * dU/dz = A * p * z^(p - 1)
         *        = 3 * 2 * 2 = 12.
         */
        if (
            !approximately_equal(
                derivative_value,
                12.0
            )
        ) {
            return fail(
                "Unexpected derivative value. "
                "Expected 12."
            );
        }

        std::cout
            << "ExprTk smoke test passed.\n";

        return 0;
    } catch (const std::exception& exception) {
        return fail(
            std::string{
                "ExprTk smoke test failed: "
            } + exception.what()
        );
    }
}