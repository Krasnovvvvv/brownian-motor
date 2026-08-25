#ifndef BROWNIAN_MOTOR_SIMULATIONUPDATE_H
#define BROWNIAN_MOTOR_SIMULATIONUPDATE_H
#pragma once

#include <cstddef>

struct SimulationUpdate final {
    std::size_t step{};
    std::size_t total_steps{};

    double time{};
    double mean_x{};
    double mean_velocity{};

    [[nodiscard]] double progress() const noexcept {
        if (total_steps == 0) {
            return 0.0;
        }

        return static_cast<double>(step + 1) /
               static_cast<double>(total_steps);
    }
};

#endif // BROWNIAN_MOTOR_SIMULATIONUPDATE_H