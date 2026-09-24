#ifndef BROWNIAN_MOTOR_POTENTIALDEFINITION_H
#define BROWNIAN_MOTOR_POTENTIALDEFINITION_H
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct PotentialCoordinateDefinition final {
    /*
     * Имя единственной пространственной переменной,
     * которую пользователь использует в expressions.
     *
     * Примеры:
     * x
     * z
     * q
     * position
     */
    std::string symbol{"x"};

    std::string unit{"dimensionless"};
};

struct PotentialProfileDefinition final {
    /*
     * Базовый spatial profile U(coordinate).
     *
     * Factor модуляции здесь НЕ хранится:
     * он применяется существующим Potential<Profile>.
     */
    std::string expression;

    /*
     * Аналитическая производная:
     *
     * dU / d(coordinate).
     */
    std::string derivative_expression;

    bool periodic{false};
    double period{0.0};
};

struct PotentialParameterDefinition final {
    /*
     * Техническое имя переменной в expression.
     *
     * Примеры:
     * V1
     * V2
     * amplitude
     * sigma
     * exponent
     */
    std::string id;

    /*
     * Человеко-читаемая подпись в UI.
     *
     * Примеры:
     * V₁
     * Barrier height
     * Width σ
     */
    std::string label;

    std::string description;
    std::string unit{"dimensionless"};

    double default_value{0.0};
    double minimum{0.0};
    double maximum{0.0};
    double step{0.1};

    int decimals{6};
};

struct PotentialDefinition final {
    int format_version{1};

    std::string source_file_path;

    std::string id;
    std::string name;
    std::string description;

    PotentialCoordinateDefinition coordinate;
    PotentialProfileDefinition profile;

    std::vector<PotentialParameterDefinition>
        parameters;
};

/*
 * Значения параметров, которые фактически выбраны
 * пользователем в UI перед запуском simulation.
 *
 * Например:
 *
 * {
 *     {"V1", 0.2},
 *     {"V2", 0.1}
 * }
 */
using PotentialParameterValues =
    std::unordered_map<std::string, double>;

#endif // BROWNIAN_MOTOR_POTENTIALDEFINITION_H