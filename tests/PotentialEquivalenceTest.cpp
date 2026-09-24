#include "core/ExpressionEvaluator.h"
#include "core/PotentialConfigParser.h"
#include "core/PotentialDerivativeCheck.h"
#include "profiles/BiharmonicProfile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool close_enough(
    double left,
    double right
) {
    const double scale = std::max(
        std::abs(left),
        std::abs(right)
    );

    return std::abs(left - right) <=
        1.0e-11 +
        1.0e-10 * scale;
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
        const QString file_name =
            QString::fromUtf8(
                BROWNIAN_MOTOR_SOURCE_DIR
            ) +
            "/resources/potentials/"
            "biharmonic-ratchet.bmpotential";

        const auto parsed =
            PotentialConfigParser::parse_file(
                file_name
            );

        if (!parsed.is_success()) {
            return fail(
                "Cannot load test potential: " +
                parsed.error_message
                    .toStdString()
            );
        }

        const PotentialDefinition&
            definition =
                *parsed.definition;

        constexpr std::array<double, 9>
            coordinates{
                -1.75,
                -0.5,
                -0.125,
                 0.0,
                 0.125,
                 0.25,
                 0.5,
                 0.75,
                 1.75
            };

        struct Parameters {
            double v1;
            double v2;
        };

        constexpr std::array<Parameters, 4>
            parameter_sets{
                Parameters{0.20, 0.10},
                Parameters{0.10, 0.025},
                Parameters{-0.40, 0.15},
                Parameters{0.0, -0.25}
            };

        for (
            const auto [v1, v2] :
            parameter_sets
        ) {
            const BiharmonicProfile native{
                .V1 = v1,
                .V2 = v2
            };

            const PotentialParameterValues
                values{
                    {"V1", v1},
                    {"V2", v2}
                };

            const ExpressionEvaluator expression{
                definition,
                values
            };

            for (
                const double x :
                coordinates
            ) {
                if (
                    !close_enough(
                        native(x),
                        expression.value(x)
                    )
                ) {
                    return fail(
                        "Profile value differs at "
                        "x = " +
                        std::to_string(x)
                    );
                }

                if (
                    !close_enough(
                        native.derivative(x),
                        expression.derivative(x)
                    )
                ) {
                    return fail(
                        "Derivative differs at "
                        "x = " +
                        std::to_string(x)
                    );
                }
            }

            const auto derivative_error =
                check_potential_derivative(
                    definition,
                    expression
                );

            if (derivative_error) {
                return fail(
                    "Valid biharmonic derivative "
                    "was rejected: " +
                    *derivative_error
                );
            }
        }

        /*
         * Намеренно испорченная производная.
         * Компилируется, но математически
         * не соответствует U(x).
         */
        PotentialDefinition incorrect =
            definition;

        incorrect.profile.derivative_expression =
            "0";

        const ExpressionEvaluator
            incorrect_evaluator{
                incorrect,
                PotentialParameterValues{
                    {"V1", 0.2},
                    {"V2", 0.1}
                }
            };

        if (
            !check_potential_derivative(
                incorrect,
                incorrect_evaluator
            )
        ) {
            return fail(
                "Incorrect derivative "
                "was accepted."
            );
        }

        std::cout
            << "Potential equivalence "
               "test passed.\n";

        return 0;
    } catch (
        const std::exception& exception
    ) {
        return fail(
            std::string{
                "Potential equivalence "
                "test failed: "
            } +
            exception.what()
        );
    }
}