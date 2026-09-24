#ifndef BROWNIAN_MOTOR_POTENTIALCONFIGPARSER_H
#define BROWNIAN_MOTOR_POTENTIALCONFIGPARSER_H
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

#include "core/PotentialDefinition.h"

struct PotentialConfigParseResult final {
    std::optional<PotentialDefinition> definition;

    QString error_message;

    QStringList warnings;

    [[nodiscard]] bool is_success() const noexcept {
        return definition.has_value();
    }
};

class PotentialConfigParser final {
public:
    
    [[nodiscard]] static PotentialConfigParseResult
    parse_file(
        const QString& file_name
    );

    [[nodiscard]] static PotentialConfigParseResult
    parse_json(
        const QByteArray& json_data,
        const QString& source_name = {}
    );
};

#endif // BROWNIAN_MOTOR_POTENTIALCONFIGPARSER_H