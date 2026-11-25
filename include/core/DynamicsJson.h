#ifndef BROWNIAN_MOTOR_DYNAMICSJSON_H
#define BROWNIAN_MOTOR_DYNAMICSJSON_H
#pragma once
#include "core/DynamicsSettings.h"
#include <QJsonObject>
#include <QString>

[[nodiscard]] inline QJsonObject dynamics_to_json(const DynamicsSettings &d) {
    QJsonObject j{{"model", d.inertial() ? QStringLiteral("inertial") : QStringLiteral("overdamped")}};
    if (d.inertial()) {
        j.insert("integrator",QStringLiteral("exponential_force_predictor_corrector_joint_xv"));
        j.insert("inertia_ratio", d.inertia_ratio);
        j.insert("initial_velocity", d.initial_velocity == InitialVelocityKind::Thermal
                                         ? QStringLiteral("thermal")
                                         : QStringLiteral("zero"));
    }
    return j;
}
#endif
