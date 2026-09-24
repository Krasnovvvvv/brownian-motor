#ifndef BROWNIAN_MOTOR_POTENTIALDERIVATIVECHECK_H
#define BROWNIAN_MOTOR_POTENTIALDERIVATIVECHECK_H
#pragma once

#include <optional>
#include <string>

#include "core/PotentialDefinition.h"

class ExpressionEvaluator;

/*
 * Если проверка прошла, возвращает std::nullopt.
 *
 * Если найдена ошибка или нечисловое значение,
 * возвращает строку с объяснением.
 *
 * Проверка выполняется в нескольких точках;
 * она не является математическим доказательством
 * корректности производной на всей оси.
 */
[[nodiscard]] std::optional<std::string>
check_potential_derivative(
    const PotentialDefinition& definition,
    const ExpressionEvaluator& evaluator
);

#endif // BROWNIAN_MOTOR_POTENTIALDERIVATIVECHECK_H