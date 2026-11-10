#include "core/PotentialDerivativeCheck.h"
#include "core/ExpressionEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
    std::string number(double x) {
        std::ostringstream out;
        out << std::setprecision(12) << x;
        return out.str();
    }

    bool close(double a, double b, double absolute, double relative) {
        if (!std::isfinite(a) || !std::isfinite(b)) return false;
        const double scale = std::max(std::abs(a), std::abs(b));
        return std::abs(a - b) <= absolute + relative * scale;
    }

    std::optional<std::string> check_point(
        const PotentialDefinition &d, const ExpressionEvaluator &e,
        double x, double h
    ) {
        const double left = x - h;
        const double right = x + h;
        if (!(left < x && x < right)) {
            return "Derivative check: interval is too narrow for finite differences.";
        }
        const double declared = e.derivative(x);
        const double ul = e.value(left);
        const double ur = e.value(right);
        const double estimated = (ur - ul) / (right - left);
        if (!std::isfinite(declared) || !std::isfinite(ul) ||
            !std::isfinite(ur) || !std::isfinite(estimated)) {
            return "Derivative check: non-finite value near " + d.coordinate.symbol + " = " + number(x);
        }
        if (!close(declared, estimated, 1.0e-6, 5.0e-3)) {
            return "Derivative mismatch at " + d.coordinate.symbol + " = " + number(x) +
                   ": declared = " + number(declared) + ", estimated = " + number(estimated);
        }
        return std::nullopt;
    }

    // First-order extrapolation from strictly inside an interval.
    double endpoint_limit(const ExpressionEvaluator &e, double lo, double hi, bool right) {
        const double endpoint = right ? hi : lo;
        double x = right ? std::lerp(lo, hi, 1.0 - 1.0e-7)
                         : std::lerp(lo, hi, 1.0e-7);
        if (!(lo < x && x < hi)) {
            x = std::nextafter(endpoint, right ? lo : hi);
        }
        if (!(lo < x && x < hi)) return std::numeric_limits<double>::quiet_NaN();
        const double u = e.value(x);
        const double du = e.derivative(x);
        if (!std::isfinite(u) || !std::isfinite(du)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return u + (endpoint - x) * du;
    }
}

std::optional<std::string> check_potential_derivative(
    const PotentialDefinition &definition, const ExpressionEvaluator &evaluator
) {
    const double scale = definition.profile.periodic ? definition.profile.period : 2.0;
    if (!std::isfinite(scale) || scale <= 0.0) {
        return "Derivative check: invalid coordinate scale.";
    }

    if (!evaluator.piecewise()) {
        constexpr std::array<double, 9> points{
            -0.83, -0.57, -0.31, -0.13, 0.0, 0.17, 0.39, 0.68, 0.91
        };
        const double h = 1.0e-5 * scale;
        if (!std::isfinite(h) || h <= 0.0) {
            return "Derivative check: cannot choose a finite difference step.";
        }
        for (const double t : points) {
            if (auto error = check_point(definition, evaluator, t * scale, h)) return error;
        }
        return std::nullopt;
    }

    std::vector<double> edges{0.0};
    edges.insert(edges.end(), evaluator.breakpoints().begin(), evaluator.breakpoints().end());
    edges.push_back(scale);
    constexpr std::array<double, 9> fractions{
        0.01, 0.05, 0.13, 0.29, 0.50, 0.71, 0.87, 0.95, 0.99
    };
    std::vector<double> left_limits;
    std::vector<double> right_limits;
    left_limits.reserve(edges.size() - 1);
    right_limits.reserve(edges.size() - 1);

    for (std::size_t i = 0; i + 1 < edges.size(); ++i) {
        const double lo = edges[i];
        const double hi = edges[i + 1];
        const double width = hi - lo;
        if (!std::isfinite(width) || width <= 0.0) return "Invalid piecewise interval.";
        for (const double t : fractions) {
            const double x = std::lerp(lo, hi, t);
            const double margin = std::min(x - lo, hi - x);
            const double h = std::min(1.0e-5 * width, 0.25 * margin);
            if (!(h > 0.0 && lo < x - h && x + h < hi)) {
                return "Piecewise interval is too narrow for derivative validation.";
            }
            if (auto error = check_point(definition, evaluator, x, h)) return error;
        }
        const double ul = endpoint_limit(evaluator, lo, hi, false);
        const double ur = endpoint_limit(evaluator, lo, hi, true);
        if (!std::isfinite(ul) || !std::isfinite(ur)) {
            return "Non-finite one-sided potential limit on interval " + std::to_string(i);
        }
        left_limits.push_back(ul);
        right_limits.push_back(ur);
    }

    for (std::size_t i = 1; i < left_limits.size(); ++i) {
        if (!close(right_limits[i - 1], left_limits[i], 1.0e-8, 1.0e-6)) {
            return "Potential is not continuous at breakpoint " + number(edges[i]) +
                   ". Potential jumps are not supported.";
        }
    }
    if (!close(right_limits.back(), left_limits.front(), 1.0e-8, 1.0e-6)) {
        return "Potential is not continuous at the periodic seam. Potential jumps are not supported.";
    }
    return std::nullopt;
}
