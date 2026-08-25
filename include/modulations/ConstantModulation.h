#ifndef BROWNIAN_MOTOR_CONSTANTMODULATION_H
#define BROWNIAN_MOTOR_CONSTANTMODULATION_H
#pragma once

struct ConstantModulation {
    double value{1.0};

    double step() const noexcept {
        return value;
    }

    double current_factor() const noexcept {
        return value;
    }
};

#endif //BROWNIAN_MOTOR_CONSTANTMODULATION_H