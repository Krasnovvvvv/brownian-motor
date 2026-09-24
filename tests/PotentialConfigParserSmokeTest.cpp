#include "core/ExpressionEvaluator.h"
#include "core/PotentialConfigParser.h"

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
        const QString file_name =
            QString::fromUtf8(
                BROWNIAN_MOTOR_SOURCE_DIR
            ) +
            "/resources/potentials/"
            "biharmonic-ratchet.bmpotential";

        const PotentialConfigParseResult result =
            PotentialConfigParser::parse_file(
                file_name
            );

        if (!result.is_success()) {
            return fail(
                std::string{
                    "Potential parser failed: "
                } +
                result.error_message.toStdString()
            );
        }

        const PotentialDefinition& definition =
            *result.definition;

        if (definition.id != "biharmonic-ratchet") {
            return fail(
                "Unexpected potential id."
            );
        }

        if (definition.coordinate.symbol != "x") {
            return fail(
                "Unexpected coordinate symbol."
            );
        }

        if (definition.parameters.size() != 2) {
            return fail(
                "Expected exactly two potential parameters."
            );
        }

        const ExpressionEvaluator evaluator{
            definition
        };

        constexpr double x = 0.125;

        constexpr double expected_value =
            0.2 * std::sqrt(0.5) +
            0.1;

        const double actual_value =
            evaluator.value(x);

        if (
            !approximately_equal(
                actual_value,
                expected_value
            )
        ) {
            return fail(
                "Unexpected Biharmonic profile value."
            );
        }

        const double derivative_value =
            evaluator.derivative(x);

        if (!std::isfinite(derivative_value)) {
            return fail(
                "Derivative must be finite."
            );
        }

        std::cout
            << "Potential config parser smoke test passed.\n";

        return 0;
    } catch (const std::exception& exception) {
        return fail(
            std::string{
                "Potential config parser smoke test failed: "
            } + exception.what()
        );
    }
}