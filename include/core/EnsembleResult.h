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

    /*
 * Fixed-coefficient control-variate estimate.
 * The control is free diffusion driven by the same
 * thermal increments over the measurement interval.
 */
    bool has_control_variate_statistics{false};

    double control_variate_velocity{
        std::numeric_limits<double>::quiet_NaN()
    };

    double control_variate_standard_error{
        std::numeric_limits<double>::quiet_NaN()
    };

    double control_variate_ci95_lower{
        std::numeric_limits<double>::quiet_NaN()
    };

    double control_variate_ci95_upper{
        std::numeric_limits<double>::quiet_NaN()
    };

    double integrated_drift_variance{
        std::numeric_limits<double>::quiet_NaN()
    };

    // Raw velocity minus control-variate velocity.
    double mean_thermal_velocity{
        std::numeric_limits<double>::quiet_NaN()
    };

};

#endif // BROWNIAN_MOTOR_ENSEMBLERESULT_H