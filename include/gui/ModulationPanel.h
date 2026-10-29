#ifndef BROWNIAN_MOTOR_MODULATIONPANEL_H
#define BROWNIAN_MOTOR_MODULATIONPANEL_H
#pragma once

#include <QJsonObject>
#include <QWidget>
#include "modulations/ModulationSettings.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QStackedWidget;

class ModulationPanel final : public QWidget {
public:
    explicit ModulationPanel(QWidget* parent = nullptr);
    [[nodiscard]] ModulationSettings settings() const;
    [[nodiscard]] static QJsonObject settings_json(const ModulationSettings& settings);

private:
    void update_pages_();
    void update_noise_summary_();
    QComboBox* category_{};
    QComboBox* deterministic_kind_{};
    QWidget* deterministic_selector_{};
    QStackedWidget* pages_{};
    QDoubleSpinBox* factor_a_{};
    QDoubleSpinBox* factor_b_{};
    QDoubleSpinBox* lifetime_a_{};
    QDoubleSpinBox* lifetime_b_{};
    QLabel* noise_summary_{};
    QDoubleSpinBox* constant_factor_{};
    QDoubleSpinBox* sine_offset_{};
    QDoubleSpinBox* sine_amplitude_{};
    QDoubleSpinBox* sine_period_{};
    QDoubleSpinBox* sine_phase_{};
};
#endif
