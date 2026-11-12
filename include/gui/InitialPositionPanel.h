#ifndef BROWNIAN_MOTOR_INITIALPOSITIONPANEL_H
#define BROWNIAN_MOTOR_INITIALPOSITIONPANEL_H
#pragma once

#include "core/InitialPositionDistribution.h"
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class InitialPositionPanel final : public QWidget {
public:
    explicit InitialPositionPanel(
        QDoubleSpinBox *point_position,
        QWidget *parent = nullptr
    );
    [[nodiscard]] InitialPositionDistribution settings() const;
    void set_period_available(bool available, double period);

private:
    void update_fields_();
    QComboBox *kind_{};
    QDoubleSpinBox *point_{};
    QDoubleSpinBox *lower_{};
    QDoubleSpinBox *upper_{};
    QDoubleSpinBox *mean_{};
    QDoubleSpinBox *sigma_{};
    QLabel *point_label_{};
    QLabel *lower_label_{};
    QLabel *upper_label_{};
    QLabel *mean_label_{};
    QLabel *sigma_label_{};
    QPushButton *one_period_{};
    bool period_available_{false};
    double period_{1.0};
};
#endif
