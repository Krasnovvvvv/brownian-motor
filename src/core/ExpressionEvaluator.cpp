#include "core/ExpressionEvaluator.h"

#include "core/PotentialDerivativeCheck.h"

#include <exprtk.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
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
        const std::string &identifier
    ) {
        if (
            identifier.empty() ||
            !is_identifier_start(identifier.front())
        ) {
            return false;
        }

        for (const char character: identifier) {
            if (!is_identifier_character(character)) {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool is_reserved_identifier(
        const std::string &identifier
    ) {
        /*
         * Сохраняем существующий список.
         *
         * Дополнительные ограничения на имена,
         * которые устанавливает сам ExprTk,
         * остаются ответственностью ExprTk.
         */
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
        const PotentialParameterDefinition &parameter
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
        const PotentialDefinition &definition
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

        for (
            const auto &parameter:
            definition.parameters
        ) {
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

        /*
         * Дополнительные проверки относятся только
         * к новым метаданным кусочного профиля.
         */
        if (definition.profile.piecewise) {
            if (definition.format_version != 2) {
                throw std::invalid_argument{
                    "Piecewise potential requires "
                    "format_version = 2."
                };
            }

            if (!definition.profile.periodic) {
                throw std::invalid_argument{
                    "Piecewise potential currently requires "
                    "periodic = true."
                };
            }
        } else if (
            !definition.profile
                    .breakpoint_expressions.empty()
        ) {
            throw std::invalid_argument{
                "Breakpoint expressions require "
                "piecewise mode."
            };
        }

        constexpr std::size_t maximum_breakpoints =
                4095;

        if (
            definition.profile
                    .breakpoint_expressions.size() >
            maximum_breakpoints
        ) {
            throw std::invalid_argument{
                "At most 4095 internal breakpoints "
                "are supported."
            };
        }
    }

    [[nodiscard]] double parameter_value(
        const PotentialParameterDefinition &parameter,
        const PotentialParameterValues &values
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
        const PotentialDefinition &definition,
        const PotentialParameterValues &values
    ) {
        std::unordered_set<std::string>
                declared_identifiers;

        for (
            const auto &parameter:
            definition.parameters
        ) {
            declared_identifiers.insert(
                parameter.id
            );
        }

        for (const auto &[id, value]: values) {
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

    [[nodiscard]] double wrap_coordinate(
        double coordinate,
        double period
    ) noexcept {
        if (!std::isfinite(coordinate)) {
            return
                    std::numeric_limits<double>::quiet_NaN();
        }

        if (
            coordinate >= 0.0 &&
            coordinate < period
        ) {
            return coordinate;
        }

        double phase = std::fmod(
            coordinate,
            period
        );

        if (phase < 0.0) {
            phase += period;
        }

        /*
         * Защита от округления при прибавлении period
         * к отрицательному остатку.
         */
        if (phase >= period) {
            phase = 0.0;
        }

        return phase;
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
        const PotentialDefinition &source_definition,
        const PotentialParameterValues &values
    )
        : definition{source_definition}
          , parameter_values{values} {
        validate_definition(definition);

        validate_parameter_values(
            definition,
            values
        );

        parameter_storage.reserve(
            definition.parameters.size()
        );

        for (
            const auto &parameter:
            definition.parameters
        ) {
            parameter_storage.push_back(
                parameter_value(
                    parameter,
                    values
                )
            );
        }

        /*
         * Основные выражения регистрируются так же,
         * как в исходной реализации.
         *
         * Параметры остаются переменными ExprTk:
         * замены add_variable на add_constant здесь нет.
         */
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

        /*
         * Дополнительная подготовка выполняется
         * только для кусочного режима.
         */
        if (definition.profile.piecewise) {
            resolve_breakpoints();
        }
    }

    Impl(const Impl &other)
        : Impl(
            other.definition,
            other.parameter_values
        ) {
    }

    [[nodiscard]] double value(
        double coordinate_value
    ) noexcept {
        coordinate = evaluation_coordinate(
            coordinate_value
        );

        return profile_expression.value();
    }

    [[nodiscard]] double derivative(
        double coordinate_value
    ) noexcept {
        coordinate = evaluation_coordinate(
            coordinate_value
        );

        return derivative_expression.value();
    }

    PotentialDefinition definition;
    PotentialParameterValues parameter_values;

    double coordinate{0.0};

    std::vector<double> parameter_storage;
    std::vector<double> resolved_breakpoints;

    SymbolTable symbol_table;
    Expression profile_expression;
    Expression derivative_expression;
    Parser parser;

private:
    [[nodiscard]] double evaluation_coordinate(
        double coordinate_value
    ) const noexcept {
        /*
         * Для существующих, некусочных определений
         * сохраняется прежняя передача координаты
         * без периодического приведения.
         */
        if (!definition.profile.piecewise) {
            return coordinate_value;
        }

        return wrap_coordinate(
            coordinate_value,
            definition.profile.period
        );
    }

    void resolve_breakpoints() {
        /*
         * Отдельная таблица символов:
         *
         * - содержит выбранные значения параметров;
         * - содержит стандартные константы;
         * - НЕ содержит пространственную координату.
         *
         * Константность параметров здесь относится
         * только к выражениям границ, а не к основным
         * выражениям потенциала и производной.
         */
        SymbolTable breakpoint_symbol_table;

        for (
            std::size_t index = 0;
            index < definition.parameters.size();
            ++index
        ) {
            const std::string &id =
                    definition.parameters[index].id;

            if (
                !breakpoint_symbol_table.add_constant(
                    id,
                    parameter_storage[index]
                )
            ) {
                throw std::invalid_argument{
                    "Cannot register parameter '" +
                    id +
                    "' for breakpoint expressions."
                };
            }
        }

        breakpoint_symbol_table.add_constants();

        resolved_breakpoints.reserve(
            definition.profile
                    .breakpoint_expressions.size()
        );

        double previous_breakpoint = 0.0;

        for (
            const std::string &expression_text:
            definition.profile.breakpoint_expressions
        ) {
            Expression breakpoint_expression;

            breakpoint_expression.register_symbol_table(
                breakpoint_symbol_table
            );

            Parser breakpoint_parser;

            if (
                !breakpoint_parser.compile(
                    expression_text,
                    breakpoint_expression
                )
            ) {
                throw std::invalid_argument{
                    "Cannot compile parameter-only "
                    "breakpoint expression '" +
                    expression_text +
                    "': " +
                    breakpoint_parser.error()
                };
            }

            const double breakpoint =
                    breakpoint_expression.value();

            if (!std::isfinite(breakpoint)) {
                throw std::invalid_argument{
                    "Breakpoint expression '" +
                    expression_text +
                    "' produced a non-finite value."
                };
            }

            if (
                breakpoint <= previous_breakpoint ||
                breakpoint >= definition.profile.period
            ) {
                throw std::invalid_argument{
                    "Breakpoints must satisfy "
                    "0 < b1 < b2 < ... < period. "
                    "Invalid breakpoint expression: '" +
                    expression_text +
                    "'."
                };
            }

            resolved_breakpoints.push_back(
                breakpoint
            );

            previous_breakpoint = breakpoint;
        }
    }
};

ExpressionEvaluator::ExpressionEvaluator(
    const PotentialDefinition &definition,
    const PotentialParameterValues &parameter_values
)
    : impl_{
        std::make_unique<Impl>(
            definition,
            parameter_values
        )
    } {
    /*
     * Для кусочного профиля проверка выполняется
     * с фактическими параметрами этого экземпляра,
     * в том числе выбранными пользователем в UI.
     *
     * Для существующего некусочного режима
     * поведение конструктора не меняется:
     * прежняя проверка при загрузке остаётся
     * в PotentialConfigParser.
     */
    if (piecewise()) {
        if (
            const auto derivative_error =
                    check_potential_derivative(
                        impl_->definition,
                        *this
                    )
        ) {
            throw std::invalid_argument{
                *derivative_error
            };
        }
    }
}

ExpressionEvaluator::~ExpressionEvaluator() = default;

ExpressionEvaluator::ExpressionEvaluator(
    const ExpressionEvaluator &other
)
    : impl_{
        std::make_unique<Impl>(
            *other.impl_
        )
    } {
}

ExpressionEvaluator &
ExpressionEvaluator::operator=(
    const ExpressionEvaluator &other
) {
    if (this == &other) {
        return *this;
    }

    impl_ = std::make_unique<Impl>(
        *other.impl_
    );

    return *this;
}

ExpressionEvaluator::ExpressionEvaluator(
    ExpressionEvaluator &&
) noexcept = default;

ExpressionEvaluator &
ExpressionEvaluator::operator=(
    ExpressionEvaluator &&
) noexcept = default;

double ExpressionEvaluator::operator()(
    double coordinate
) const noexcept {
    return value(coordinate);
}

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

const PotentialDefinition &
ExpressionEvaluator::definition() const noexcept {
    return impl_->definition;
}

bool ExpressionEvaluator::piecewise() const noexcept {
    return impl_->definition.profile.piecewise;
}

const std::vector<double> &
ExpressionEvaluator::breakpoints() const noexcept {
    return impl_->resolved_breakpoints;
}