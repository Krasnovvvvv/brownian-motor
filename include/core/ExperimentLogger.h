#ifndef BROWNIAN_MOTOR_EXPERIMENTLOGGER_H
#define BROWNIAN_MOTOR_EXPERIMENTLOGGER_H
#pragma once

#include <QJsonObject>
#include <QString>

class ExperimentLogger final {
public:
    ExperimentLogger();

    [[nodiscard]] QString log_file_path() const;

    [[nodiscard]] bool write_event(
        const QString& event_name,
        const QJsonObject& data = {}
    );

    [[nodiscard]] QString last_error() const;

private:
    [[nodiscard]] QString create_log_file_path_() const;

    QString log_file_path_;
    QString last_error_;
};

#endif // BROWNIAN_MOTOR_EXPERIMENTLOGGER_H