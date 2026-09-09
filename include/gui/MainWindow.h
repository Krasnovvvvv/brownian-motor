#ifndef BROWNIAN_MOTOR_MAINWINDOW_H
#define BROWNIAN_MOTOR_MAINWINDOW_H
#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>

#include <memory>
#include <stop_token>

#include "core/ExperimentLogger.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QDialog;
class QEvent;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QString;
class QThread;

class SimulationWorker;
struct SimulationRequest;
class TrajectoryPlotWidget;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
    ) override;

private:
    void create_interface_();
    void connect_controls_();

    void log_experiment_event_(
    const QString& event_name,
    const QJsonObject& data = {}
    );

    [[nodiscard]] SimulationRequest request_from_controls_() const;

    [[nodiscard]] bool validate_request_(
        const SimulationRequest& request,
        QString& error_message
    ) const;

    void start_simulation_();
    void cancel_simulation_();

    void show_trajectory_window_();
    void update_show_graph_button_();

    void begin_trend_selection_();
    void clear_trend_();
    void validate_burn_in_();
    void clear_burn_in_validation_();
    void update_plot_tools_();

    void export_trajectory_();
    void export_trajectory_csv_(const QString& file_name);

    void export_plot_image_(
        const QString& file_name,
        const QString& format
    );

    void update_elapsed_time_();
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

    QComboBox* mode_combo_{nullptr};

    QPushButton* run_button_{nullptr};
    QPushButton* cancel_button_{nullptr};
    QPushButton* show_graph_button_{nullptr};

    QPushButton* select_trend_button_{nullptr};
    QPushButton* clear_trend_button_{nullptr};

    QCheckBox* highlight_trend_range_check_{nullptr};

    QPushButton* export_button_{nullptr};
    QPushButton* validate_burn_in_button_{nullptr};
    QPushButton* clear_burn_in_button_{nullptr};

    QProgressBar* progress_bar_{nullptr};

    QLabel* status_label_{nullptr};
    QLabel* elapsed_live_label_{nullptr};

    QLabel* velocity_value_label_{nullptr};
    QLabel* final_x_value_label_{nullptr};
    QLabel* elapsed_value_label_{nullptr};
    QLabel* throughput_value_label_{nullptr};
    QLabel* workers_value_label_{nullptr};

    QLabel* trend_info_label_{nullptr};
    QLabel* burn_in_info_label_{nullptr};
    QSpinBox* burn_in_tolerance_spin_{nullptr};

    QPlainTextEdit* log_output_{nullptr};

    QDialog* trajectory_dialog_{nullptr};
    TrajectoryPlotWidget* trajectory_plot_{nullptr};

    QThread* simulation_thread_{nullptr};
    SimulationWorker* simulation_worker_{nullptr};

    std::shared_ptr<std::stop_source> cancellation_source_;

    bool simulation_completed_{false};
    double trajectory_dt_{0.0};

    QTimer elapsed_timer_;
    QElapsedTimer elapsed_clock_;

    ExperimentLogger experiment_logger_;
};

#endif // BROWNIAN_MOTOR_MAINWINDOW_H