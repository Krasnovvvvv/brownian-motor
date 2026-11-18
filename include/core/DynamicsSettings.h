#ifndef BROWNIAN_MOTOR_DYNAMICSSETTINGS_H
#define BROWNIAN_MOTOR_DYNAMICSSETTINGS_H
#pragma once
#include <cmath>
#include <stdexcept>

enum class DynamicsModel { Overdamped, Inertial };

enum class InitialVelocityKind { Zero, Thermal };

struct DynamicsSettings final {
    DynamicsModel model{DynamicsModel::Overdamped};
    double inertia_ratio{0.1};
    InitialVelocityKind initial_velocity{InitialVelocityKind::Thermal};
    [[nodiscard]] bool inertial() const noexcept { return model == DynamicsModel::Inertial; }

    void validate() const {
        if (model != DynamicsModel::Overdamped && model != DynamicsModel::Inertial)
            throw std::invalid_argument("Unknown dynamics model");
        if (!inertial()) return;
        if (!std::isfinite(inertia_ratio) || inertia_ratio <= 0)
            throw std::invalid_argument("Inertia epsilon must be finite and positive");
        if (initial_velocity != InitialVelocityKind::Zero && initial_velocity != InitialVelocityKind::Thermal)
            throw std::invalid_argument("Unknown initial velocity distribution");
    }
};
#endif
