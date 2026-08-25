#ifndef BROWNIAN_MOTOR_ENSEMBLERESULT_H
#define BROWNIAN_MOTOR_ENSEMBLERESULT_H
#pragma once

#include <cstddef>
#include <vector>

struct EnsembleResult final {
    std::vector<double> sim_time;
    std::vector<double> mean_x;

    std::size_t n_particles{0};
    bool has_trajectory{false};

    double mean_velocity{0.0};
    double mean_x_final{0.0};
};

#endif // BROWNIAN_MOTOR_ENSEMBLERESULT_H