#include "core/ExpressionEvaluator.h"

#include <exprtk.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] bool is_identifier_start(
    char character
) {
    return
        (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        character == '_';
}

[[nodiscard]] bool is_identifier_character(
    char character
) {
    return
        is_identifier_start(character) ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_valid_identifier(
    const std::string& identifier
) {
    if (
        identifier.empty() ||
        !is_identifier_start(identifier.front())
    ) {
        return false;
    }

    for (const char character : identifier) {
        if (!is_identifier_character(character)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool is_reserved_identifier(
    const std::string& identifier
) {
    static const std::unordered_set<std::string>
        reserved_identifiers{
            "abs",
            "cos",
            "e",
            "exp",
            "pi",
            "pow",
            "sin",
            "sqrt"
        };

    return reserved_identifiers.contains(identifier);
}

void validate_parameter_definition(
    const PotentialParameterDefinition& parameter
) {
    if (!is_valid_identifier(parameter.id)) {
        throw std::invalid_argument{
            "Potential parameter id '" +
            parameter.id +
            "' is not a valid identifier."
        };
    }

    if (is_reserved_identifier(parameter.id)) {
        throw std::invalid_argument{
            "Potential parameter id '" +
            parameter.id +
            "' is reserved by the expression language."
        };
    }

    if (
        !std::isfinite(parameter.minimum) ||
        !std::isfinite(parameter.maximum) ||
        !std::isfinite(parameter.default_value) ||
        !std::isfinite(parameter.step)
    ) {
        throw std::invalid_argument{
            "Potential parameter '" +
            parameter.id +
            "' contains a non-finite numeric value."
        };
    }

    if (parameter.minimum >= parameter.maximum) {
        throw std::invalid_argument{
            "Potential parameter '" +
            parameter.id +
            "' must satisfy minimum < maximum."
        };
    }

    if (
        parameter.default_value < parameter.minimum ||
        parameter.default_value > parameter.maximum
    ) {
        throw std::invalid_argument{
            "Default value of potential parameter '" +
            parameter.id +
            "' is outside its allowed range."
        };
    }

    if (parameter.step <= 0.0) {
        throw std::invalid_argument{
            "Step of potential parameter '" +
            parameter.id +
            "' must be greater than zero."
        };
    }

    if (
        parameter.decimals < 0 ||
        parameter.decimals > 15
    ) {
        throw std::invalid_argument{
            "Decimals of potential parameter '" +
            parameter.id +
            "' must be in range [0, 15]."
        };
    }
}

void validate_definition(
    const PotentialDefinition& definition
) {
    if (!is_valid_identifier(
            definition.coordinate.symbol
        )) {
        throw std::invalid_argument{
            "Coordinate symbol '" +
            definition.coordinate.symbol +
            "' is not a valid identifier."
        };
    }

    if (is_reserved_identifier(
            definition.coordinate.symbol
        )) {
        throw std::invalid_argument{
            "Coordinate symbol '" +
            definition.coordinate.symbol +
            "' is reserved by the expression language."
        };
    }

    if (definition.profile.expression.empty()) {
        throw std::invalid_argument{
            "Potential expression must not be empty."
        };
    }

    if (
        definition.profile.derivative_expression.empty()
    ) {
        throw std::invalid_argument{
            "Potential derivative expression "
            "must not be empty."
        };
    }

    if (definition.profile.periodic) {
        if (
            !std::isfinite(
                definition.profile.period
            ) ||
            definition.profile.period <= 0.0
        ) {
            throw std::invalid_argument{
                "Periodic potential must define "
                "a finite period greater than zero."
            };
        }
    }

    std::unordered_set<std::string> identifiers;

    identifiers.insert(
        definition.coordinate.symbol
    );

    for (const auto& parameter : definition.parameters) {
        validate_parameter_definition(parameter);

        if (
            parameter.id ==
            definition.coordinate.symbol
        ) {
            throw std::invalid_argument{
                "Potential parameter id '" +
                parameter.id +
                "' conflicts with coordinate symbol."
            };
        }

        if (!identifiers.insert(parameter.id).second) {
            throw std::invalid_argument{
                "Potential parameter id '" +
                parameter.id +
                "' is declared more than once."
            };
        }
    }
}

[[nodiscard]] double parameter_value(
    const PotentialParameterDefinition& parameter,
    const PotentialParameterValues& values
) {
    const auto iterator = values.find(
        parameter.id
    );

    const double value =
        iterator == values.end()
            ? parameter.default_value
            : iterator->second;

    if (!std::isfinite(value)) {
        throw std::invalid_argument{
            "Value of potential parameter '" +
            parameter.id +
            "' must be finite."
        };
    }

    if (
        value < parameter.minimum ||
        value > parameter.maximum
    ) {
        throw std::invalid_argument{
            "Value of potential parameter '" +
            parameter.id +
            "' is outside its allowed range."
        };
    }

    return value;
}

void validate_parameter_values(
    const PotentialDefinition& definition,
    const PotentialParameterValues& values
) {
    std::unordered_set<std::string>
        declared_identifiers;

    for (const auto& parameter : definition.parameters) {
        declared_identifiers.insert(
            parameter.id
        );
    }

    for (const auto& [id, value] : values) {
        if (!declared_identifiers.contains(id)) {
            throw std::invalid_argument{
                "Value supplied for undeclared "
                "potential parameter '" +
                id +
                "'."
            };
        }

        if (!std::isfinite(value)) {
            throw std::invalid_argument{
                "Value of potential parameter '" +
                id +
                "' must be finite."
            };
        }
    }
}

} // namespace

class ExpressionEvaluator::Impl final {
public:
    using SymbolTable =
        exprtk::symbol_table<double>;

    using Expression =
        exprtk::expression<double>;

    using Parser =
        exprtk::parser<double>;

    explicit Impl(
        const PotentialDefinition& source_definition,
        const PotentialParameterValues& values
    )
        : definition{source_definition}
    {
        validate_definition(definition);
        validate_parameter_values(
            definition,
            values
        );

        parameter_storage.reserve(
            definition.parameters.size()
        );

        for (const auto& parameter : definition.parameters) {
            parameter_storage.push_back(
                parameter_value(
                    parameter,
                    values
                )
            );
        }

        symbol_table.add_variable(
            definition.coordinate.symbol,
            coordinate
        );

        for (
            std::size_t index = 0;
            index < definition.parameters.size();
            ++index
        ) {
            symbol_table.add_variable(
                definition.parameters[index].id,
                parameter_storage[index]
            );
        }

        symbol_table.add_constants();

        profile_expression.register_symbol_table(
            symbol_table
        );

        derivative_expression.register_symbol_table(
            symbol_table
        );

        if (
            !parser.compile(
                definition.profile.expression,
                profile_expression
            )
        ) {
            throw std::invalid_argument{
                "Cannot compile potential expression."
            };
        }

        if (
            !parser.compile(
                definition.profile.derivative_expression,
                derivative_expression
            )
        ) {
            throw std::invalid_argument{
                "Cannot compile potential derivative expression."
            };
        }
    }

    [[nodiscard]] double value(
        double coordinate_value
    ) noexcept {
        coordinate = coordinate_value;

        return profile_expression.value();
    }

    [[nodiscard]] double derivative(
        double coordinate_value
    ) noexcept {
        coordinate = coordinate_value;

        return derivative_expression.value();
    }

    PotentialDefinition definition;

    double coordinate{0.0};

    std::vector<double> parameter_storage;

    SymbolTable symbol_table;
    Expression profile_expression;
    Expression derivative_expression;
    Parser parser;
};

ExpressionEvaluator::ExpressionEvaluator(
    const PotentialDefinition& definition,
    const PotentialParameterValues& parameter_values
)
    : impl_{
        std::make_unique<Impl>(
            definition,
            parameter_values
        )
    }
{}

ExpressionEvaluator::~ExpressionEvaluator() = default;

ExpressionEvaluator::ExpressionEvaluator(
    ExpressionEvaluator&&
) noexcept = default;

ExpressionEvaluator&
ExpressionEvaluator::operator=(
    ExpressionEvaluator&&
) noexcept = default;

double ExpressionEvaluator::value(
    double coordinate
) const noexcept {
    return impl_->value(coordinate);
}

double ExpressionEvaluator::derivative(
    double coordinate
) const noexcept {
    return impl_->derivative(coordinate);
}

const PotentialDefinition&
ExpressionEvaluator::definition() const noexcept {
    return impl_->definition;
}