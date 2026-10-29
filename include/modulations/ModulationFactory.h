#ifndef BROWNIAN_MOTOR_MODULATIONFACTORY_H
#define BROWNIAN_MOTOR_MODULATIONFACTORY_H
#pragma once

#include <cstddef>
#include <vector>

#include "DichotomicModulation.h"
#include "DichotomicParams.h"
#include "core/SeedFactory.h"
#include "TwoStateFactorModulation.h"

class ModulationFactory final {
public:
    [[nodiscard]] static std::vector<DichotomicModulation>
    make_dichotomic_ensemble(
        const DichotomicParams& params,
        std::size_t n_particles,
        const SeedFactory& seeds
    ) {
        params.validate();

        std::vector<DichotomicModulation> modulations;
        modulations.reserve(n_particles);

        for (std::size_t particle = 0; particle < n_particles; ++particle) {
            modulations.emplace_back(
                params,
                seeds.particle_seed(
                    particle,
                    SeedFactory::Stream::Dichotomic
                )
            );
        }

        return modulations;
    }

    [[nodiscard]]
    static std::vector<TwoStateFactorModulation>
    make_two_state_factor_ensemble(
        const ModulationSettings& settings,
        double dt,
        std::size_t n_particles,
        const SeedFactory& seeds
    ) {
        settings.validate();

        std::vector<TwoStateFactorModulation> result;
        result.reserve(n_particles);

        for (
            std::size_t particle = 0;
            particle < n_particles;
            ++particle
        ) {
            result.emplace_back(
                settings,
                dt,
                seeds.particle_seed(
                    particle,
                    SeedFactory::Stream::Dichotomic
                )
            );
        }

        return result;
    }
};

#endif // BROWNIAN_MOTOR_MODULATIONFACTORY_H