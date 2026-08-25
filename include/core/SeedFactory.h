#ifndef BROWNIAN_MOTOR_SEEDFACTORY_H
#define BROWNIAN_MOTOR_SEEDFACTORY_H
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

class SeedFactory final {
public:
    enum class Stream : std::uint32_t {
        GaussianThermal = 0xA341316Cu,
        Dichotomic      = 0xA5A5A5A5u
    };

    explicit SeedFactory(std::uint32_t run_seed)
        : run_seed_{run_seed}
    {}

    [[nodiscard]] std::uint32_t run_seed() const noexcept {
        return run_seed_;
    }

    [[nodiscard]] std::uint32_t particle_seed(
        std::size_t particle_index,
        Stream stream
    ) const
    {
        const auto index = static_cast<std::uint64_t>(particle_index);

        std::seed_seq sequence{
            run_seed_,
            static_cast<std::uint32_t>(stream),
            static_cast<std::uint32_t>(index & 0xFFFFFFFFull),
            static_cast<std::uint32_t>((index >> 32) & 0xFFFFFFFFull),
            0x9E3779B9u,
            0x85EBCA6Bu,
            0xC2B2AE35u
        };

        std::array<std::uint32_t, 1> output{};
        sequence.generate(output.begin(), output.end());
        return output.front();
    }

    [[nodiscard]] std::mt19937 make_mt19937(
        std::size_t particle_index,
        Stream stream
    ) const
    {
        const auto index = static_cast<std::uint64_t>(particle_index);

        std::seed_seq sequence{
            run_seed_,
            static_cast<std::uint32_t>(stream),
            static_cast<std::uint32_t>(index & 0xFFFFFFFFull),
            static_cast<std::uint32_t>((index >> 32) & 0xFFFFFFFFull),
            0x9E3779B9u,
            0x85EBCA6Bu,
            0xC2B2AE35u
        };

        return std::mt19937{sequence};
    }

private:
    std::uint32_t run_seed_{};
};

#endif // BROWNIAN_MOTOR_SEEDFACTORY_H