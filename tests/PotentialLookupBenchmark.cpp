#include "core/ExpressionEvaluator.h"
#include "core/Potential.h"
#include "core/PotentialConfigParser.h"
#include "profiles/BiharmonicProfile.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr double period = 1.0;
constexpr std::size_t grid_size = 4096;
constexpr std::size_t rounds = 256;
constexpr std::size_t repetitions = 5;
constexpr std::size_t validation_points = 131072;

volatile double benchmark_sink = 0.0;

double seconds(Clock::time_point first, Clock::time_point last) {
    return std::chrono::duration<double>(last - first).count();
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

class LookupTable {
public:
    LookupTable(
        const Potential<ExpressionEvaluator>& exact,
        std::size_t intervals
    )
        : intervals_(intervals),
          values_(intervals + 1) {

        for (std::size_t i = 0; i < intervals_; ++i) {
            const double x =
                period * static_cast<double>(i) /
                static_cast<double>(intervals_);

            values_[i] = exact.derivative_value(x, 1.0);

            if (!std::isfinite(values_[i])) {
                throw std::runtime_error(
                    "Non-finite force in lookup table"
                );
            }
        }

        values_[intervals_] = values_[0];
    }

    // Требуется x в диапазоне [0, period).
    double derivative_value(double x, double factor) const {
        const double scaled =
            x * static_cast<double>(intervals_) / period;

        const auto i = static_cast<std::size_t>(scaled);
        const double fraction = scaled - static_cast<double>(i);

        return factor * (
            values_[i] +
            fraction * (values_[i + 1] - values_[i])
        );
    }

    std::size_t bytes() const {
        return values_.size() * sizeof(double);
    }

private:
    std::size_t intervals_;
    std::vector<double> values_;
};

struct TimedRun {
    double elapsed{};
    double checksum{};
};

template <typename Force>
TimedRun measure_force(
    const std::vector<double>& coordinates,
    Force&& force
) {
    const auto begin = Clock::now();
    double total = 0.0;

    for (std::size_t round = 0; round < rounds; ++round) {
        const double offset =
            static_cast<double>(round % 31) * 1.0e-6;

        double local = 0.0;

        for (std::size_t i = 0; i < coordinates.size(); ++i) {
            double x = coordinates[i] + offset;
            if (x >= period) {
                x -= period;
            }

            double next_x = x + 0.00031;
            if (next_x >= period) {
                next_x -= period;
            }

            const double factor =
                i % 2 == 0 ? 1.0 : -1.0 / 3.0;

            local += force(x, factor);
            local += force(next_x, factor);
        }

        total += local;
        benchmark_sink = total;
    }

    return {seconds(begin, Clock::now()), total};
}

template <typename Force>
std::pair<double, double> time_five(
    const std::vector<double>& coordinates,
    Force&& force
) {
    std::vector<double> warmup(
        coordinates.begin(),
        coordinates.begin() + 64
    );
    (void)measure_force(warmup, force);

    std::vector<double> samples;
    samples.reserve(repetitions);

    double checksum = 0.0;

    for (std::size_t run = 0; run < repetitions; ++run) {
        const TimedRun result =
            measure_force(coordinates, force);

        samples.push_back(result.elapsed);
        checksum = result.checksum;
    }

    return {median(samples), checksum};
}

} // namespace

int main() {
    try {
        const QString path =
            QString::fromUtf8(BROWNIAN_MOTOR_SOURCE_DIR) +
            "/resources/potentials/"
            "biharmonic-ratchet.bmpotential";

        const auto parsed =
            PotentialConfigParser::parse_file(path);

        if (!parsed.is_success()) {
            std::cerr
                << "Potential parse failed: "
                << parsed.error_message.toStdString()
                << '\n';
            return EXIT_FAILURE;
        }

        const PotentialParameterValues parameters{
            {"V1", 0.2},
            {"V2", 0.1}
        };

        const Potential<ExpressionEvaluator> exact{
            ExpressionEvaluator{
                *parsed.definition,
                parameters
            }
        };

        const Potential<BiharmonicProfile> native{
            BiharmonicProfile{
                .V1 = 0.2,
                .V2 = 0.1
            }
        };

        std::vector<double> coordinates;
        coordinates.reserve(grid_size);

        for (std::size_t i = 0; i < grid_size; ++i) {
            coordinates.push_back(
                std::fmod(
                    static_cast<double>(i) *
                        0.6180339887498948,
                    period
                )
            );
        }

        // Проверяем, что выбранная формула действительно
        // периодична и совпадает со встроенным профилем.
        for (std::size_t i = 0; i < 256; ++i) {
            const double x =
                (static_cast<double>(i) + 0.37) / 256.0;

            const double a =
                exact.derivative_value(x, 1.0);

            const double periodic =
                exact.derivative_value(x + period, 1.0);

            const double builtin =
                native.derivative_value(x, 1.0);

            const double tolerance =
                1.0e-9 *
                std::max(1.0, std::abs(a));

            if (!std::isfinite(a) ||
                std::abs(a - periodic) > tolerance ||
                std::abs(a - builtin) > tolerance) {
                std::cerr
                    << "Periodicity or built-in comparison "
                       "failed at x="
                    << x << '\n';
                return EXIT_FAILURE;
            }
        }

        std::vector<double> reference;
        reference.reserve(validation_points);

        double maximum_reference = 0.0;

        for (std::size_t i = 0;
             i < validation_points;
             ++i) {
            const double x =
                (static_cast<double>(i) + 0.5) /
                static_cast<double>(validation_points);

            const double force =
                exact.derivative_value(x, 1.0);

            if (!std::isfinite(force)) {
                throw std::runtime_error(
                    "Non-finite reference force"
                );
            }

            reference.push_back(force);
            maximum_reference =
                std::max(
                    maximum_reference,
                    std::abs(force)
                );
        }

        const auto native_force =
            [&native](double x, double factor) {
                return native.derivative_value(
                    x, factor
                );
            };

        const auto exact_force =
            [&exact](double x, double factor) {
                return exact.derivative_value(
                    x, factor
                );
            };

        const auto [native_time, native_checksum] =
            time_five(coordinates, native_force);

        const auto [exact_time, exact_checksum] =
            time_five(coordinates, exact_force);

        std::cout
            << std::fixed
            << std::setprecision(9)
            << "period: " << period << '\n'
            << "derivative evaluations per timed run: "
            << 2 * grid_size * rounds << '\n'
            << "native median (s): "
            << native_time
            << ", checksum: "
            << native_checksum << '\n'
            << "ExprTk median (s): "
            << exact_time
            << ", checksum: "
            << exact_checksum << '\n'
            << "validation points per table: "
            << validation_points << '\n'
            << "intervals | bytes | build (s) | "
               "median (s) | ExprTk/table | "
               "max abs error | rms error | "
               "max error / peak force | checksum\n";

        for (const std::size_t intervals :
             std::array<std::size_t, 3>{
                 4096, 8192, 16384
             }) {
            const auto build_start = Clock::now();

            const LookupTable table{
                exact,
                intervals
            };

            const double build_time =
                seconds(build_start, Clock::now());

            double max_error = 0.0;
            double squared_error = 0.0;

            for (std::size_t i = 0;
                 i < validation_points;
                 ++i) {
                const double x =
                    (static_cast<double>(i) + 0.5) /
                    static_cast<double>(
                        validation_points
                    );

                const double difference =
                    table.derivative_value(x, 1.0) -
                    reference[i];

                max_error =
                    std::max(
                        max_error,
                        std::abs(difference)
                    );

                squared_error +=
                    difference * difference;
            }

            const double rms_error =
                std::sqrt(
                    squared_error /
                    static_cast<double>(
                        validation_points
                    )
                );

            const auto lookup_force =
                [&table](double x, double factor) {
                    return table.derivative_value(
                        x, factor
                    );
                };

            const auto [
                lookup_time,
                lookup_checksum
            ] = time_five(
                coordinates,
                lookup_force
            );

            std::cout
                << intervals
                << " | " << table.bytes()
                << " | " << build_time
                << " | " << lookup_time
                << " | " << exact_time / lookup_time
                << " | " << max_error
                << " | " << rms_error
                << " | "
                << max_error /
                   maximum_reference
                << " | " << lookup_checksum
                << '\n';
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr
            << "Benchmark failed: "
            << error.what() << '\n';
        return EXIT_FAILURE;
    }
}