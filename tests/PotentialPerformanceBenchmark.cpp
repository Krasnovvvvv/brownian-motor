#include "core/ExpressionEvaluator.h"
#include "core/Potential.h"
#include "core/PotentialConfigParser.h"
#include "profiles/BiharmonicProfile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

volatile double benchmark_sink = 0.0;

[[nodiscard]] double elapsed_seconds(
    Clock::time_point start,
    Clock::time_point end
) {
    return std::chrono::duration<double>(end - start).count();
}

[[nodiscard]] double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

struct TimedRun {
    double seconds{};
    double checksum{};
};

template <typename Force>
[[nodiscard]] TimedRun measure_force(
    const std::vector<double>& coordinates,
    std::size_t rounds,
    Force&& force
) {
    const auto start = Clock::now();
    double total = 0.0;

    for (std::size_t round = 0; round < rounds; ++round) {
        const double offset =
            static_cast<double>(round % 31) * 1.0e-6;
        double local = 0.0;

        for (std::size_t i = 0; i < coordinates.size(); ++i) {
            const double x = coordinates[i] + offset;
            const double factor = i % 2 == 0 ? 1.0 : -1.0 / 3.0;
            local += force(x, factor);
            local += force(x + 0.00031, factor);
        }

        total += local;
        benchmark_sink = total;
    }

    const auto end = Clock::now();
    return {elapsed_seconds(start, end), total};
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const std::size_t worker_count = argc > 1
            ? std::stoull(argv[1])
            : std::min<std::size_t>(
                8,
                std::max(1u, std::thread::hardware_concurrency())
            );

        if (worker_count == 0 || worker_count > 64) {
            std::cerr << "Worker count must be between 1 and 64.\n";
            return EXIT_FAILURE;
        }

        const QString path =
            QString::fromUtf8(BROWNIAN_MOTOR_SOURCE_DIR) +
            "/resources/potentials/biharmonic-ratchet.bmpotential";

        const auto parsed = PotentialConfigParser::parse_file(path);
        if (!parsed.is_success()) {
            std::cerr << "Potential parse failed: "
                      << parsed.error_message.toStdString() << '\n';
            return EXIT_FAILURE;
        }

        const PotentialDefinition& definition = *parsed.definition;
        const PotentialParameterValues parameters{
            {"V1", 0.2},
            {"V2", 0.1}
        };

        const Potential<BiharmonicProfile> native{
            BiharmonicProfile{.V1 = 0.2, .V2 = 0.1}
        };

        const auto setup_begin = Clock::now();
        const Potential<ExpressionEvaluator> runtime{
            ExpressionEvaluator{definition, parameters}
        };
        const auto prototype_ready = Clock::now();

        std::vector<Potential<ExpressionEvaluator>> worker_potentials;
        worker_potentials.reserve(worker_count);
        for (std::size_t worker = 0; worker < worker_count; ++worker) {
            worker_potentials.push_back(runtime);
        }
        const auto workers_ready = Clock::now();

        constexpr std::size_t grid_size = 4096;
        constexpr std::size_t rounds = 256;
        constexpr std::size_t repetitions = 5;

        std::vector<double> coordinates;
        coordinates.reserve(grid_size);
        for (std::size_t i = 0; i < grid_size; ++i) {
            coordinates.push_back(
                std::fmod(
                    static_cast<double>(i) * 0.6180339887498948,
                    1.0
                )
            );
        }

        const auto native_force = [&native](double x, double factor) {
            return native.derivative_value(x, factor);
        };
        const auto expression_force = [&worker_potentials](
            double x,
            double factor
        ) {
            return worker_potentials.front().derivative_value(x, factor);
        };

        for (std::size_t i = 0; i < 64; ++i) {
            const double x = coordinates[i];
            const double a = native_force(x, -1.0 / 3.0);
            const double b = expression_force(x, -1.0 / 3.0);
            if (std::abs(a - b) > 1.0e-10 * std::max({1.0, std::abs(a), std::abs(b)})) {
                std::cerr << "Force mismatch at x = " << x << '\n';
                return EXIT_FAILURE;
            }
        }

        (void)measure_force(coordinates, 4, native_force);
        (void)measure_force(coordinates, 4, expression_force);

        std::vector<double> native_times;
        std::vector<double> expression_times;
        native_times.reserve(repetitions);
        expression_times.reserve(repetitions);
        double native_checksum = 0.0;
        double expression_checksum = 0.0;

        for (std::size_t run = 0; run < repetitions; ++run) {
            const auto record_native = [&] {
                const TimedRun measured = measure_force(
                    coordinates, rounds, native_force
                );
                native_times.push_back(measured.seconds);
                native_checksum = measured.checksum;
            };
            const auto record_expression = [&] {
                const TimedRun measured = measure_force(
                    coordinates, rounds, expression_force
                );
                expression_times.push_back(measured.seconds);
                expression_checksum = measured.checksum;
            };

            if (run % 2 == 0) {
                record_native();
                record_expression();
            } else {
                record_expression();
                record_native();
            }
        }

        const double native_median = median(native_times);
        const double expression_median = median(expression_times);
        const std::size_t evaluations = 2 * grid_size * rounds;

        std::cout << std::fixed << std::setprecision(6)
                  << "worker copies: " << worker_count << '\n'
                  << "initial expression compile (s): "
                  << elapsed_seconds(setup_begin, prototype_ready) << '\n'
                  << "worker expression copies and recompiles (s): "
                  << elapsed_seconds(prototype_ready, workers_ready) << '\n'
                  << "derivative evaluations per timed run: "
                  << evaluations << '\n'
                  << "native median (s): " << native_median << '\n'
                  << "ExprTk median (s): " << expression_median << '\n'
                  << "ExprTk / native: "
                  << expression_median / native_median << "x\n"
                  << "native checksum: " << native_checksum << '\n'
                  << "ExprTk checksum: " << expression_checksum << '\n';

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}