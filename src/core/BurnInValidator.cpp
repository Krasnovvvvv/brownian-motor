#include "core/BurnInValidator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr std::size_t minimum_samples = 40;
constexpr std::size_t minimum_points_per_segment = 12;

constexpr double minimum_tolerance = 0.05;
constexpr double maximum_tolerance = 0.50;

constexpr double zero_tolerance = 1.0e-18;

struct RegressionResult {
    bool valid{false};

    double slope{0.0};
    double intercept{0.0};
    double r_squared{0.0};
};

[[nodiscard]] RegressionResult calculate_regression(
    const std::vector<BurnInSample>& samples,
    std::size_t first,
    std::size_t last_exclusive
) {
    RegressionResult result;

    const std::size_t count =
        last_exclusive - first;

    if (count < 2) {
        return result;
    }

    long double sum_time = 0.0L;
    long double sum_x = 0.0L;
    long double sum_time_squared = 0.0L;
    long double sum_time_x = 0.0L;

    for (
        std::size_t index = first;
        index < last_exclusive;
        ++index
    ) {
        const long double time =
            static_cast<long double>(
                samples[index].time
            );

        const long double mean_x =
            static_cast<long double>(
                samples[index].mean_x
            );

        sum_time += time;
        sum_x += mean_x;
        sum_time_squared += time * time;
        sum_time_x += time * mean_x;
    }

    const long double denominator =
        static_cast<long double>(count) *
        sum_time_squared -
        sum_time * sum_time;

    if (std::abs(denominator) < 1.0e-18L) {
        return result;
    }

    const long double slope =
        (
            static_cast<long double>(count) *
            sum_time_x -
            sum_time * sum_x
        ) / denominator;

    const long double intercept =
        (
            sum_x -
            slope * sum_time
        ) / static_cast<long double>(count);

    const long double mean_x =
        sum_x / static_cast<long double>(count);

    long double residual_sum = 0.0L;
    long double total_sum = 0.0L;

    for (
        std::size_t index = first;
        index < last_exclusive;
        ++index
    ) {
        const long double time =
            static_cast<long double>(
                samples[index].time
            );

        const long double value =
            static_cast<long double>(
                samples[index].mean_x
            );

        const long double fitted =
            intercept + slope * time;

        const long double residual =
            value - fitted;

        const long double deviation =
            value - mean_x;

        residual_sum += residual * residual;
        total_sum += deviation * deviation;
    }

    result.valid = true;
    result.slope = static_cast<double>(slope);
    result.intercept = static_cast<double>(intercept);
    result.r_squared = 1.0;

    if (total_sum > 1.0e-18L) {
        result.r_squared = static_cast<double>(
            1.0L - residual_sum / total_sum
        );
    }

    return result;
}

[[nodiscard]] bool are_samples_valid(
    const std::vector<BurnInSample>& samples
) {
    if (samples.size() < minimum_samples) {
        return false;
    }

    for (
        std::size_t index = 0;
        index < samples.size();
        ++index
    ) {
        const BurnInSample& sample =
            samples[index];

        if (
            !std::isfinite(sample.time) ||
            !std::isfinite(sample.mean_x)
        ) {
            return false;
        }

        if (
            index > 0 &&
            sample.time <= samples[index - 1].time
        ) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] double velocity_floor(
    const std::vector<BurnInSample>& samples
) {
    const double duration =
        samples.back().time -
        samples.front().time;

    if (
        !std::isfinite(duration) ||
        duration <= 0.0
    ) {
        return zero_tolerance;
    }

    double minimum_x =
        std::numeric_limits<double>::infinity();

    double maximum_x =
        -std::numeric_limits<double>::infinity();

    for (const BurnInSample& sample : samples) {
        minimum_x = std::min(
            minimum_x,
            sample.mean_x
        );

        maximum_x = std::max(
            maximum_x,
            sample.mean_x
        );
    }

    return std::max(
        0.01 * std::abs(maximum_x - minimum_x) /
            duration,
        zero_tolerance
    );
}

} // namespace

BurnInValidationResult BurnInValidator::validate(
    const std::vector<BurnInSample>& samples,
    double dt,
    double velocity_tolerance
) {
    BurnInValidationResult result;

    if (
        !std::isfinite(dt) ||
        dt <= 0.0 ||
        !std::isfinite(velocity_tolerance) ||
        velocity_tolerance < minimum_tolerance ||
        velocity_tolerance > maximum_tolerance ||
        !are_samples_valid(samples)
    ) {
        return result;
    }

    result.valid = true;
    result.tolerance = velocity_tolerance;

    const std::size_t minimum_tail_points =
        2 * minimum_points_per_segment;

    const std::size_t first_candidate =
        std::max(
            std::size_t{1},
            samples.size() / 10
        );

    const std::size_t last_candidate =
        samples.size() - minimum_tail_points;

    const double comparison_floor =
        velocity_floor(samples);

    for (
        std::size_t candidate = first_candidate;
        candidate <= last_candidate;
        ++candidate
    ) {
        const std::size_t tail_count =
            samples.size() - candidate;

        const std::size_t middle =
            candidate + tail_count / 2;

        if (
            middle - candidate <
                minimum_points_per_segment ||
            samples.size() - middle <
                minimum_points_per_segment
        ) {
            continue;
        }

        const RegressionResult early =
            calculate_regression(
                samples,
                candidate,
                middle
            );

        const RegressionResult late =
            calculate_regression(
                samples,
                middle,
                samples.size()
            );

        const RegressionResult tail =
            calculate_regression(
                samples,
                candidate,
                samples.size()
            );

        if (
            !early.valid ||
            !late.valid ||
            !tail.valid
        ) {
            continue;
        }

        const double mean_abs_velocity =
            0.5 * (
                std::abs(early.slope) +
                std::abs(late.slope)
            );

        const double denominator =
            std::max(
                mean_abs_velocity,
                comparison_floor
            );

        const double relative_difference =
            std::abs(
                late.slope - early.slope
            ) / denominator;

        if (relative_difference > velocity_tolerance) {
            continue;
        }

        result.stable = true;
        result.start_index = candidate;
        result.start_time = samples[candidate].time;

        result.early_velocity = early.slope;
        result.late_velocity = late.slope;
        result.tail_velocity = tail.slope;

        result.relative_velocity_difference =
            relative_difference;

        result.tail_r_squared =
            tail.r_squared;

        result.recommended_burn_in_steps =
            static_cast<std::size_t>(
                std::llround(
                    result.start_time / dt
                )
            );

        return result;
    }

    return result;
}