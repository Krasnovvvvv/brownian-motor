#include "core/InitialPositionDistribution.h"
#include "core/SimulationParams.h"
#include "core/Potential.h"
#include "solver/LangevinSolverCpu.h"
#include "fixtures/LegacyLangevinSolverCpu.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool test, const std::string &message) {
        if (!test) throw std::runtime_error(message);
    }
    void near(double actual, double expected, double tolerance, const std::string &message) {
        require(std::isfinite(actual) && std::isfinite(expected) &&
                std::abs(actual-expected) <= tolerance*std::max({1.0,std::abs(actual),std::abs(expected)}),message);
    }
    template<class F> void must_reject(F f) {
        bool rejected=false;
        try {f();} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"invalid initialization was accepted");
    }
    struct Profile {
        double strength{};
        double operator()(double x) const noexcept {return strength*std::cos(2*std::numbers::pi*x);}
        double value(double x) const noexcept {return (*this)(x);}
        double derivative(double x) const noexcept {return -2*std::numbers::pi*strength*std::sin(2*std::numbers::pi*x);}
    };
    struct Constant {
        double step() noexcept {return 1.0;}
    };
    SimulationParams params(bool interactive=false) {
        SimulationParams p;
        p.dt=0.001; p.total_time=0.024;
        p.n_particles=64; p.burn_in_steps=0; p.x0=-0.2;
        p.store_trajectory=interactive;
        p.batch_steps=7; p.trajectory_stride=7; p.live_update_stride=7;
        return p;
    }
    EnsembleResult run(SimulationParams p, std::size_t workers=1, double strength=0.0) {
        LangevinSolverCpu<Profile> solver{Potential<Profile>{Profile{strength}},p,SeedFactory{42},workers};
        Constant modulation;
        if (p.store_trajectory) return solver.solve_deterministic_interactive(modulation);
        return solver.solve_deterministic_fast(modulation);
    }
    void compare(const EnsembleResult &a, const EnsembleResult &b, double tolerance) {
        near(a.mean_velocity,b.mean_velocity,tolerance,"mean velocity mismatch");
        near(a.mean_x_final,b.mean_x_final,tolerance,"final position mismatch");
        require(a.has_velocity_statistics==b.has_velocity_statistics,"statistics availability mismatch");
        require(a.has_control_variate_statistics==b.has_control_variate_statistics,"CV availability mismatch");
        near(a.mean_displacement,b.mean_displacement,tolerance,"displacement mismatch");
        near(a.displacement_variance,b.displacement_variance,tolerance,"variance mismatch");
        near(a.velocity_standard_error,b.velocity_standard_error,tolerance,"SE mismatch");
        near(a.control_variate_velocity,b.control_variate_velocity,tolerance,"CV velocity mismatch");
        near(a.control_variate_standard_error,b.control_variate_standard_error,tolerance,"CV SE mismatch");
        require(a.sim_time==b.sim_time,"trajectory times mismatch");
        require(a.mean_x.size()==b.mean_x.size(),"trajectory length mismatch");
        for(std::size_t i=0;i<a.mean_x.size();++i) near(a.mean_x[i],b.mean_x[i],tolerance,"trajectory mismatch");
    }
}

int main() {
    try {
        int checks=0;
        const SeedFactory seeds{42};
        InitialPositionDistribution settings;
        settings.kind=InitialPositionKind::Uniform;
        settings.lower=2.0; settings.upper=3.0;
        std::vector<double> samples;
        for(std::size_t i=0;i<8192;++i) {
            const double x=settings.sample(0,seeds,i);
            require(x>=2.0 && x<3.0,"uniform bounds/wrapping");
            require(x==settings.sample(0,seeds,i),"repeatability");
            samples.push_back(x);
        }
        auto summary=summarize_initial_positions(samples);
        near(summary.mean,2.5,0.01,"uniform mean");
        near(summary.variance,1.0/12.0,0.006,"uniform variance");
        ++checks;
        settings.kind=InitialPositionKind::Gaussian;
        settings.mean=-2.0; settings.standard_deviation=0.3;
        samples.clear();
        for(std::size_t i=0;i<8192;++i) samples.push_back(settings.sample(0,seeds,i));
        summary=summarize_initial_positions(samples);
        near(summary.mean,-2.0,0.01,"Gaussian mean");
        near(summary.variance,0.09,0.006,"Gaussian variance");
        require(summary.mean<0,"Gaussian was wrapped");
        ++checks;
        auto thermal_before=seeds.make_mt19937(7,SeedFactory::Stream::GaussianThermal);
        for(std::size_t i=0;i<20;++i) (void)settings.sample(0,seeds,i);
        auto thermal_after=seeds.make_mt19937(7,SeedFactory::Stream::GaussianThermal);
        for(int i=0;i<20;++i) require(thermal_before()==thermal_after(),"initialization changed thermal RNG");
        ++checks;
        must_reject([&]{auto s=settings;s.standard_deviation=0;s.validate(0);});
        must_reject([&]{auto s=settings;s.kind=InitialPositionKind::Uniform;s.lower=1;s.upper=1;s.validate(0);});
        checks+=2;

        // Exact regression against the supplied original solver for Point mode.
        for(bool interactive:{false,true}) for(std::size_t burn:{0u,5u}) {
            auto p=params(interactive); p.burn_in_steps=burn;
            const auto current=run(p,3,0.08);
            LegacyLangevinSolverCpu<Profile> previous{
                Potential<Profile>{Profile{0.08}},p,SeedFactory{42},3};
            Constant modulation;
            const auto legacy=interactive ? previous.solve_deterministic_interactive(modulation)
                                          : previous.solve_deterministic_fast(modulation);
            compare(current,legacy,0.0);
            ++checks;
        }

        for(auto kind:{InitialPositionKind::Point,InitialPositionKind::Uniform,InitialPositionKind::Gaussian}) {
            for(std::size_t burn:{0u,5u}) {
                auto p=params(); p.burn_in_steps=burn;
                p.initial_positions.kind=kind;
                p.initial_positions.lower=2.0; p.initial_positions.upper=3.0;
                p.initial_positions.mean=3.0; p.initial_positions.standard_deviation=0.2;
                const auto one=run(p,1,0.08);
                const auto many=run(p,3,0.08);
                compare(one,many,1e-11);
                require(one.initial_mean_position==many.initial_mean_position,"initial mean depends on workers");
                require(one.initial_position_variance==many.initial_position_variance,"initial variance depends on workers");
                p.store_trajectory=true;
                const auto interactive=run(p,3,0.08);
                auto no_trajectory=interactive;
                no_trajectory.mean_x.clear(); no_trajectory.sim_time.clear();
                compare(many,no_trajectory,1e-11);
                ++checks;
            }
        }

        // Independent initial cloud must not create artificial drift in a flat potential.
        auto p=params();
        p.initial_positions.kind=InitialPositionKind::Gaussian;
        p.initial_positions.mean=1000.0;
        p.initial_positions.standard_deviation=0.1;
        const auto flat=run(p,3);
        require(flat.control_variate_velocity==0.0,"spurious free-diffusion CV drift");
        require(flat.control_variate_standard_error==0.0,"spurious free-diffusion CV error");
        require(flat.initial_mean_position>999.0,"initial positions lost their unwrapped offset");
        p.store_trajectory=true;
        LangevinSolverCpu<Profile> interactive{Potential<Profile>{Profile{}},p,seeds,3};
        double observed=0.0;
        const SimulationObserver observer=[&](const SimulationUpdate &u){observed=u.mean_velocity;};
        Constant modulation;
        const auto result=interactive.solve_deterministic_interactive(modulation,observer);
        near(observed,result.mean_velocity,1e-9,"observer used x0 instead of actual initial mean");
        near(flat.mean_velocity,result.mean_velocity,1e-10,"flat fast/interactive mismatch");
        ++checks;
        p=params(); p.initial_positions.kind=InitialPositionKind::Gaussian;
        p.initial_positions.mean=-3; p.initial_positions.standard_deviation=0.2;
        p.periodic_profile=false;
        const auto nonperiodic=run(p,2);
        require(nonperiodic.initial_mean_position< -2,"non-periodic positions were wrapped");
        ++checks;
        std::cout<<"PASS: "<<checks<<" initialization/solver scenarios; Point legacy regression; RNG, workers, observer and CV checks\n";
        return 0;
    } catch(const std::exception &e) {
        std::cerr<<"FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
