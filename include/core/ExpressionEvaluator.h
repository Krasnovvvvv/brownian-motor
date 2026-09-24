#ifndef BROWNIAN_MOTOR_EXPRESSIONEVALUATOR_H
#define BROWNIAN_MOTOR_EXPRESSIONEVALUATOR_H
#pragma once

#include <memory>

#include "core/PotentialDefinition.h"

class ExpressionEvaluator final {
public:
    explicit ExpressionEvaluator(
        const PotentialDefinition& definition,
        const PotentialParameterValues& parameter_values = {}
    );

    ~ExpressionEvaluator();

    ExpressionEvaluator(
        const ExpressionEvaluator& other
    );

    ExpressionEvaluator& operator=(
        const ExpressionEvaluator& other
    );

    ExpressionEvaluator(
        ExpressionEvaluator&&
    ) noexcept;

    ExpressionEvaluator& operator=(
        ExpressionEvaluator&&
    ) noexcept;

    /*
    * Позволяет использовать ExpressionEvaluator
    * как SpatialProfile в Potential<Profile>.
    *
    *  Profile(coordinate) == U(coordinate).
    */
    [[nodiscard]] double operator()(
        double coordinate
    ) const noexcept;

    /*
     * Возвращает U(coordinate).
     */
    [[nodiscard]] double value(
        double coordinate
    ) const noexcept;

    /*
     * Возвращает dU/d(coordinate).
     */
    [[nodiscard]] double derivative(
        double coordinate
    ) const noexcept;

    [[nodiscard]] const PotentialDefinition&
    definition() const noexcept;

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

#endif // BROWNIAN_MOTOR_EXPRESSIONEVALUATOR_H