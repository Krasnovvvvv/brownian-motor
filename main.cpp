#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "profiles/BiharmonicProfile.h"
#include "modulations//DichotomicModulation.h"
#include "modulations/DichotomicParams.h"
#include "core/EnsembleResult.h"
#include "solver/LangevinSolverCpu.h"
#include "modulations/ModulationFactory.h"
#include "core/Potential.h"
#include "core/SeedFactory.h"
#include "core/SimulationParams.h"

namespace {

struct ExperimentConfig final {
    double V1{0.10};
    double V2{0.025};

    double modulation_amplitude{1.0};
    double tau_c{0.10};
    double alpha{-1.0 / 3.0};

    double dt{1.0e-3};
    double total_time{100.0};
    std::size_t n_particles{2'000};

    std::size_t burn_in_steps{10'000};

    double x0{0.0};

    std::uint32_t seed{125u};

    std::size_t requested_workers{0};

    std::size_t cancellation_check_steps{4'096};
};

[[nodiscard]] std::size_t hardware_workers() noexcept {
    return std::max(
        std::size_t{1},
        static_cast<std::size_t>(
            std::thread::hardware_concurrency()
        )
    );
}

[[nodiscard]] std::size_t effective_workers(
    const ExperimentConfig& config
) noexcept {
    const std::size_t requested =
        config.requested_workers == 0
            ? hardware_workers()
            : config.requested_workers;

    return std::min(
        std::max(std::size_t{1}, requested),
        config.n_particles
    );
}

void print_configuration(
    const ExperimentConfig& config,
    std::size_t steps,
    std::size_t workers
) {
    std::cout
        << "=== Brownian Motor: Fast CPU Experiment ===\n\n"
        << "Build mode:              Release (-O3 -march=native)\n"
        << "Hardware concurrency:    " << hardware_workers() << '\n'
        << "Workers used:            " << workers << '\n'
        << "Random seed:             " << config.seed << "\n\n"

        << "--- Potential ---\n"
        << "V(x) = V1*sin(2*pi*x) + V2*sin(4*pi*x)\n"
        << "V1:                      " << config.V1 << '\n'
        << "V2:                      " << config.V2 << "\n\n"

        << "--- Dichotomic modulation ---\n"
        << "Amplitude:               "
        << config.modulation_amplitude << '\n'
        << "tau_c:                   " << config.tau_c << '\n'
        << "alpha:                   " << config.alpha << "\n\n"

        << "--- Simulation ---\n"
        << "Particles:               " << config.n_particles << '\n'
        << "dt:                      " << config.dt << '\n'
        << "Total time:              " << config.total_time << '\n'
        << "Physical steps:          " << steps << '\n'
        << "Burn-in steps:           "
        << config.burn_in_steps << '\n'
        << "Measurement time:        "
        << (config.total_time -
            static_cast<double>(config.burn_in_steps) *
                config.dt)
        << '\n'
        << "Particle updates:        "
        << config.n_particles * steps
        << "\n\n";
}

} // namespace

int main() {
    try {

        const ExperimentConfig config{
            .V1 = 0.2,
            .V2 = 0.1,

            .modulation_amplitude = 1.0,
            .tau_c = 0.075,
            .alpha = -1.0 / 3.0,

            .dt = 1.0e-3,
            .total_time = 250.0,
            .n_particles = 150'000,

            .burn_in_steps = 10'000,
            .x0 = 0.0,

            .seed = 45u,

            .requested_workers = 0,

            .cancellation_check_steps = 4'096
        };

        const SimulationParams simulation_params{
            .dt = config.dt,
            .total_time = config.total_time,
            .n_particles = config.n_particles,
            .burn_in_steps = config.burn_in_steps,
            .x0 = config.x0,

            .store_trajectory = false,

            /*
             * Пока не используются fast solver.
             * Они пригодятся для будущего interactive Qt mode.
             */
            .trajectory_stride = 1'000,
            .live_update_stride = 1'000,
            .batch_steps = 3'500,

            .cancellation_check_steps =
                config.cancellation_check_steps
        };

        simulation_params.validate();

        const std::size_t total_steps =
            simulation_params.step_count();

        const std::size_t total_updates =
            simulation_params.n_particles * total_steps;

        const std::size_t workers =
            effective_workers(config);

        print_configuration(
            config,
            total_steps,
            workers
        );

        const BiharmonicProfile profile{
            .V1 = config.V1,
            .V2 = config.V2
        };

        const Potential potential{profile};
        const SeedFactory seeds{config.seed};

        const DichotomicParams modulation_params =
            DichotomicParams::symmetric(
                config.modulation_amplitude,
                config.tau_c,
                config.dt,
                config.alpha
            );

        auto modulations =
            ModulationFactory::make_dichotomic_ensemble(
                modulation_params,
                config.n_particles,
                seeds
            );

        LangevinSolverCpu<BiharmonicProfile> solver{
            potential,
            simulation_params,
            seeds,
            workers
        };

        std::cout << "Simulation is running...\n\n";

        const auto started_at =
            std::chrono::steady_clock::now();

        const EnsembleResult result =
            solver.solve_stochastic_fast(modulations);

        const auto finished_at =
            std::chrono::steady_clock::now();

        const double elapsed_seconds =
            std::chrono::duration<double>(
                finished_at - started_at
            ).count();

        const double throughput =
            elapsed_seconds > 0.0
                ? static_cast<double>(total_updates) /
                  elapsed_seconds /
                  1.0e6
                : 0.0;

        std::cout
            << "=== Result ===\n\n"
            << std::scientific
            << std::setprecision(12)
            << "<x_final>:               "
            << result.mean_x_final << '\n'
            << "Mean drift velocity:     "
            << result.mean_velocity << "\n\n"

            << std::fixed
            << std::setprecision(3)
            << "Elapsed time:            "
            << elapsed_seconds << " s\n"
            << "Throughput:              "
            << throughput << " M updates/s\n"
            << "Workers:                 "
            << workers << '\n';

        return 0;
    } catch (const std::exception& exception) {
        std::cerr
            << "\nSimulation failed: "
            << exception.what()
            << '\n';

        return 1;
    }
}