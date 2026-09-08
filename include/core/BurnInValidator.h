#ifndef BROWNIAN_MOTOR_BURNINVALIDATOR_H
#define BROWNIAN_MOTOR_BURNINVALIDATOR_H
#pragma once

#include <cstddef>
#include <vector>

struct BurnInSample {
    double time{0.0};
    double mean_x{0.0};
};

struct BurnInValidationResult {
    bool valid{false};
    bool stable{false};

    std::size_t start_index{0};
    std::size_t recommended_burn_in_steps{0};

    double start_time{0.0};

    double early_velocity{0.0};
    double late_velocity{0.0};
    double tail_velocity{0.0};

    double relative_velocity_difference{0.0};
    double tail_r_squared{0.0};

    double tolerance{0.25};
};

class BurnInValidator final {
public:
    [[nodiscard]] static BurnInValidationResult validate(
        const std::vector<BurnInSample>& samples,
        double dt,
        double velocity_tolerance
    );
};

#endif // BROWNIAN_MOTOR_BURNINVALIDATOR_H