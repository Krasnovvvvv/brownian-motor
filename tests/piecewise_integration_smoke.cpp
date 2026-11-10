// Build as a SEPARATE test executable using the project's real Qt/ExprTk setup.
// Do not add this main() to the application's existing executable.
#include "core/PotentialConfigParser.h"
#include "core/ExpressionEvaluator.h"
#include "profiles/TabulatedPeriodicProfile.h"

#include <QCoreApplication>
#include <QString>
#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition) throw std::runtime_error(message);
    }
    void near(double actual, double expected, const std::string &message) {
        const double tolerance = 1.0e-12 * std::max(1.0, std::abs(expected));
        require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
    }
    double phase(double x) {
        double p = std::fmod(x, 1.0);
        if (p < 0.0) p += 1.0;
        return p >= 1.0 ? 0.0 : p;
    }
    template<class F> void must_throw(F &&f, const std::string &message) {
        bool rejected = false;
        try { f(); } catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, message);
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        std::cerr << "Usage: piecewise_integration_smoke <path-to-file.bmpotential>\n";
        return 2;
    }
    try {
        const auto result = PotentialConfigParser::parse_file(QString::fromLocal8Bit(argv[1]));
        if (!result.definition) {
            throw std::runtime_error(result.error_message.toStdString());
        }
        const PotentialDefinition definition = *result.definition;
        require(definition.profile.piecewise, "JSON did not enable piecewise mode");
        require(definition.profile.period == 1.0, "Unexpected period");
        require(!definition.source_file_path.empty(), "Source path was not preserved");
        std::size_t checked = 0;
        for (const double a : {0.01, 0.3, 0.5, 0.99}) {
            for (const double h : {0.0, 1.0, 2.0}) {
                ExpressionEvaluator exact{definition, {{"H", h}, {"a", a}}};
                require(exact.piecewise(), "Evaluator lost piecewise mode");
                require(exact.breakpoints().size() == 1, "Unexpected breakpoint count");
                near(exact.breakpoints().front(), a, "Breakpoint did not follow selected a");
                TabulatedPeriodicProfile<ExpressionEvaluator> fast{exact, 1.0};
                const double rising = h/a;
                const double falling = -h/(1.0-a);
                near(exact.value(0.0), 0.0, "U(0)");
                near(exact.value(a), h, "U(a)");
                near(exact.value(1.0), 0.0, "U(1)");
                near(fast.derivative(std::nextafter(a, 0.0)), rising, "Left of peak was smeared");
                near(fast.derivative(a), falling, "Peak must select right segment");
                near(fast.derivative(std::nextafter(a, 1.0)), falling, "Right of peak");
                near(fast.derivative(std::nextafter(1.0, 0.0)), falling, "Seam was smeared");
                near(fast.derivative(1.0), rising, "Periodic endpoint must wrap");
                for (const double x : {-1.0, -0.1, 0.0, 0.1, 0.299999, 0.3,
                                       0.300001, 0.9, 0.999999, 1.0, 1.1}) {
                    const double q = phase(x);
                    const double expected_u = q < a ? h*q/a : h*(1.0-q)/(1.0-a);
                    const double expected_du = q < a ? rising : falling;
                    near(exact.value(x), expected_u, "Periodic U mismatch");
                    near(exact.derivative(x), expected_du, "Exact derivative mismatch");
                    near(fast.derivative(x), expected_du, "Tabulated derivative mismatch");
                    near(fast.derivative_scaled(x, 2.0), 2.0*expected_du, "Modulation mismatch");
                    ++checked;
                }
                for (int i=0; i<10000; ++i) {
                    const double x = (i+0.123)/10000.0;
                    near(fast.derivative(x), x < a ? rising : falling, "Interior lookup mismatch");
                    ++checked;
                }
                require(fast.table_bytes() == 2*sizeof(double), "Constant segments were not compressed");
                std::cout << "OK: H=" << h << " a=" << a
                          << " sample_bytes=" << fast.table_bytes() << '\n';
            }
        }
        must_throw([&] {
            auto bad = definition;
            bad.profile.breakpoint_expressions = {"x/2"};
            ExpressionEvaluator evaluator{bad};
        }, "Coordinate-dependent breakpoint was accepted");
        must_throw([&] {
            auto bad = definition;
            bad.profile.breakpoint_expressions = {"0.7", "0.2"};
            ExpressionEvaluator evaluator{bad};
        }, "Unordered breakpoints were accepted");
        must_throw([&] {
            auto bad = definition;
            bad.profile.derivative_expression = "0";
            ExpressionEvaluator evaluator{bad};
        }, "Incorrect derivative was accepted");
        must_throw([&] {
            auto bad = definition;
            bad.profile.expression = "if(x < a, H*x/a, H*(1-x)/(1-a)+0.1)";
            ExpressionEvaluator evaluator{bad};
        }, "Discontinuous potential was accepted");
        std::cout << "PASS: " << checked
                  << " interior/periodic coordinate checks; boundary and rejection checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
