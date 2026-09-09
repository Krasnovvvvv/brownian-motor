#include "core/ExperimentLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTextStream>

ExperimentLogger::ExperimentLogger()
    : log_file_path_{create_log_file_path_()}
{
}

QString ExperimentLogger::log_file_path() const {
    return log_file_path_;
}

bool ExperimentLogger::write_event(
    const QString& event_name,
    const QJsonObject& data
) {
    QJsonObject event = data;

    event.insert(
        "timestamp",
        QDateTime::currentDateTime().toString(
            Qt::ISODateWithMs
        )
    );

    event.insert(
        "event",
        event_name
    );

    QFile file{log_file_path_};

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Append |
            QIODevice::Text
        )) {
        last_error_ = file.errorString();
        return false;
        }

    QTextStream stream{&file};

    stream << QJsonDocument{event}.toJson(
        QJsonDocument::Compact
    );

    stream << Qt::endl;

    last_error_.clear();

    return true;
}

QString ExperimentLogger::last_error() const {
    return last_error_;
}

QString ExperimentLogger::create_log_file_path_() const {
    const QString data_directory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );

    QDir directory{data_directory};

    if (!directory.exists()) {
        directory.mkpath(".");
    }

    const QString file_name = QString{
        "brownian-motor-%1.jsonl"
    }.arg(
        QDateTime::currentDateTime().toString(
            "yyyy-MM-dd_HH-mm-ss-zzz"
        )
    );

    return directory.filePath(file_name);
}