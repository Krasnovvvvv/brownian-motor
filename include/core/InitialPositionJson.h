#ifndef BROWNIAN_MOTOR_INITIALPOSITIONJSON_H
#define BROWNIAN_MOTOR_INITIALPOSITIONJSON_H
#pragma once
#include "core/InitialPositionDistribution.h"
#include <QJsonObject>
#include <QString>

[[nodiscard]] inline QJsonObject initial_position_to_json(
    const InitialPositionDistribution &settings, double point
) {
    QJsonObject result{{"kind", QString::fromLatin1(settings.name())}};
    switch (settings.kind) {
        case InitialPositionKind::Point:
            result.insert("position", point);
            break;
        case InitialPositionKind::Uniform:
            result.insert("from", settings.lower);
            result.insert("to", settings.upper);
            break;
        case InitialPositionKind::Gaussian:
            result.insert("mean", settings.mean);
            result.insert("standard_deviation", settings.standard_deviation);
            break;
    }
    return result;
}
#endif
