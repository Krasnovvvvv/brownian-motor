#ifndef BROWNIAN_MOTOR_LANGEVINSOLVERCPU_H
#define BROWNIAN_MOTOR_LANGEVINSOLVERCPU_H
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/EnsembleResult.h"
#include "core/Potential.h"
#include "core/SeedFactory.h"
#include "core/SimulationObserver.h"
#include "core/SimulationParams.h"

struct alignas(64) PaddedReduction final {
    double final_position_sum{0.0};
    double burn_in_position_sum{0.0};
};

template <typename Modulation>
concept ModulationProcess =
    requires(Modulation modulation) {
        { modulation.step() } noexcept -> std::convertible_to<double>;
    };

template <typename Profile>
class LangevinSolverCpu final {
public:
    LangevinSolverCpu(
        Potential<Profile> potential,
        SimulationParams params,
        SeedFactory seeds,
        std::size_t requested_workers = std::thread::hardware_concurrency()
    )
        : potential_{std::move(potential)}
        , params_{params}
        , seeds_{seeds}
    {
        params_.validate();

        if (requested_workers == 0) {
            requested_workers =
                std::thread::hardware_concurrency();
        }

        requested_workers_ = std::max(
            std::size_t{1},
            requested_workers
        );

        sqrt_2dt_ = std::sqrt(2.0 * params_.dt);
    }

    template <ModulationProcess Modulation>
    [[nodiscard]] EnsembleResult solve_stochastic_fast(
        std::vector<Modulation>& modulations,
        std::stop_token stop_token = {}
    ) const {
        if (modulations.size() != params_.n_particles) {
            throw std::invalid_argument(
                "modulations.size() must equal n_particles"
            );
        }

        ensure_fast_mode_();

        return solve_fast_(
            [&modulations](
                std::size_t particle,
                std::size_t
            ) noexcept {
                return modulations[particle].step();
            },
            stop_token
        );
    }

    template <ModulationProcess Modulation>
    [[nodiscard]] EnsembleResult solve_deterministic_fast(
        Modulation& modulation,
        std::stop_token stop_token = {}
    ) const {
        ensure_fast_mode_();

        const std::size_t total_steps = params_.step_count();

        std::vector<double> factors(total_steps);

        for (std::size_t step = 0;
             step < total_steps;
             ++step) {
            factors[step] = modulation.step();
        }

        return solve_fast_(
            [&factors](
                std::size_t,
                std::size_t step
            ) noexcept {
                return factors[step];
            },
            stop_token
        );
    }
    
    template <ModulationProcess Modulation>
    [[nodiscard]] EnsembleResult solve_stochastic_interactive(
        std::vector<Modulation>& modulations,
        const SimulationObserver& observer = {},
        std::stop_token stop_token = {}
    ) const {
        if (modulations.size() != params_.n_particles) {
            throw std::invalid_argument(
                "modulations.size() must equal n_particles"
            );
        }

        return solve_interactive_(
            [&modulations](
                std::size_t particle,
                std::size_t
            ) noexcept {
                return modulations[particle].step();
            },
            observer,
            stop_token
        );
    }

    template <ModulationProcess Modulation>
    [[nodiscard]] EnsembleResult solve_stochastic(
        std::vector<Modulation>& modulations,
        std::stop_token stop_token = {}
    ) const {
        return solve_stochastic_fast(modulations, stop_token);
    }

    template <ModulationProcess Modulation>
    [[nodiscard]] EnsembleResult solve_deterministic(
        Modulation& modulation,
        std::stop_token stop_token = {}
    ) const {
        return solve_deterministic_fast(modulation, stop_token);
    }

private:
    struct NumericalFailure final {
        std::size_t particle{};
        std::size_t step{};

        double position{};

        const char* reason{};
    };

    struct SimulationState final {
        std::vector<double> x_unwrapped;
        std::vector<double> x_wrapped;

        std::vector<std::mt19937> gaussian_rngs;

        std::vector<std::normal_distribution<double>>
            gaussian_distributions;
    };

    static void throw_if_numerical_failure_(
        const std::vector<
            std::optional<NumericalFailure>
        >& failures
    ) {
        for (const auto& failure : failures) {
            if (!failure) {
                continue;
            }

            throw std::runtime_error(
                "Non-finite simulation value: " +
                std::string{
                    failure->reason
                        ? failure->reason
                        : "unknown cause"
                } +
                "; particle = " +
                std::to_string(
                    failure->particle + 1
                ) +
                "; step = " +
                std::to_string(
                    failure->step + 1
                ) +
                "; previous x = " +
                std::to_string(
                    failure->position
                )
            );
        }
    }

    [[nodiscard]] std::vector<Potential<Profile>>
    make_worker_potentials_(
        std::size_t worker_count
    ) const {
        std::vector<Potential<Profile>>
            worker_potentials;

        worker_potentials.reserve(worker_count);

        for (
            std::size_t worker = 0;
            worker < worker_count;
            ++worker
        ) {
            /*
             * Для BiharmonicProfile это обычная дешёвая копия.
             *
             * Для ExpressionEvaluator copy constructor заново
             * строит отдельные ExprTk symbol table и expressions.
             */
            worker_potentials.push_back(
                potential_
            );
        }

        return worker_potentials;
    }

    void ensure_fast_mode_() const {
        if (params_.store_trajectory) {
            throw std::invalid_argument(
                "Fast solver requires store_trajectory = false. "
                "Use the interactive solver for live trajectories."
            );
        }
    }

    [[nodiscard]] std::size_t worker_count_() const noexcept {
        return std::min(
            requested_workers_,
            params_.n_particles
        );
    }

    [[nodiscard]] SimulationState make_initial_state_() const {
        SimulationState state;

        state.x_unwrapped.assign(
            params_.n_particles,
            params_.x0
        );

        state.x_wrapped.assign(
            params_.n_particles,
            evaluation_coordinate_(params_.x0)
        );

        state.gaussian_rngs.reserve(params_.n_particles);

        for (std::size_t particle = 0;
             particle < params_.n_particles;
             ++particle) {
            state.gaussian_rngs.emplace_back(
                seeds_.make_mt19937(
                    particle,
                    SeedFactory::Stream::GaussianThermal
                )
            );
        }

        state.gaussian_distributions.assign(
            params_.n_particles,
            std::normal_distribution<double>{0.0, 1.0}
        );

        return state;
    }

    template <typename FactorProvider>
    [[nodiscard]] EnsembleResult solve_fast_(
        FactorProvider factor_provider,
        const std::stop_token& stop_token
    ) const {
        const std::size_t total_steps = params_.step_count();
        const std::size_t active_workers = worker_count_();

        const std::size_t burn_in_steps = std::min(
            params_.burn_in_steps,
            total_steps - 1
        );

        SimulationState state = make_initial_state_();

        std::vector<Potential<Profile>>
            worker_potentials =
                make_worker_potentials_(
                    active_workers
                );

        std::atomic<bool> cancellation_requested{false};

        std::vector<PaddedReduction> partial_results(
            active_workers
        );

        std::vector<
            std::optional<NumericalFailure>
        > numerical_failures(
            active_workers
        );

        const std::size_t particles_per_worker =
            (params_.n_particles + active_workers - 1) /
            active_workers;

        std::vector<std::jthread> workers;
        workers.reserve(active_workers);

        for (std::size_t worker = 0;
             worker < active_workers;
             ++worker) {
            const std::size_t first_particle =
                worker * particles_per_worker;

            const std::size_t last_particle = std::min(
                params_.n_particles,
                first_particle + particles_per_worker
            );

            workers.emplace_back(
                [this,
                 &state,
                 &worker_potentials,
                 &partial_results,
                 &numerical_failures,
                 &cancellation_requested,
                 factor_provider,
                 stop_token,
                 first_particle,
                 last_particle,
                 worker,
                 total_steps,
                 burn_in_steps](const std::stop_token&) mutable {
                     const Potential<Profile>& worker_potential =
                         worker_potentials[worker];
                    double local_final_sum = 0.0;
                    double local_burn_in_sum = 0.0;

                    for (std::size_t particle = first_particle;
                         particle < last_particle;
                         ++particle) {
                        double& x_unwrapped =
                            state.x_unwrapped[particle];

                        double& x_wrapped =
                            state.x_wrapped[particle];

                        std::mt19937& gaussian_rng =
                            state.gaussian_rngs[particle];

                        std::normal_distribution<double>&
                            gaussian_distribution =
                                state.gaussian_distributions[
                                    particle
                                ];

                        double x_at_burn_in = params_.x0;

                        for (std::size_t step = 0;
                             step < total_steps;
                             ++step) {
                            if (step % params_.cancellation_check_steps ==
                                    0 &&
                                stop_token.stop_requested()) {
                                cancellation_requested.store(
                                    true,
                                    std::memory_order_release
                                );
                                break;
                            }

                            const double factor =
                                factor_provider(particle, step);

                            const double previous_x =
                                x_unwrapped;

                            const char* failure_reason =
                                nullptr;

                            if (
                                !advance_particle_(
                                    worker_potential,
                                    x_unwrapped,
                                    x_wrapped,
                                    gaussian_rng,
                                    gaussian_distribution,
                                    factor,
                                    failure_reason
                                )
                            ) {
                                numerical_failures[worker] =
                                    NumericalFailure{
                                        .particle = particle,
                                        .step = step,
                                        .position = previous_x,
                                        .reason = failure_reason
                                    };

                                break;
                            }

                            if (step + 1 == burn_in_steps) {
                                x_at_burn_in = x_unwrapped;
                            }
                        }

                        if (numerical_failures[worker]) {
                            break;
                        }

                        local_final_sum += x_unwrapped;
                        local_burn_in_sum += x_at_burn_in;
                    }

                    partial_results[worker].final_position_sum =
                        local_final_sum;

                    partial_results[worker].burn_in_position_sum =
                        local_burn_in_sum;
                }
            );
        }

        workers.clear();

        throw_if_numerical_failure_(
            numerical_failures
        );

        double final_position_sum = 0.0;
        double burn_in_position_sum = 0.0;

        for (const PaddedReduction& partial : partial_results) {
            final_position_sum += partial.final_position_sum;
            burn_in_position_sum +=
                partial.burn_in_position_sum;
        }

        const double particle_count =
            static_cast<double>(params_.n_particles);

        const double mean_x_final =
            final_position_sum / particle_count;

        const double mean_x_burn_in =
            burn_in_position_sum / particle_count;

        EnsembleResult result;
        result.n_particles = params_.n_particles;
        result.has_trajectory = false;
        result.mean_x_final = mean_x_final;

        if (cancellation_requested.load(
                std::memory_order_acquire)) {
            result.mean_velocity = 0.0;
            return result;
        }

        const double measurement_time =
            static_cast<double>(
                total_steps - burn_in_steps
            ) * params_.dt;

        result.mean_velocity =
            measurement_time > 0.0
                ? (mean_x_final - mean_x_burn_in) /
                  measurement_time
                : 0.0;

        return result;
    }

    template <typename FactorProvider>
    [[nodiscard]] EnsembleResult solve_interactive_(
        FactorProvider factor_provider,
        const SimulationObserver& observer,
        const std::stop_token& stop_token
    ) const {
        const std::size_t total_steps = params_.step_count();
        const std::size_t active_workers = worker_count_();

        const std::size_t burn_in_steps = std::min(
            params_.burn_in_steps,
            total_steps - 1
        );

        const std::size_t particles_per_worker =
            (params_.n_particles + active_workers - 1) /
            active_workers;

        SimulationState state = make_initial_state_();

        std::vector<Potential<Profile>>
            worker_potentials =
                make_worker_potentials_(
                    active_workers
                );

        EnsembleResult result;
        result.n_particles = params_.n_particles;
        result.has_trajectory = params_.store_trajectory;

        if (params_.store_trajectory) {
            const std::size_t point_count =
                (total_steps + params_.batch_steps - 1) /
                params_.batch_steps;

            result.sim_time.reserve(point_count);
            result.mean_x.reserve(point_count);
        }

        const double particle_count =
            static_cast<double>(params_.n_particles);

        double mean_x_burn_in = params_.x0;
        double mean_x_current = params_.x0;

        for (std::size_t batch_begin = 0;
             batch_begin < total_steps;
             batch_begin += params_.batch_steps) {
            const std::size_t batch_end = std::min(
                total_steps,
                batch_begin + params_.batch_steps
            );

            std::atomic<bool> cancellation_requested{false};

            std::vector<PaddedReduction> partial_results(
                active_workers
            );

            std::vector<
                std::optional<NumericalFailure>
            > numerical_failures(
                active_workers
            );

            std::vector<std::jthread> workers;
            workers.reserve(active_workers);

            for (std::size_t worker = 0;
                 worker < active_workers;
                 ++worker) {
                const std::size_t first_particle =
                    worker * particles_per_worker;

                const std::size_t last_particle = std::min(
                    params_.n_particles,
                    first_particle + particles_per_worker
                );

                workers.emplace_back(
                    [this,
                     &state,
                     &worker_potentials,
                     &numerical_failures,
                     &partial_results,
                     &cancellation_requested,
                     factor_provider,
                     stop_token,
                     first_particle,
                     last_particle,
                     worker,
                     batch_begin,
                     batch_end,
                     burn_in_steps](
                        const std::stop_token&
                    ) mutable {
                        const Potential<Profile>& worker_potential =
                            worker_potentials[worker];
                        double local_final_sum = 0.0;
                        double local_burn_in_sum = 0.0;

                        for (std::size_t particle = first_particle;
                             particle < last_particle;
                             ++particle) {
                            double& x_unwrapped =
                                state.x_unwrapped[particle];

                            double& x_wrapped =
                                state.x_wrapped[particle];

                            std::mt19937& gaussian_rng =
                                state.gaussian_rngs[particle];

                            std::normal_distribution<double>&
                                gaussian_distribution =
                                    state.gaussian_distributions[
                                        particle
                                    ];

                            double x_at_burn_in = params_.x0;

                            for (std::size_t step = batch_begin;
                                 step < batch_end;
                                 ++step) {
                                if (step %
                                        params_.cancellation_check_steps ==
                                        0 &&
                                    stop_token.stop_requested()) {
                                    cancellation_requested.store(
                                        true,
                                        std::memory_order_release
                                    );
                                    break;
                                }

                                const double factor =
                                    factor_provider(
                                        particle,
                                        step
                                    );

                                const double previous_x =
                                    x_unwrapped;

                                const char* failure_reason =
                                    nullptr;

                                if (
                                    !advance_particle_(
                                        worker_potential,
                                        x_unwrapped,
                                        x_wrapped,
                                        gaussian_rng,
                                        gaussian_distribution,
                                        factor,
                                        failure_reason
                                    )
                                ) {
                                    numerical_failures[worker] =
                                        NumericalFailure{
                                            .particle = particle,
                                            .step = step,
                                            .position = previous_x,
                                            .reason = failure_reason
                                        };

                                    break;
                                }

                                if (step + 1 == burn_in_steps) {
                                    x_at_burn_in = x_unwrapped;
                                }
                            }

                            if (numerical_failures[worker]) {
                                break;
                            }

                            local_final_sum += x_unwrapped;

                            if (batch_begin < burn_in_steps &&
                                burn_in_steps <= batch_end) {
                                local_burn_in_sum +=
                                    x_at_burn_in;
                            }
                        }

                        partial_results[worker].final_position_sum =
                            local_final_sum;

                        partial_results[worker].burn_in_position_sum =
                            local_burn_in_sum;
                    }
                );
            }

            workers.clear();

            throw_if_numerical_failure_(
                numerical_failures
            );

            if (cancellation_requested.load(
                    std::memory_order_acquire)) {
                result.mean_x_final = mean_x_current;
                result.mean_velocity = 0.0;
                return result;
            }

            double final_position_sum = 0.0;
            double burn_in_position_sum = 0.0;

            for (const PaddedReduction& partial : partial_results) {
                final_position_sum += partial.final_position_sum;
                burn_in_position_sum +=
                    partial.burn_in_position_sum;
            }

            mean_x_current =
                final_position_sum / particle_count;

            const std::size_t completed_steps = batch_end;

            if (batch_begin < burn_in_steps &&
                burn_in_steps <= batch_end) {
                mean_x_burn_in =
                    burn_in_position_sum / particle_count;
            }

            const double current_time =
                static_cast<double>(completed_steps) *
                params_.dt;

            double mean_velocity = 0.0;

            if (completed_steps > burn_in_steps) {
                const double measurement_time =
                    static_cast<double>(
                        completed_steps - burn_in_steps
                    ) * params_.dt;

                mean_velocity =
                    (mean_x_current - mean_x_burn_in) /
                    measurement_time;
            }

            if (params_.store_trajectory) {
                result.sim_time.push_back(current_time);
                result.mean_x.push_back(mean_x_current);
            }

            if (observer) {
                observer(
                    SimulationUpdate{
                        .step = completed_steps - 1,
                        .total_steps = total_steps,
                        .time = current_time,
                        .mean_x = mean_x_current,
                        .mean_velocity = mean_velocity
                    }
                );
            }
        }

        result.mean_x_final = mean_x_current;

        const double measurement_time =
            static_cast<double>(
                total_steps - burn_in_steps
            ) * params_.dt;

        result.mean_velocity =
            measurement_time > 0.0
                ? (mean_x_current - mean_x_burn_in) /
                  measurement_time
                : 0.0;

        return result;
    }

    [[nodiscard]] bool advance_particle_(
        const Potential<Profile>& potential,
        double& x_unwrapped,
        double& x_wrapped,
        std::mt19937& gaussian_rng,
        std::normal_distribution<double>&
            gaussian_distribution,
        double factor,
        const char*& failure_reason
    ) const noexcept {
    failure_reason = nullptr;

    if (!std::isfinite(factor)) {
        failure_reason =
            "modulation factor is not finite";

        return false;
    }

    const double thermal_increment =
        sqrt_2dt_ *
        gaussian_distribution(
            gaussian_rng
        );

    if (!std::isfinite(thermal_increment)) {
        failure_reason =
            "thermal increment is not finite";

        return false;
    }

    const double derivative_now =
        potential.derivative_value(
            x_wrapped,
            factor
        );

    if (!std::isfinite(derivative_now)) {
        failure_reason =
            "potential derivative at "
            "current position is not finite";

        return false;
    }

    const double predicted_x =
        x_unwrapped -
        derivative_now * params_.dt +
        thermal_increment;

    if (!std::isfinite(predicted_x)) {
        failure_reason =
            "predicted position is not finite";

        return false;
    }

    const double predicted_coordinate =
        evaluation_coordinate_(
            predicted_x
        );

    if (
        !std::isfinite(
            predicted_coordinate
        )
    ) {
        failure_reason =
            "wrapped predicted position "
            "is not finite";

        return false;
    }

    const double derivative_predicted =
        potential.derivative_value(
            predicted_coordinate,
            factor
        );

    if (
        !std::isfinite(
            derivative_predicted
        )
    ) {
        failure_reason =
            "potential derivative at "
            "predicted position is not finite";

        return false;
    }

    const double delta_x =
        -0.5 *
        (
            derivative_now +
            derivative_predicted
        ) *
        params_.dt +
        thermal_increment;

    if (!std::isfinite(delta_x)) {
        failure_reason =
            "position increment is not finite";

        return false;
    }

    const double next_x =
        x_unwrapped + delta_x;

    if (!std::isfinite(next_x)) {
        failure_reason =
            "new position is not finite";

        return false;
    }

    const double next_coordinate =
        evaluation_coordinate_(
            next_x
        );

    if (
        !std::isfinite(
            next_coordinate
        )
    ) {
        failure_reason =
            "wrapped new position "
            "is not finite";

        return false;
    }

        /*
        * Состояние частицы обновляем только после
        * прохождения всех проверок.
         */
        x_unwrapped = next_x;
        x_wrapped = next_coordinate;

        return true;
    }

    [[nodiscard]] double evaluation_coordinate_(
        double x
    ) const noexcept {
        if (!params_.periodic_profile) {
            return x;
        }

        if (params_.spatial_period == 1.0) {
            return wrap_unit_fast_(x);
        }

        return wrap_period_(
            x,
            params_.spatial_period
        );
    }

    [[nodiscard]] static double wrap_period_(
        double x,
        double period
    ) noexcept {
        return x -
            period * std::floor(x / period);
    }

    [[nodiscard]] static double wrap_unit_fast_(
        const double x
    ) noexcept {
        if (x >= 1.0 && x < 2.0) {
            return x - 1.0;
        }

        if (x < 0.0 && x >= -1.0) {
            return x + 1.0;
        }

        return x - std::floor(x);
    }

    [[nodiscard]] static double wrap_unit_(
        const double x
    ) noexcept {
        return x - std::floor(x);
    }

    Potential<Profile> potential_;
    SimulationParams params_;
    SeedFactory seeds_;

    std::size_t requested_workers_{1};
    double sqrt_2dt_{0.0};
};

#endif // BROWNIAN_MOTOR_LANGEVINSOLVERCPU_H