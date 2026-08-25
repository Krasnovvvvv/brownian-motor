#ifndef BROWNIAN_MOTOR_SIMULATIONPARAMS_H
#define BROWNIAN_MOTOR_SIMULATIONPARAMS_H
#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>

struct SimulationParams final {
    double dt{};
    double total_time{};

    std::size_t n_particles{};

    /*
     * Количество первых физических шагов, которые не учитываются
     * при измерении stationary drift velocity.
     */
    std::size_t burn_in_steps{};

    double x0{};

    /*
     * Fast solver не строит trajectory во время расчёта:
     * workers работают независимо до самого конца.
     *
     * Для максимальной производительности это обязательно false.
     */
    bool store_trajectory{false};

    /*
     * Эти параметры сохранены для будущего interactive solver
     * с Qt/live plot. Fast solver их не использует.
     */
    std::size_t trajectory_stride{1'000};
    std::size_t live_update_stride{1'000};

    /*
     * Сохранено для будущего interactive/batched solver.
     * Fast solver не использует batch_steps, так как у него
     * вообще нет промежуточных global barriers.
     */
    std::size_t batch_steps{3'500};

    /*
     * Как часто worker проверяет stop_token во время обработки
     * одной particle trajectory.
     *
     * 4'096 означает проверку примерно раз в 4096 physical steps.
     * Это даёт адекватную отмену без atomic/barrier в hot path
     * на каждом step.
     */
    std::size_t cancellation_check_steps{4'096};

    void validate() const {
        if (!std::isfinite(dt) || dt <= 0.0) {
            throw std::invalid_argument("dt must be finite and > 0");
        }

        if (!std::isfinite(total_time) || total_time <= 0.0) {
            throw std::invalid_argument(
                "total_time must be finite and > 0"
            );
        }

        if (n_particles == 0) {
            throw std::invalid_argument(
                "n_particles must be > 0"
            );
        }

        if (!std::isfinite(x0)) {
            throw std::invalid_argument("x0 must be finite");
        }

        if (trajectory_stride == 0) {
            throw std::invalid_argument(
                "trajectory_stride must be > 0"
            );
        }

        if (live_update_stride == 0) {
            throw std::invalid_argument(
                "live_update_stride must be > 0"
            );
        }

        if (batch_steps == 0) {
            throw std::invalid_argument(
                "batch_steps must be > 0"
            );
        }

        if (cancellation_check_steps == 0) {
            throw std::invalid_argument(
                "cancellation_check_steps must be > 0"
            );
        }

        if (step_count() == 0) {
            throw std::invalid_argument(
                "total_time / dt must produce at least one step"
            );
        }
    }

    [[nodiscard]] std::size_t step_count() const noexcept {
        return static_cast<std::size_t>(total_time / dt);
    }
};

#endif // BROWNIAN_MOTOR_SIMULATIONPARAMS_H