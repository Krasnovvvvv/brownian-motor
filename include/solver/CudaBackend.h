#pragma once
#include <cstddef>
#include <string>
#include <memory>

namespace bm_cuda {
    std::string device(); // throws if unavailable
    struct Segment {
        double left, inv;
        std::size_t offset, intervals;
        bool constant;
    };

    struct Config {
        double period, v1, v2, h, a, q, B, C, vx, vy, xx, xy;
        bool inertial, piecewise, stochastic;
        double paa, pbb, stationary, fa, fb;
    };

    struct Particle {
        double x, v, d, xb, cv;
        bool in_a;
    };

    struct Context;

    struct Destroy {
        void operator()(Context *) const noexcept;
    };

    using Handle = std::unique_ptr<Context, Destroy>;

    Handle create(Config, const Segment *, std::size_t, const double *, std::size_t,
                  const Particle *, std::size_t, unsigned long long);

    void advance(Context &, const double *, std::size_t, std::size_t, std::size_t);

    void snapshot(Context &, Particle *);
}
