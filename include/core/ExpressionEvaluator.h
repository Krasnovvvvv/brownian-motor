#ifndef BROWNIAN_MOTOR_EXPRESSIONEVALUATOR_H
#define BROWNIAN_MOTOR_EXPRESSIONEVALUATOR_H
#pragma once

#include <memory>
#include <vector>
#include "core/PotentialDefinition.h"   

class ExpressionEvaluator final {
public:
    explicit ExpressionEvaluator(
        const PotentialDefinition &definition,
        const PotentialParameterValues &parameter_values = {}
    );
    ~ExpressionEvaluator();
    ExpressionEvaluator(const ExpressionEvaluator &other);
    ExpressionEvaluator &operator=(const ExpressionEvaluator &other);
    ExpressionEvaluator(ExpressionEvaluator &&) noexcept;
    ExpressionEvaluator &operator=(ExpressionEvaluator &&) noexcept;

    [[nodiscard]] double operator()(double coordinate) const noexcept;
    [[nodiscard]] double value(double coordinate) const noexcept;
    [[nodiscard]] double derivative(double coordinate) const noexcept;
    [[nodiscard]] const PotentialDefinition &definition() const noexcept;

    // Resolved using the actual parameter values supplied to this evaluator.
    [[nodiscard]] bool piecewise() const noexcept;
    [[nodiscard]] const std::vector<double> &breakpoints() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
