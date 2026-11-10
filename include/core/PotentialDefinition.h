#ifndef BROWNIAN_MOTOR_POTENTIALDEFINITION_H
#define BROWNIAN_MOTOR_POTENTIALDEFINITION_H
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct PotentialCoordinateDefinition final {
    std::string symbol{"x"};
    std::string unit{"dimensionless"};
};

struct PotentialProfileDefinition final {
    std::string expression;
    std::string derivative_expression;
    bool periodic{false};
    double period{0.0};

    // Version 2 only. An explicit empty breakpoint array still enables
    // piecewise mode (a derivative jump at the periodic seam is allowed).
    bool piecewise{false};
    // Parameter-only expressions, in strictly increasing evaluated order.
    std::vector<std::string> breakpoint_expressions;
};

struct PotentialParameterDefinition final {
    std::string id;
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
    std::vector<PotentialParameterDefinition> parameters;
};

using PotentialParameterValues = std::unordered_map<std::string, double>;

#endif
