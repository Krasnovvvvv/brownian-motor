#ifndef BROWNIAN_MOTOR_SIMULATIONWORKER_H
#define BROWNIAN_MOTOR_SIMULATIONWORKER_H
#pragma once

#include <QObject>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>

struct SimulationRequest final {
    double v1{0.10};
    double v2{0.025};

    double modulation_amplitude{1.0};
    double epsilon{0.075};
    double alpha{-1.0 / 3.0};

    double dt{1.0e-3};
    double total_time{100.0};

    std::size_t n_particles{2'000};
    std::size_t burn_in_steps{10'000};

    double x0{0.0};

    std::uint32_t seed{42u};
    std::size_t requested_workers{0};

    std::size_t cancellation_check_steps{4'096};
};

class SimulationWorker final : public QObject {
    Q_OBJECT

public:
    explicit SimulationWorker(
        SimulationRequest request,
        std::shared_ptr<std::stop_source> cancellation_source,
        QObject* parent = nullptr
    );

public slots:
    void run();

    signals:
        void completed(
            double mean_velocity,
            double mean_x_final,
            double elapsed_seconds,
            double throughput,
            std::size_t workers
        );

    void failed(const QString& message);

    void finished();

private:
    SimulationRequest request_;
    std::shared_ptr<std::stop_source> cancellation_source_;
};

#endif // BROWNIAN_MOTOR_SIMULATIONWORKER_H