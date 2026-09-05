#include "gui/SimulationWorker.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "core/EnsembleResult.h"
#include "core/Potential.h"
#include "core/SeedFactory.h"
#include "core/SimulationParams.h"
#include "modulations/DichotomicParams.h"
#include "modulations/ModulationFactory.h"
#include "profiles/BiharmonicProfile.h"
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
        const SimulationParams simulation_params{
            .dt = request_.dt,
            .total_time = request_.total_time,
            .n_particles = request_.n_particles,
            .burn_in_steps = request_.burn_in_steps,
            .x0 = request_.x0,
            .store_trajectory = false,
            .trajectory_stride = 1'000,
            .live_update_stride = 1'000,
            .batch_steps = 3'500,
            .cancellation_check_steps =
                request_.cancellation_check_steps
        };

        simulation_params.validate();

        const BiharmonicProfile profile{
            .V1 = request_.v1,
            .V2 = request_.v2
        };

        const Potential potential{profile};
        const SeedFactory seeds{request_.seed};

        const DichotomicParams modulation_params =
            DichotomicParams::symmetric(
                request_.modulation_amplitude,
                request_.epsilon,
                request_.dt,
                request_.alpha
            );

        auto modulations =
            ModulationFactory::make_dichotomic_ensemble(
                modulation_params,
                request_.n_particles,
                seeds
            );

        LangevinSolverCpu<BiharmonicProfile> solver{
            potential,
            simulation_params,
            seeds,
            request_.requested_workers
        };

        const auto started_at =
            std::chrono::steady_clock::now();

        const EnsembleResult result =
            solver.solve_stochastic_fast(
                modulations,
                cancellation_source_->get_token()
            );

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
                std::max(std::size_t{1}, requested_workers),
                request_.n_particles
            );

        emit completed(
            result.mean_velocity,
            result.mean_x_final,
            elapsed_seconds,
            throughput,
            workers
        );
    } catch (const std::exception& exception) {
        emit failed(QString::fromUtf8(exception.what()));
    }

    emit finished();
}