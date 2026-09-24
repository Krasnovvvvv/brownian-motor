#ifndef BROWNIAN_MOTOR_POTENTIALDROPAREA_H
#define BROWNIAN_MOTOR_POTENTIALDROPAREA_H
#pragma once

#include <QFrame>
#include <QString>

class QDragEnterEvent;
class QDropEvent;
class QLabel;

class PotentialDropArea final : public QFrame {
    Q_OBJECT

public:
    explicit PotentialDropArea(
        QWidget* parent = nullptr
    );

    void set_message(
        const QString& message,
        const QString& details = {}
    );

    signals:
        void file_dropped(
            const QString& file_name
        );

protected:
    void dragEnterEvent(
        QDragEnterEvent* event
    ) override;

    void dropEvent(
        QDropEvent* event
    ) override;

private:
    QLabel* message_label_{nullptr};
};

#endif // BROWNIAN_MOTOR_POTENTIALDROPAREA_H