#include "core/PotentialConfigParser.h"
#include "core/ExpressionEvaluator.h"
#include "core/PotentialDerivativeCheck.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr int supported_format_version = 1;

constexpr const char* expected_format =
    "brownian-motor-potential";

[[nodiscard]] PotentialConfigParseResult fail(
    QString message
) {
    return PotentialConfigParseResult{
        .definition = std::nullopt,
        .error_message = std::move(message),
        .warnings = {}
    };
}

[[nodiscard]] bool require_string(
    const QJsonObject& object,
    const QString& key,
    QString& destination,
    QString& error_message,
    const QString& context
) {
    if (!object.contains(key)) {
        error_message = QString{
            "%1: required field '%2' is missing."
        }
            .arg(context, key);

        return false;
    }

    const QJsonValue value = object.value(key);

    if (!value.isString()) {
        error_message = QString{
            "%1: field '%2' must be a string."
        }
            .arg(context, key);

        return false;
    }

    destination = value.toString();

    if (destination.trimmed().isEmpty()) {
        error_message = QString{
            "%1: field '%2' must not be empty."
        }
            .arg(context, key);

        return false;
    }

    return true;
}

[[nodiscard]] bool optional_string(
    const QJsonObject& object,
    const QString& key,
    QString& destination,
    QString& error_message,
    const QString& context
) {
    if (!object.contains(key)) {
        return true;
    }

    const QJsonValue value = object.value(key);

    if (!value.isString()) {
        error_message = QString{
            "%1: optional field '%2' must be a string."
        }
            .arg(context, key);

        return false;
    }

    destination = value.toString();

    return true;
}

[[nodiscard]] bool require_number(
    const QJsonObject& object,
    const QString& key,
    double& destination,
    QString& error_message,
    const QString& context
) {
    if (!object.contains(key)) {
        error_message = QString{
            "%1: required field '%2' is missing."
        }
            .arg(context, key);

        return false;
    }

    const QJsonValue value = object.value(key);

    if (!value.isDouble()) {
        error_message = QString{
            "%1: field '%2' must be a number."
        }
            .arg(context, key);

        return false;
    }

    destination = value.toDouble();

    if (!std::isfinite(destination)) {
        error_message = QString{
            "%1: field '%2' must be finite."
        }
            .arg(context, key);

        return false;
    }

    return true;
}

[[nodiscard]] bool require_integer(
    const QJsonObject& object,
    const QString& key,
    int& destination,
    QString& error_message,
    const QString& context
) {
    double value = 0.0;

    if (
        !require_number(
            object,
            key,
            value,
            error_message,
            context
        )
    ) {
        return false;
    }

    const double rounded_value =
        std::round(value);

    if (
        std::abs(value - rounded_value) >
        1.0e-12
    ) {
        error_message = QString{
            "%1: field '%2' must be an integer."
        }
            .arg(context, key);

        return false;
    }

    constexpr double minimum_int =
        static_cast<double>(
            std::numeric_limits<int>::min()
        );

    constexpr double maximum_int =
        static_cast<double>(
            std::numeric_limits<int>::max()
        );

    if (
        rounded_value < minimum_int ||
        rounded_value > maximum_int
    ) {
        error_message = QString{
            "%1: field '%2' is outside int range."
        }
            .arg(context, key);

        return false;
    }

    destination = static_cast<int>(
        rounded_value
    );

    return true;
}

[[nodiscard]] bool require_bool(
    const QJsonObject& object,
    const QString& key,
    bool& destination,
    QString& error_message,
    const QString& context
) {
    if (!object.contains(key)) {
        error_message = QString{
            "%1: required field '%2' is missing."
        }
            .arg(context, key);

        return false;
    }

    const QJsonValue value = object.value(key);

    if (!value.isBool()) {
        error_message = QString{
            "%1: field '%2' must be true or false."
        }
            .arg(context, key);

        return false;
    }

    destination = value.toBool();

    return true;
}

[[nodiscard]] bool parse_parameter(
    const QJsonObject& object,
    std::size_t index,
    PotentialParameterDefinition& parameter,
    QString& error_message
) {
    const QString context = QString{
        "parameters[%1]"
    }.arg(
        static_cast<qulonglong>(index)
    );

    QString id;
    QString label;
    QString description;
    QString unit{"dimensionless"};

    if (
        !require_string(
            object,
            "id",
            id,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !require_string(
            object,
            "label",
            label,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !optional_string(
            object,
            "description",
            description,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !optional_string(
            object,
            "unit",
            unit,
            error_message,
            context
        )
    ) {
        return false;
    }

    parameter.id = id.toStdString();
    parameter.label = label.toStdString();
    parameter.description =
        description.toStdString();

    parameter.unit = unit.toStdString();

    if (
        !require_number(
            object,
            "default",
            parameter.default_value,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !require_number(
            object,
            "minimum",
            parameter.minimum,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !require_number(
            object,
            "maximum",
            parameter.maximum,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !require_number(
            object,
            "step",
            parameter.step,
            error_message,
            context
        )
    ) {
        return false;
    }

    if (
        !require_integer(
            object,
            "decimals",
            parameter.decimals,
            error_message,
            context
        )
    ) {
        return false;
    }

    return true;
}

[[nodiscard]] bool validate_expression_values(
    const PotentialDefinition& definition,
    QString& error_message
) {
    try {
        const ExpressionEvaluator evaluator{
            definition
        };

        /*
         * Эти точки не доказывают корректность функции
         * на всей вещественной оси, но ловят типичные
         * ошибки:
         *
         * - sqrt(x) при x < 0;
         * - x^p при отрицательном x и дробном p;
         * - division by zero;
         * - NaN / infinity;
         */
        constexpr std::array<double, 7>
            validation_coordinates{
                -2.0,
                -1.0,
                -0.5,
                0.0,
                0.5,
                1.0,
                2.0
            };

        for (
            const double coordinate :
            validation_coordinates
        ) {
            const double profile_value =
                evaluator.value(coordinate);

            if (!std::isfinite(profile_value)) {
                error_message = QString{
                    "Potential expression produced a "
                    "non-finite value at %1 = %2."
                }
                    .arg(
                        QString::fromStdString(
                            definition.coordinate.symbol
                        )
                    )
                    .arg(
                        coordinate,
                        0,
                        'g',
                        12
                    );

                return false;
            }

            const double derivative_value =
                evaluator.derivative(coordinate);

            if (!std::isfinite(derivative_value)) {
                error_message = QString{
                    "Derivative expression produced a "
                    "non-finite value at %1 = %2."
                }
                    .arg(
                        QString::fromStdString(
                            definition.coordinate.symbol
                        )
                    )
                    .arg(
                        coordinate,
                        0,
                        'g',
                        12
                    );

                return false;
            }
        }
        if (
            const auto derivative_error =
                check_potential_derivative(
                    definition,
                    evaluator
                )
        ) {
            error_message =
                QString::fromStdString(
                    *derivative_error
                );

            return false;
        }

    } catch (const std::exception& exception) {
        error_message = QString{
            "Invalid profile expression: %1"
        }.arg(
            QString::fromUtf8(
                exception.what()
            )
        );

        return false;
    }

    return true;
}

} // namespace

PotentialConfigParseResult
PotentialConfigParser::parse_file(
    const QString& file_name
) {
    const QFileInfo file_info{file_name};

    if (!file_info.exists()) {
        return fail(
            QString{
                "Potential file does not exist:\n%1"
            }.arg(
                QDir::toNativeSeparators(
                    file_name
                )
            )
        );
    }

    if (!file_info.isFile()) {
        return fail(
            QString{
                "Potential path is not a file:\n%1"
            }.arg(
                QDir::toNativeSeparators(
                    file_name
                )
            )
        );
    }

    QFile file{file_info.absoluteFilePath()};

    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            QString{
                "Cannot open potential file:\n%1\n\n%2"
            }
                .arg(
                    QDir::toNativeSeparators(
                        file_info.absoluteFilePath()
                    )
                )
                .arg(
                    file.errorString()
                )
        );
    }

    const QByteArray json_data =
        file.readAll();

    PotentialConfigParseResult result =
        parse_json(
            json_data,
            file_info.absoluteFilePath()
        );

    if (result.definition) {
        result.definition->source_file_path =
            file_info.absoluteFilePath()
                .toStdString();
    }

    return result;
}

PotentialConfigParseResult
PotentialConfigParser::parse_json(
    const QByteArray& json_data,
    const QString& source_name
) {
    QJsonParseError parse_error;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            json_data,
            &parse_error
        );

    if (
        parse_error.error !=
        QJsonParseError::NoError
    ) {
        const QString source_prefix =
            source_name.isEmpty()
                ? QString{}
                : QString{
                    "%1: "
                }.arg(source_name);

        return fail(
            QString{
                "%1Invalid JSON at offset %2: %3"
            }
                .arg(source_prefix)
                .arg(
                    parse_error.offset
                )
                .arg(
                    parse_error.errorString()
                )
        );
    }

    if (!document.isObject()) {
        return fail(
            "Potential configuration root "
            "must be a JSON object."
        );
    }

    const QJsonObject root =
        document.object();

    QString error_message;

    QString format;

    if (
        !require_string(
            root,
            "format",
            format,
            error_message,
            "root"
        )
    ) {
        return fail(error_message);
    }

    if (format != expected_format) {
        return fail(
            QString{
                "Unsupported potential format '%1'. "
                "Expected '%2'."
            }
                .arg(format)
                .arg(expected_format)
        );
    }

    int format_version = 0;

    if (
        !require_integer(
            root,
            "format_version",
            format_version,
            error_message,
            "root"
        )
    ) {
        return fail(error_message);
    }

    if (
        format_version !=
        supported_format_version
    ) {
        return fail(
            QString{
                "Unsupported potential format version %1. "
                "Supported version: %2."
            }
                .arg(format_version)
                .arg(supported_format_version)
        );
    }

    PotentialDefinition definition;

    definition.format_version =
        format_version;

    QString id;
    QString name;
    QString description;

    if (
        !require_string(
            root,
            "id",
            id,
            error_message,
            "root"
        )
    ) {
        return fail(error_message);
    }

    if (
        !require_string(
            root,
            "name",
            name,
            error_message,
            "root"
        )
    ) {
        return fail(error_message);
    }

    if (
        !optional_string(
            root,
            "description",
            description,
            error_message,
            "root"
        )
    ) {
        return fail(error_message);
    }

    definition.id = id.toStdString();
    definition.name = name.toStdString();
    definition.description =
        description.toStdString();

    if (!root.contains("coordinate")) {
        return fail(
            "root: required object 'coordinate' is missing."
        );
    }

    const QJsonValue coordinate_value =
        root.value("coordinate");

    if (!coordinate_value.isObject()) {
        return fail(
            "root.coordinate must be an object."
        );
    }

    const QJsonObject coordinate_object =
        coordinate_value.toObject();

    QString coordinate_symbol;
    QString coordinate_unit{"dimensionless"};

    if (
        !require_string(
            coordinate_object,
            "symbol",
            coordinate_symbol,
            error_message,
            "coordinate"
        )
    ) {
        return fail(error_message);
    }

    if (
        !optional_string(
            coordinate_object,
            "unit",
            coordinate_unit,
            error_message,
            "coordinate"
        )
    ) {
        return fail(error_message);
    }

    definition.coordinate.symbol =
        coordinate_symbol.toStdString();

    definition.coordinate.unit =
        coordinate_unit.toStdString();

    if (!root.contains("profile")) {
        return fail(
            "root: required object 'profile' is missing."
        );
    }

    const QJsonValue profile_value =
        root.value("profile");

    if (!profile_value.isObject()) {
        return fail(
            "root.profile must be an object."
        );
    }

    const QJsonObject profile_object =
        profile_value.toObject();

    QString expression;
    QString derivative_expression;

    if (
        !require_string(
            profile_object,
            "expression",
            expression,
            error_message,
            "profile"
        )
    ) {
        return fail(error_message);
    }

    if (
        !require_string(
            profile_object,
            "derivative_expression",
            derivative_expression,
            error_message,
            "profile"
        )
    ) {
        return fail(error_message);
    }

    definition.profile.expression =
        expression.toStdString();

    definition.profile.derivative_expression =
        derivative_expression.toStdString();

    if (!profile_object.contains("periodic")) {
        definition.profile.periodic = false;
    } else if (
        !require_bool(
            profile_object,
            "periodic",
            definition.profile.periodic,
            error_message,
            "profile"
        )
    ) {
        return fail(error_message);
    }

    if (definition.profile.periodic) {
        if (
            !require_number(
                profile_object,
                "period",
                definition.profile.period,
                error_message,
                "profile"
            )
        ) {
            return fail(error_message);
        }
    }

    if (!root.contains("parameters")) {
        return fail(
            "root: required array 'parameters' is missing."
        );
    }

    const QJsonValue parameters_value =
        root.value("parameters");

    if (!parameters_value.isArray()) {
        return fail(
            "root.parameters must be an array."
        );
    }

    const QJsonArray parameters_array =
        parameters_value.toArray();

    definition.parameters.reserve(
        static_cast<std::size_t>(
            parameters_array.size()
        )
    );

    for (
        qsizetype index = 0;
        index < parameters_array.size();
        ++index
    ) {
        const QJsonValue parameter_value =
            parameters_array.at(index);

        if (!parameter_value.isObject()) {
            return fail(
                QString{
                    "parameters[%1] must be an object."
                }.arg(index)
            );
        }

        PotentialParameterDefinition parameter;

        if (
            !parse_parameter(
                parameter_value.toObject(),
                static_cast<std::size_t>(index),
                parameter,
                error_message
            )
        ) {
            return fail(error_message);
        }

        definition.parameters.push_back(
            std::move(parameter)
        );
    }

    if (
        !validate_expression_values(
            definition,
            error_message
        )
    ) {
        return fail(error_message);
    }

    return PotentialConfigParseResult{
        .definition = std::move(definition),
        .error_message = {},
        .warnings = {}
    };
}