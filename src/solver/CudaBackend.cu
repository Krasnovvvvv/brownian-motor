#include "solver/CudaBackend.h"
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <stdexcept>

namespace bm_cuda {
    static void check(cudaError_t e) {
        if (e != cudaSuccess) throw std::runtime_error(cudaGetErrorString(e));
    }

    std::string device() {
        int count = 0, id = 0;
        check(cudaGetDeviceCount(&count));
        if (!count) throw std::runtime_error("No CUDA device found");
        check(cudaGetDevice(&id));
        cudaDeviceProp p{};
        check(cudaGetDeviceProperties(&p, id));
        check(cudaFree(nullptr));
        return p.name;
    }

    using Random = curandStatePhilox4_32_10_t;

    template<class T>
    struct Buffer {
        T *p = nullptr;
        ~Buffer() { if (p) cudaFree(p); }
        void allocate(std::size_t n) { if (n) check(cudaMalloc(reinterpret_cast<void **>(&p), n * sizeof(T))); }

        void upload(const T *src, std::size_t n) {
            if (n) check(cudaMemcpy(p, src, n * sizeof(T), cudaMemcpyHostToDevice));
        }
    };

    struct Context {
        Config c;
        std::size_t n, ns;
        Buffer<Particle> particles;
        Buffer<Random> rng, mod;
        Buffer<Segment> segments;
        Buffer<double> samples, factors;
        Buffer<int> invalid;
    };

    void Destroy::operator()(Context *p) const noexcept { delete p; }

__global__ void seed_rng(Random *rng, Random *mod, Particle *p, Config c, std::size_t n, unsigned long long seed) {
        auto i = std::size_t(blockIdx.x) * blockDim.x + threadIdx.x;
        if (i < n) {
            curand_init(seed, i, 0, rng + i);
            if (c.stochastic) {
                curand_init(seed, i + n, 0, mod + i);
                p[i].in_a = 1.0 - curand_uniform_double(mod + i) < c.stationary;
            }
        }
    }

    Handle create(Config c, const Segment *segments, std::size_t ns, const double *samples,
                  std::size_t nm, const Particle *particles, std::size_t n, unsigned long long seed) {
        Handle h(new Context{});
        h->c = c;
        h->n = n;
        h->ns = ns;
        h->particles.allocate(n);
        h->particles.upload(particles, n);
        h->segments.allocate(ns);
        h->segments.upload(segments, ns);
        h->samples.allocate(nm);
        h->samples.upload(samples, nm);
        h->rng.allocate(n);
        h->factors.allocate(1024);
        h->invalid.allocate(1);
        if (c.stochastic) h->mod.allocate(n);
        seed_rng<<<(n - 1) / 128 + 1,128>>>(h->rng.p, h->mod.p, h->particles.p, c, n, seed);
        check(cudaGetLastError());
        return h;
    }

__device__ double force(Config c, const Segment *seg, std::size_t ns, const double *y, double x, double g) {
        x -= c.period * floor(x / c.period);
        if (!ns) {
            constexpr double tau = 6.2831853071795864769;
            double a = cos(tau * x);
            return -g * (tau * c.v1 * a + 2 * tau * c.v2 * (2 * a * a - 1));
        }
        std::size_t lo = 0, hi = ns;
        while (hi - lo > 1) {
            auto mid = lo + (hi - lo) / 2;
            if (x < seg[mid].left) hi = mid;
            else lo = mid;
        }
        auto row = seg[lo];
        const auto *a = y + row.offset;
        if (row.constant) return -g * a[0];
        double u = (x - row.left) * row.inv;
        if (u <= 0) return -g * a[0];
        if (u >= row.intervals) return -g * a[c.piecewise ? row.intervals : 0];
        auto i = static_cast<std::size_t>(u);
        double t = u - i;
        double value = ((a[i] <= 0 && a[i + 1] >= 0) || (a[i] >= 0 && a[i + 1] <= 0))
                           ? t * a[i + 1] + (1 - t) * a[i]
                           : a[i] + t * (a[i + 1] - a[i]);
        return -g * value;
    }

__global__ void evolve(Config c, const Segment *seg, std::size_t ns, const double *y,
                       Particle *p, Random *rng, Random *mod, std::size_t n, const double *factors,
                       std::size_t steps, std::size_t first, std::size_t burn, int *invalid) {
        auto i = std::size_t(blockIdx.x) * blockDim.x + threadIdx.x;
        if (i >= n) return;
        auto z = p[i];
        auto r = rng[i];
        Random mr;
        if (c.stochastic) mr = mod[i];
        for (std::size_t j = 0; j < steps; ++j) {
            double g;
            if (c.stochastic) {
                double u = 1.0 - curand_uniform_double(&mr);
                z.in_a = z.in_a ? u < c.paa : u >= c.pbb;
                g = z.in_a ? c.fa : c.fb;
            } else g = factors[j];
            const double f = force(c, seg, ns, y, z.x, g);
            if (c.inertial) {
                auto u = curand_normal2_double(&r);
                double ex = c.xx * u.x + c.xy * u.y, ev = c.vx * u.x + c.vy * u.y;
                double xp = z.x + (c.B * z.v + c.C * f + ex);
                if (!isfinite(xp)) {
                    atomicExch(invalid, 1);
                    return;
                }
                double k = 0.5 * f + 0.5 * force(c, seg, ns, y, xp, g);
                z.x += c.B * z.v + c.C * k + ex;
                z.v = c.a * z.v + c.q * k + ev;
                z.cv += c.B * z.d + c.C * k;
                z.d = c.a * z.d + c.q * k;
            } else {
                double ex = c.xx * curand_normal_double(&r), xp = z.x + c.h * f + ex;
                if (!isfinite(xp)) {
                    atomicExch(invalid, 1);
                    return;
                }
                double dx = c.h * (0.5 * f + 0.5 * force(c, seg, ns, y, xp, g));
                z.x += dx + ex;
                z.cv += dx;
            }
            if (!isfinite(z.x) || !isfinite(z.v) || !isfinite(z.cv) || !isfinite(z.d)) {
                atomicExch(invalid, 1);
                return;
            }
            if (first + j + 1 == burn) {
                z.xb = z.x;
                z.cv = 0;
            }
        }
        p[i] = z;
        rng[i] = r;
        if (c.stochastic) mod[i] = mr;
    }

    void advance(Context &h, const double *factors, std::size_t n, std::size_t first, std::size_t burn) {
        if (n > 1024) throw std::invalid_argument("CUDA batch too large");
        if (!h.c.stochastic) h.factors.upload(factors, n);
        check(cudaMemset(h.invalid.p, 0, sizeof(int)));
        evolve<<<(h.n - 1) / 128 + 1,128>>>(h.c, h.segments.p, h.ns, h.samples.p, h.particles.p,
                                            h.rng.p, h.mod.p, h.n, h.factors.p, n, first, burn, h.invalid.p);
        check(cudaGetLastError());
        int bad = 0;
        check(cudaMemcpy(&bad, h.invalid.p, sizeof(int), cudaMemcpyDeviceToHost));
        if (bad) throw std::runtime_error("Non-finite CUDA trajectory");
    }

    void snapshot(Context &h, Particle *dst) {
        check(cudaMemcpy(dst, h.particles.p, h.n * sizeof(Particle), cudaMemcpyDeviceToHost));
    }
}
