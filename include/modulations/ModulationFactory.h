#ifndef BROWNIAN_MOTOR_MODULATIONFACTORY_H
#define BROWNIAN_MOTOR_MODULATIONFACTORY_H
#pragma once

#include <cstddef>
#include <vector>

#include "DichotomicModulation.h"
#include "DichotomicParams.h"
#include "core/SeedFactory.h"

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
};

#endif // BROWNIAN_MOTOR_MODULATIONFACTORY_H