#include "gui/SimulationWorker.h"

#include <QString>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>
#include <numbers>

#include "modulations/ConstantModulation.h"
#include "modulations/SinusoidalModulation.h"

#include "core/EnsembleResult.h"
#include "core/ExpressionEvaluator.h"
#include "core/Potential.h"
#include "core/SeedFactory.h"
#include "core/SimulationObserver.h"
#include "core/SimulationParams.h"

#include "modulations/DichotomicParams.h"
#include "modulations/ModulationFactory.h"
#include "modulations/ModulationSettings.h"
#include "modulations/PeriodicTwoStateModulation.h"
#include "modulations/RectangularModulation.h"

#include "profiles/BiharmonicProfile.h"
#include "profiles/TabulatedPeriodicProfile.h"

#include "solver/LangevinSolverCpu.h"

SimulationWorker::SimulationWorker(
    SimulationRequest request,
    std::shared_ptr<std::stop_source> cancellation_source,
    QObject* parent
)
    : QObject{parent}
    , request_{std::move(request)}
    , cancellation_source_{std::move(cancellation_source)}
{}

void SimulationWorker::run() {
    try {
        if (!cancellation_source_) {
            throw std::invalid_argument(
                "Simulation cancellation source must not be null"
            );
        }

        /*
         * Built-in BiharmonicProfile:
         *   periodic, period = 1.
         *
         * JSON profile:
         *   periodic or non-periodic according to its definition.
         */
        const bool periodic_profile =
            request_.potential_definition
                ? request_.potential_definition->profile.periodic
                : true;

        const double spatial_period =
            request_.potential_definition &&
            request_.potential_definition->profile.periodic
                ? request_.potential_definition->profile.period
                : 1.0;

        if (
            request_.use_lookup_table &&
            (
                !request_.potential_definition ||
                !periodic_profile
            )
        ) {
            throw std::invalid_argument(
                "Lookup mode requires a periodic JSON potential"
            );
        }

        const SimulationParams simulation_params{
            .dt = request_.dt,
            .total_time = request_.total_time,

            .n_particles = request_.n_particles,
            .burn_in_steps = request_.burn_in_steps,

            .x0 = request_.x0,

            .periodic_profile = periodic_profile,
            .spatial_period = spatial_period,

            .store_trajectory = request_.interactive_mode,

            .trajectory_stride = request_.batch_steps,
            .live_update_stride = request_.batch_steps,
            .batch_steps = request_.batch_steps,

            .cancellation_check_steps =
                request_.cancellation_check_steps
        };

        simulation_params.validate();
        request_.modulation.validate();

        const SeedFactory seeds{
            request_.seed
        };

        const auto stop_token =
            cancellation_source_->get_token();

        /*
         * One observer is shared by all modulation/profile paths.
         * It is passed to the solver only in interactive mode.
         */
        const SimulationObserver observer =
            [this](
                const SimulationUpdate& update
            ) {
                emit progress_updated(
                    static_cast<qulonglong>(
                        update.step + 1
                    ),
                    static_cast<qulonglong>(
                        update.total_steps
                    ),
                    update.time,
                    update.mean_x,
                    update.mean_velocity
                );
            };

        /*
         * Instantiate the solver for the selected profile type,
         * then dispatch to the selected modulation process.
         */
        const auto solve_with_profile =
            [&]<typename Profile>(
                Profile profile
            ) -> EnsembleResult {
                Potential<Profile> potential{
                    std::move(profile)
                };

                LangevinSolverCpu<Profile> solver{
                    std::move(potential),
                    simulation_params,
                    seeds,
                    request_.requested_workers
                };

            const auto solve_deterministic =
                [&](auto& modulation) -> EnsembleResult {
                    if (request_.interactive_mode) {
                        return solver
                            .solve_deterministic_interactive(
                                modulation,
                                observer,
                                stop_token
                            );
                    }

                    return solver
                        .solve_deterministic_fast(
                            modulation,
                            stop_token
                        );
                };

                switch (request_.modulation.kind) {
                case ModulationKind::LegacySymmetric: {
                    /*
                     * Preserve the existing parameter mapping:
                     * amplitude, epsilon (= correlation time), alpha.
                     */
                    const DichotomicParams modulation_params =
                        DichotomicParams::symmetric(
                            request_.modulation_amplitude,
                            request_.epsilon,
                            request_.dt,
                            request_.alpha
                        );

                    auto modulations =
                        ModulationFactory::
                            make_dichotomic_ensemble(
                                modulation_params,
                                request_.n_particles,
                                seeds
                            );

                    if (request_.interactive_mode) {
                        return solver
                            .solve_stochastic_interactive(
                                modulations,
                                observer,
                                stop_token
                            );
                    }

                    return solver.solve_stochastic_fast(
                        modulations,
                        stop_token
                    );
                }

                case ModulationKind::MarkovTwoState: {
                    /*
                     * Explicit potential factors and independent
                     * transition rates A -> B and B -> A.
                     *
                     * Each particle receives its own process.
                     */
                    auto modulations =
                        ModulationFactory::
                            make_two_state_factor_ensemble(
                                request_.modulation,
                                request_.dt,
                                request_.n_particles,
                                seeds
                            );

                    if (request_.interactive_mode) {
                        return solver
                            .solve_stochastic_interactive(
                                modulations,
                                observer,
                                stop_token
                            );
                    }

                    return solver.solve_stochastic_fast(
                        modulations,
                        stop_token
                    );
                }

                case ModulationKind::PeriodicTwoState: {
                    /*
                     * One deterministic switching schedule
                     * is used for the entire ensemble.
                     *
                     * The deterministic solver prepares the
                     * factors before launching worker threads.
                     */
                    PeriodicTwoStateModulation modulation{
                        request_.modulation
                    };

                    if (request_.interactive_mode) {
                        return solver
                            .solve_deterministic_interactive(
                                modulation,
                                observer,
                                stop_token
                            );
                    }

                    return solver.solve_deterministic_fast(
                        modulation,
                        stop_token
                    );
                }

                case ModulationKind::Constant: {
                        ConstantModulation modulation{
                            request_.modulation.constant_factor
                        };

                        return solve_deterministic(
                            modulation
                        );
                    }

                case ModulationKind::Sinusoidal: {
                        const double omega =
                            2.0 *
                            std::numbers::pi_v<double> /
                            request_.modulation.sinusoidal_period;

                        SinusoidalModulation modulation{
                            request_.modulation.sinusoidal_amplitude,
                            omega,
                            request_.dt,

                            // Initial model time.
                            0.0,

                            request_.modulation.sinusoidal_offset,
                            request_.modulation.sinusoidal_phase
                        };

                        return solve_deterministic(
                            modulation
                        );
                    }

                    case ModulationKind::RectangularPeriodic: {
                        RectangularModulation modulation{
                            request_.modulation,
                            request_.dt
                        };

                        return solve_deterministic(
                            modulation
                        );
                    }
                }

                throw std::invalid_argument(
                    "Unknown modulation kind"
                );
            };

        /*
         * Total measured time includes profile preparation,
         * modulation preparation and the solver execution.
         */
        const auto started_at =
            std::chrono::steady_clock::now();

        EnsembleResult result;

        if (request_.potential_definition) {
            const auto preparation_started_at =
                std::chrono::steady_clock::now();

            ExpressionEvaluator exact_profile{
                *request_.potential_definition,
                request_.potential_parameter_values
            };

            if (request_.use_lookup_table) {
                TabulatedPeriodicProfile<
                    ExpressionEvaluator
                > profile{
                    std::move(exact_profile),
                    spatial_period,
                    request_.lookup_intervals
                };

                const double preparation_seconds =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() -
                        preparation_started_at
                    ).count();

                emit profile_prepared(
                    QStringLiteral(
                        "periodic_lookup_linear"
                    ),
                    static_cast<qulonglong>(
                        profile.intervals()
                    ),
                    preparation_seconds
                );

                result = solve_with_profile(
                    std::move(profile)
                );
            } else {
                const double preparation_seconds =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() -
                        preparation_started_at
                    ).count();

                emit profile_prepared(
                    QStringLiteral("exprtk"),
                    0,
                    preparation_seconds
                );

                result = solve_with_profile(
                    std::move(exact_profile)
                );
            }
        } else {
            const BiharmonicProfile profile{
                .V1 = request_.v1,
                .V2 = request_.v2
            };

            emit profile_prepared(
                QStringLiteral("builtin"),
                0,
                0.0
            );

            result = solve_with_profile(profile);
        }

        const auto finished_at =
            std::chrono::steady_clock::now();

        const double elapsed_seconds =
            std::chrono::duration<double>(
                finished_at - started_at
            ).count();

        const std::size_t total_updates =
            request_.n_particles *
            simulation_params.step_count();

        const double throughput =
            elapsed_seconds > 0.0
                ? static_cast<double>(total_updates) /
                  elapsed_seconds /
                  1.0e6
                : 0.0;

        const std::size_t detected_hardware_threads =
            std::max(
                std::size_t{1},
                static_cast<std::size_t>(
                    std::thread::hardware_concurrency()
                )
            );

        const std::size_t requested_workers =
            request_.requested_workers == 0
                ? detected_hardware_threads
                : request_.requested_workers;

        const std::size_t workers =
            std::min(
                std::max(
                    std::size_t{1},
                    requested_workers
                ),
                request_.n_particles
            );

        QJsonObject statistics{
            {
                "available",
                result.has_velocity_statistics
            },
            {
                "particles",
                static_cast<qint64>(
                    result.n_particles
                )
            },
            {
                "cancelled",
                stop_token.stop_requested()
            },
            {
                "initial_position",
                request_.x0
            },
            {
                "dt",
                request_.dt
            },
            {
                "total_time_requested",
                request_.total_time
            },
            {
                "burn_in_steps_requested",
                static_cast<qint64>(
                    request_.burn_in_steps
                )
            },
            {
                "seed",
                static_cast<qint64>(
                    request_.seed
                )
            }
        };

        if (result.has_velocity_statistics) {
            statistics.insert(
                "measurement_time",
                result.measurement_time
            );

            statistics.insert(
                "mean_displacement",
                result.mean_displacement
            );

            statistics.insert(
                "displacement_variance",
                result.displacement_variance
            );

            statistics.insert(
                "mean_squared_displacement",
                result.mean_squared_displacement
            );

            statistics.insert(
                "mean_velocity",
                result.mean_velocity
            );

            statistics.insert(
                "velocity_standard_error",
                result.velocity_standard_error
            );

            statistics.insert(
                "velocity_ci95_lower",
                result.velocity_ci95_lower
            );

            statistics.insert(
                "velocity_ci95_upper",
                result.velocity_ci95_upper
            );

            statistics.insert(
                "ci_method",
                QStringLiteral(
                    "normal_approximation_95"
                )
            );

            const bool contains_zero =
                result.velocity_ci95_lower <= 0.0 &&
                result.velocity_ci95_upper >= 0.0;

            statistics.insert(
                "ci95_contains_zero",
                contains_zero
            );
        }

        statistics.insert(
        "control_variate_available",
            result.has_control_variate_statistics
        );

        if (result.has_control_variate_statistics) {
            statistics.insert(
                "control_variate_velocity",
                result.control_variate_velocity
            );

            statistics.insert(
                "control_variate_standard_error",
                result.control_variate_standard_error
            );

            statistics.insert(
                "control_variate_ci95_lower",
                result.control_variate_ci95_lower
            );

            statistics.insert(
                "control_variate_ci95_upper",
                result.control_variate_ci95_upper
            );

            statistics.insert(
                "integrated_drift_variance",
                result.integrated_drift_variance
            );

            statistics.insert(
                "mean_thermal_velocity",
                result.mean_thermal_velocity
            );

            const bool contains_zero =
                result.control_variate_ci95_lower <= 0.0 &&
                result.control_variate_ci95_upper >= 0.0;

            statistics.insert(
                "control_variate_ci95_contains_zero",
                contains_zero
            );

            statistics.insert(
                "control_variate_method",
                QStringLiteral(
                    "same_thermal_noise_fixed_beta_1"
                )
            );
        }

        emit statistics_ready(
            statistics
        );

        emit completed(
            result.mean_velocity,
            result.mean_x_final,
            elapsed_seconds,
            throughput,
            workers
        );
    } catch (const std::exception& exception) {
        emit failed(
            QString::fromUtf8(
                exception.what()
            )
        );
    }

    emit finished();
}