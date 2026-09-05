#ifndef BROWNIAN_MOTOR_COMPACTDOUBLESPINBOX_H
#define BROWNIAN_MOTOR_COMPACTDOUBLESPINBOX_H
#pragma once

#include <QDoubleSpinBox>
#include <QString>

class CompactDoubleSpinBox final : public QDoubleSpinBox {
public:
    explicit CompactDoubleSpinBox(
        QWidget* parent = nullptr
    )
        : QDoubleSpinBox{parent}
    {}

protected:
    [[nodiscard]] QString textFromValue(
        double value
    ) const override {
        if (value == 0.0) {
            return "0";
        }

        return locale().toString(
            value,
            'g',
            decimals()
        );
    }
};

#endif // BROWNIAN_MOTOR_COMPACTDOUBLESPINBOX_H