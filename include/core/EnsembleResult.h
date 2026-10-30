#ifndef BROWNIAN_MOTOR_ENSEMBLERESULT_H
#define BROWNIAN_MOTOR_ENSEMBLERESULT_H
#pragma once

#include <cstddef>
#include <vector>
#include <limits>

struct EnsembleResult final {
    std::vector<double> sim_time;
    std::vector<double> mean_x;

    std::size_t n_particles{0};
    bool has_trajectory{false};

    double mean_velocity{0.0};
    double mean_x_final{0.0};

    /*
 * Statistics of unwrapped particle displacements
 * over the measurement interval, excluding burn-in.
 *
 * Valid only for a completed run with at least
 * two particles.
 */
    bool has_velocity_statistics{false};

    double measurement_time{0.0};

    double mean_displacement{
        std::numeric_limits<double>::quiet_NaN()
    };

    // Sample variance, denominator N - 1.
    double displacement_variance{
        std::numeric_limits<double>::quiet_NaN()
    };

    // Mean of squared displacements, denominator N.
    double mean_squared_displacement{
        std::numeric_limits<double>::quiet_NaN()
    };

    double velocity_standard_error{
        std::numeric_limits<double>::quiet_NaN()
    };

    // Approximate large-sample normal interval.
    double velocity_ci95_lower{
        std::numeric_limits<double>::quiet_NaN()
    };

    double velocity_ci95_upper{
        std::numeric_limits<double>::quiet_NaN()
    };

};

#endif // BROWNIAN_MOTOR_ENSEMBLERESULT_H