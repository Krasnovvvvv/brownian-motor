#ifndef BROWNIAN_MOTOR_MAINWINDOW_H
#define BROWNIAN_MOTOR_MAINWINDOW_H
#pragma once

#include <QMainWindow>

#include <memory>
#include <stop_token>

class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;

class SimulationWorker;
struct SimulationRequest;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void create_interface_();
    void connect_controls_();

    [[nodiscard]] SimulationRequest request_from_controls_() const;

    void start_simulation_();
    void cancel_simulation_();

    void set_running_state_(bool is_running);
    void append_log_(const QString& message);

    QDoubleSpinBox* v1_spin_{nullptr};
    QDoubleSpinBox* v2_spin_{nullptr};

    QDoubleSpinBox* amplitude_spin_{nullptr};
    QDoubleSpinBox* epsilon_spin_{nullptr};
    QDoubleSpinBox* alpha_spin_{nullptr};

    QDoubleSpinBox* dt_spin_{nullptr};
    QDoubleSpinBox* total_time_spin_{nullptr};

    QSpinBox* particles_spin_{nullptr};
    QSpinBox* burn_in_spin_{nullptr};
    QSpinBox* seed_spin_{nullptr};
    QSpinBox* workers_spin_{nullptr};

    QPushButton* run_button_{nullptr};
    QPushButton* cancel_button_{nullptr};

    QLabel* status_label_{nullptr};

    QLabel* velocity_value_label_{nullptr};
    QLabel* final_x_value_label_{nullptr};
    QLabel* elapsed_value_label_{nullptr};
    QLabel* throughput_value_label_{nullptr};
    QLabel* workers_value_label_{nullptr};

    QPlainTextEdit* log_output_{nullptr};

    QThread* simulation_thread_{nullptr};
    SimulationWorker* simulation_worker_{nullptr};

    std::shared_ptr<std::stop_source> cancellation_source_;
};

#endif // BROWNIAN_MOTOR_MAINWINDOW_H