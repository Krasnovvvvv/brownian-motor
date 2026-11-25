#ifndef BROWNIAN_MOTOR_DYNAMICSPANEL_H
#define BROWNIAN_MOTOR_DYNAMICSPANEL_H
#pragma once
#include "core/DynamicsSettings.h"
#include <QWidget>
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QFormLayout;

class DynamicsPanel final : public QWidget {
public:
    explicit DynamicsPanel(QWidget *parent = nullptr);

    [[nodiscard]] DynamicsSettings settings() const;

    [[nodiscard]] QFormLayout *form_layout() const noexcept;

private:
    void update_fields_();

    QComboBox *model_{}, *velocity_{};
    QDoubleSpinBox *epsilon_{};
    QLabel *epsilon_label_{}, *velocity_label_{};
};
#endif
