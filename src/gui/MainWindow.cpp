#include "gui/MainWindow.h"

#include <Qt>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

#include "gui/CompactDoubleSpinBox.h"
#include "gui/SimulationWorker.h"
#include "gui/TrajectoryPlotWidget.h"

namespace {

[[nodiscard]] CompactDoubleSpinBox* make_double_spin_box(
    double minimum,
    double maximum,
    double value,
    int decimals,
    double step
) {
    auto* spin_box = new CompactDoubleSpinBox;

    spin_box->setRange(minimum, maximum);
    spin_box->setValue(value);
    spin_box->setDecimals(decimals);
    spin_box->setSingleStep(step);
    spin_box->setKeyboardTracking(false);

    return spin_box;
}

[[nodiscard]] QSpinBox* make_spin_box(
    int minimum,
    int maximum,
    int value,
    int step = 1
) {
    auto* spin_box = new QSpinBox;

    spin_box->setRange(minimum, maximum);
    spin_box->setValue(value);
    spin_box->setSingleStep(step);
    spin_box->setKeyboardTracking(false);

    return spin_box;
}

[[nodiscard]] QLabel* make_result_label() {
    auto* label = new QLabel{"—"};

    label->setTextInteractionFlags(
        Qt::TextSelectableByMouse
    );

    return label;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow{parent}
{
    create_interface_();
    connect_controls_();

    trajectory_dialog_ = new QDialog{nullptr};

    trajectory_dialog_->setWindowFlags(
    Qt::Window |
    Qt::WindowTitleHint |
    Qt::WindowSystemMenuHint |
    Qt::WindowMinimizeButtonHint |
    Qt::WindowMaximizeButtonHint |
    Qt::WindowCloseButtonHint
);

    trajectory_dialog_->setWindowTitle(
        "Mean position trajectory"
    );

    trajectory_dialog_->setModal(false);

    trajectory_dialog_->resize(900, 540);
    trajectory_dialog_->installEventFilter(this);

    auto* trajectory_layout =
        new QVBoxLayout{trajectory_dialog_};

    trajectory_plot_ = new TrajectoryPlotWidget{
        trajectory_dialog_
    };

    trajectory_layout->addWidget(
        trajectory_plot_
    );

    update_show_graph_button_();

    elapsed_timer_.setInterval(250);

    connect(
        &elapsed_timer_,
        &QTimer::timeout,
        this,
        [this] {
            update_elapsed_time_();
        }
    );

    setWindowTitle("Brownian Motor");
    resize(980, 680);

    append_log_(
        "Ready. Fast mode is optimized for final calculations. "
        "Interactive mode provides real progress and live observables."
    );
}

MainWindow::~MainWindow() {
    if (cancellation_source_) {
        cancellation_source_->request_stop();
    }

    if (simulation_thread_) {
        simulation_thread_->quit();
        simulation_thread_->wait();
    }

    delete trajectory_dialog_;
    trajectory_dialog_ = nullptr;
}

void MainWindow::create_interface_() {
    auto* central_widget = new QWidget{this};
    setCentralWidget(central_widget);

    auto* root_layout = new QVBoxLayout{central_widget};

    auto* parameters_layout = new QHBoxLayout;
    root_layout->addLayout(parameters_layout);

    auto* potential_group = new QGroupBox{"Potential"};
    auto* potential_form = new QFormLayout{potential_group};

    v1_spin_ = make_double_spin_box(
        -100.0, 100.0, 0.2, 8, 0.01
    );

    v2_spin_ = make_double_spin_box(
        -100.0, 100.0, 0.1, 8, 0.01
    );

    potential_form->addRow("V1:", v1_spin_);
    potential_form->addRow("V2:", v2_spin_);

    parameters_layout->addWidget(potential_group);

    auto* modulation_group =
        new QGroupBox{"Dichotomic modulation"};

    auto* modulation_form = new QFormLayout{
        modulation_group
    };

    amplitude_spin_ = make_double_spin_box(
        1.0e-8, 1'000.0, 1.0, 8, 0.1
    );

    epsilon_spin_ = make_double_spin_box(
        1.0e-8, 1'000.0, 0.075, 8, 0.01
    );

    alpha_spin_ = make_double_spin_box(
        -1.0, 1.0, -1.0 / 3.0, 8, 0.01
    );

    modulation_form->addRow("Amplitude:", amplitude_spin_);
    modulation_form->addRow("epsilon:", epsilon_spin_);
    modulation_form->addRow("alpha:", alpha_spin_);

    parameters_layout->addWidget(modulation_group);

    auto* simulation_group = new QGroupBox{"Simulation"};
    auto* simulation_form = new QFormLayout{
        simulation_group
    };

    dt_spin_ = make_double_spin_box(
        1.0e-8, 1.0, 0.001, 10, 1.0e-4
    );

    total_time_spin_ = make_double_spin_box(
        1.0e-6, 1'000'000.0, 100.0, 6, 10.0
    );

    particles_spin_ = make_spin_box(
        1, 10'000'000, 2'000, 1'000
    );

    burn_in_spin_ = make_spin_box(
        0, 2'000'000'000, 10'000, 1'000
    );

    seed_spin_ = make_spin_box(
        0, 2'147'483'647, 42
    );

    const std::size_t hardware_threads =
        std::max(
            std::size_t{1},
            static_cast<std::size_t>(
                std::thread::hardware_concurrency()
            )
        );

    workers_spin_ = make_spin_box(
        0,
        static_cast<int>(hardware_threads),
        0
    );

    workers_spin_->setSpecialValueText(
        QString{"Auto (%1)"}
            .arg(
                static_cast<qulonglong>(
                    hardware_threads
                )
            )
    );

    mode_combo_ = new QComboBox;

    mode_combo_->addItem(
        "Fast: final result only",
        false
    );

    mode_combo_->addItem(
        "Interactive: progress and live data",
        true
    );

    mode_combo_->setToolTip(
        "Fast mode avoids intermediate ensemble reductions.\n"
        "Interactive mode calculates in batches and provides "
        "real progress updates."
    );

    simulation_form->addRow("Mode:", mode_combo_);
    simulation_form->addRow("dt:", dt_spin_);
    simulation_form->addRow("Total time:", total_time_spin_);
    simulation_form->addRow("Particles:", particles_spin_);
    simulation_form->addRow("Burn-in steps:", burn_in_spin_);
    simulation_form->addRow("Seed:", seed_spin_);
    simulation_form->addRow("Workers:", workers_spin_);

    parameters_layout->addWidget(simulation_group);

    auto* controls_layout = new QHBoxLayout;

    run_button_ = new QPushButton{"Run simulation"};
    cancel_button_ = new QPushButton{"Cancel"};

    show_graph_button_ = new QPushButton{"Show graph"};
    show_graph_button_->setEnabled(false);

    progress_bar_ = new QProgressBar;
    progress_bar_->setFixedWidth(210);
    progress_bar_->setVisible(false);

    elapsed_live_label_ = new QLabel{"Elapsed: —"};
    elapsed_live_label_->setMinimumWidth(140);

    cancel_button_->setEnabled(false);

    status_label_ = new QLabel{"Ready"};
    status_label_->setFrameStyle(
        QFrame::Panel | QFrame::Sunken
    );
    status_label_->setMinimumWidth(330);

    controls_layout->addWidget(run_button_);
    controls_layout->addWidget(cancel_button_);
    controls_layout->addWidget(show_graph_button_);
    controls_layout->addWidget(progress_bar_);
    controls_layout->addWidget(elapsed_live_label_);
    controls_layout->addSpacing(12);
    controls_layout->addWidget(status_label_);
    controls_layout->addStretch();

    root_layout->addLayout(controls_layout);

    auto* results_group = new QGroupBox{"Result"};
    auto* results_layout = new QGridLayout{results_group};

    velocity_value_label_ = make_result_label();
    final_x_value_label_ = make_result_label();
    elapsed_value_label_ = make_result_label();
    throughput_value_label_ = make_result_label();
    workers_value_label_ = make_result_label();

    results_layout->addWidget(
        new QLabel{"Mean velocity:"}, 0, 0
    );
    results_layout->addWidget(
        velocity_value_label_, 0, 1
    );

    results_layout->addWidget(
        new QLabel{"<x_final>:"}, 1, 0
    );
    results_layout->addWidget(
        final_x_value_label_, 1, 1
    );

    results_layout->addWidget(
        new QLabel{"Elapsed time:"}, 0, 2
    );
    results_layout->addWidget(
        elapsed_value_label_, 0, 3
    );

    results_layout->addWidget(
        new QLabel{"Throughput:"}, 1, 2
    );
    results_layout->addWidget(
        throughput_value_label_, 1, 3
    );

    results_layout->addWidget(
        new QLabel{"Workers used:"}, 2, 0
    );
    results_layout->addWidget(
        workers_value_label_, 2, 1
    );

    root_layout->addWidget(results_group);

    auto* log_group = new QGroupBox{"Log"};
    auto* log_layout = new QVBoxLayout{log_group};

    log_output_ = new QPlainTextEdit;
    log_output_->setReadOnly(true);
    log_output_->setMaximumBlockCount(1'000);

    log_layout->addWidget(log_output_);
    root_layout->addWidget(log_group, 1);
}

void MainWindow::connect_controls_() {
    connect(
        run_button_,
        &QPushButton::clicked,
        this,
        [this] {
            start_simulation_();
        }
    );

    connect(
        cancel_button_,
        &QPushButton::clicked,
        this,
        [this] {
            cancel_simulation_();
        }
    );

    connect(
    show_graph_button_,
    &QPushButton::clicked,
    this,
    [this] {
        show_trajectory_window_();
    }
    );

    connect(
        mode_combo_,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this](int) {
            update_show_graph_button_();
        }
    );
}

SimulationRequest MainWindow::request_from_controls_() const {
    const bool interactive_mode =
        mode_combo_->currentData().toBool();

    return SimulationRequest{
        .v1 = v1_spin_->value(),
        .v2 = v2_spin_->value(),

        .modulation_amplitude =
            amplitude_spin_->value(),

        .epsilon = epsilon_spin_->value(),
        .alpha = alpha_spin_->value(),

        .dt = dt_spin_->value(),
        .total_time = total_time_spin_->value(),

        .n_particles = static_cast<std::size_t>(
            particles_spin_->value()
        ),

        .burn_in_steps = static_cast<std::size_t>(
            burn_in_spin_->value()
        ),

        .x0 = 0.0,

        .seed = static_cast<std::uint32_t>(
            seed_spin_->value()
        ),

        .requested_workers = static_cast<std::size_t>(
            workers_spin_->value()
        ),

        .batch_steps = 3'500,
        .cancellation_check_steps = 4'096,
        .interactive_mode = interactive_mode
    };
}

bool MainWindow::validate_request_(
    const SimulationRequest& request,
    QString& error_message
) const {
    if (request.dt <= 0.0) {
        error_message = "dt must be greater than zero.";
        return false;
    }

    if (request.total_time <= 0.0) {
        error_message = "Total time must be greater than zero.";
        return false;
    }

    const std::size_t total_steps =
        static_cast<std::size_t>(
            request.total_time / request.dt
        );

    if (total_steps == 0) {
        error_message =
            "Total time / dt must produce at least one physical step.";
        return false;
    }

    if (request.burn_in_steps >= total_steps) {
        error_message = QString{
            "Burn-in steps (%1) must be smaller than the "
            "total number of physical steps (%2)."
        }
            .arg(
                static_cast<qulonglong>(
                    request.burn_in_steps
                )
            )
            .arg(
                static_cast<qulonglong>(
                    total_steps
                )
            );

        return false;
    }

    return true;
}

void MainWindow::update_elapsed_time_() {
    if (!elapsed_clock_.isValid()) {
        elapsed_live_label_->setText("Elapsed: —");
        return;
    }

    const double elapsed_seconds =
        static_cast<double>(
            elapsed_clock_.elapsed()
        ) / 1000.0;

    elapsed_live_label_->setText(
        QString{"Elapsed: %1 s"}
            .arg(elapsed_seconds, 0, 'f', 1)
    );
}

void MainWindow::start_simulation_() {
    if (simulation_thread_) {
        return;
    }

    const SimulationRequest request =
        request_from_controls_();

    QString validation_error;

    if (!validate_request_(request, validation_error)) {
        QMessageBox::warning(
            this,
            "Invalid simulation parameters",
            validation_error
        );

        append_log_(
            QString{"Start rejected: %1"}
                .arg(validation_error)
        );

        return;
    }

    const std::size_t total_steps =
        static_cast<std::size_t>(
            request.total_time / request.dt
        );

    const std::size_t total_updates =
        request.n_particles * total_steps;

    cancellation_source_ =
        std::make_shared<std::stop_source>();

    trajectory_plot_->clear_points();

    update_show_graph_button_();

    if (request.interactive_mode) {
        show_trajectory_window_();
    } else {
        trajectory_dialog_->hide();
    }

    simulation_thread_ = new QThread{this};

    simulation_worker_ = new SimulationWorker{
        request,
        cancellation_source_
    };

    simulation_worker_->moveToThread(
        simulation_thread_
    );

    connect(
        simulation_thread_,
        &QThread::started,
        simulation_worker_,
        &SimulationWorker::run
    );

    connect(
        simulation_worker_,
        &SimulationWorker::progress_updated,
        this,
        [this](
            qulonglong completed_steps,
            qulonglong total_steps_value,
            double time,
            double mean_x,
            double mean_velocity
        ) {
            if (total_steps_value == 0) {
                return;
            }

            const double fraction =
                static_cast<double>(completed_steps) /
                static_cast<double>(total_steps_value);

            const int progress_percent =
                static_cast<int>(
                    std::lround(100.0 * fraction)
                );

            progress_bar_->setRange(0, 100);
            progress_bar_->setValue(
                std::clamp(progress_percent, 0, 100)
            );

            status_label_->setText(
                QString{
                    "Running: %1% | t = %2 | <x> = %3 | v = %4"
                }
                    .arg(progress_percent)
                    .arg(time, 0, 'g', 8)
                    .arg(mean_x, 0, 'e', 4)
                    .arg(mean_velocity, 0, 'e', 4)
            );

            trajectory_plot_->append_point(
                time,
                mean_x
            );
        },
        Qt::QueuedConnection
    );

    connect(
        simulation_worker_,
        &SimulationWorker::completed,
        this,
        [this](
            double mean_velocity,
            double mean_x_final,
            double elapsed_seconds,
            double throughput,
            std::size_t workers
        ) {
            elapsed_timer_.stop();

            elapsed_live_label_->setText(
                QString{"Elapsed: %1 s"}
                    .arg(elapsed_seconds, 0, 'f', 3)
            );

            progress_bar_->setRange(0, 100);
            progress_bar_->setValue(100);
            progress_bar_->setVisible(false);

            velocity_value_label_->setText(
                QString::number(
                    mean_velocity,
                    'e',
                    12
                )
            );

            final_x_value_label_->setText(
                QString::number(
                    mean_x_final,
                    'e',
                    12
                )
            );

            elapsed_value_label_->setText(
                QString{"%1 s"}
                    .arg(elapsed_seconds, 0, 'f', 3)
            );

            throughput_value_label_->setText(
                QString{"%1 M updates/s"}
                    .arg(throughput, 0, 'f', 3)
            );

            workers_value_label_->setText(
                QString::number(
                    static_cast<qulonglong>(workers)
                )
            );

            const bool was_cancelled =
                cancellation_source_ &&
                cancellation_source_->stop_requested();

            status_label_->setText(
                was_cancelled
                    ? "Cancelled: partial result returned"
                    : "Completed"
            );

            append_log_(
                QString{
                    "Completed: velocity = %1, "
                    "<x_final> = %2, time = %3 s, "
                    "throughput = %4 M updates/s"
                }
                    .arg(mean_velocity, 0, 'e', 12)
                    .arg(mean_x_final, 0, 'e', 12)
                    .arg(elapsed_seconds, 0, 'f', 3)
                    .arg(throughput, 0, 'f', 3)
            );

            update_show_graph_button_();
        }
    );

    connect(
        simulation_worker_,
        &SimulationWorker::failed,
        this,
        [this](const QString& message) {
            elapsed_timer_.stop();

            progress_bar_->setRange(0, 100);
            progress_bar_->setValue(0);
            progress_bar_->setVisible(false);

            elapsed_live_label_->setText("Elapsed: —");
            status_label_->setText("Failed");

            append_log_(
                QString{"Simulation failed: %1"}
                    .arg(message)
            );

            update_show_graph_button_();
        }
    );

    connect(
        simulation_worker_,
        &SimulationWorker::finished,
        simulation_thread_,
        &QThread::quit
    );

    connect(
        simulation_worker_,
        &SimulationWorker::finished,
        simulation_worker_,
        &QObject::deleteLater
    );

    connect(
        simulation_thread_,
        &QThread::finished,
        this,
        [this] {
            elapsed_timer_.stop();

            progress_bar_->setRange(0, 100);
            progress_bar_->setValue(0);
            progress_bar_->setVisible(false);

            if (elapsed_clock_.isValid()) {
                elapsed_clock_.invalidate();
            }

            simulation_thread_->deleteLater();

            simulation_thread_ = nullptr;
            simulation_worker_ = nullptr;
            cancellation_source_.reset();

            set_running_state_(false);
        }
    );

    velocity_value_label_->setText("Running...");
    final_x_value_label_->setText("Running...");
    elapsed_value_label_->setText("Running...");
    throughput_value_label_->setText("Running...");
    workers_value_label_->setText("Running...");

    status_label_->setText(
        request.interactive_mode
            ? "Starting interactive solver..."
            : "Starting optimized fast solver..."
    );

    append_log_(
        QString{
            "Run started: mode = %1, particles = %2, "
            "T = %3, dt = %4, seed = %5, workers = %6"
        }
            .arg(
                request.interactive_mode
                    ? "interactive"
                    : "fast"
            )
            .arg(
                static_cast<qulonglong>(
                    request.n_particles
                )
            )
            .arg(request.total_time, 0, 'g', 12)
            .arg(request.dt, 0, 'g', 12)
            .arg(request.seed)
            .arg(
                request.requested_workers == 0
                    ? QString{"auto"}
                    : QString::number(
                        static_cast<qulonglong>(
                            request.requested_workers
                        )
                    )
            )
    );

    append_log_(
        QString{
            "Work estimate: %1 physical steps, "
            "%2 particle updates."
        }
            .arg(
                static_cast<qulonglong>(
                    total_steps
                )
            )
            .arg(
                static_cast<qulonglong>(
                    total_updates
                )
            )
    );

    set_running_state_(true);

    if (request.interactive_mode) {
        progress_bar_->setRange(0, 100);
        progress_bar_->setValue(0);
        progress_bar_->setTextVisible(true);
        progress_bar_->setFormat("%p %");
    } else {
        progress_bar_->setRange(0, 0);
        progress_bar_->setTextVisible(false);
    }

    progress_bar_->setVisible(true);

    elapsed_live_label_->setText("Elapsed: 0.0 s");
    elapsed_clock_.start();
    elapsed_timer_.start();

    simulation_thread_->start();
}

void MainWindow::cancel_simulation_() {
    if (!cancellation_source_) {
        return;
    }

    cancellation_source_->request_stop();

    cancel_button_->setEnabled(false);

    status_label_->setText(
        "Cancellation requested..."
    );

    append_log_(
        "Cancellation requested. "
        "The solver stops at its next cancellation check."
    );
}

void MainWindow::show_trajectory_window_() {
    if (!trajectory_dialog_) {
        return;
    }

    trajectory_dialog_->show();
    trajectory_dialog_->raise();
    trajectory_dialog_->activateWindow();

    update_show_graph_button_();
}

void MainWindow::update_show_graph_button_() {
    const bool interactive_mode =
        mode_combo_->currentData().toBool();

    const bool graph_is_hidden =
        trajectory_dialog_ &&
        !trajectory_dialog_->isVisible();

    show_graph_button_->setEnabled(
        interactive_mode &&
        graph_is_hidden
    );
}

bool MainWindow::eventFilter(
    QObject* watched,
    QEvent* event
) {
    if (
        watched == trajectory_dialog_ &&
        (
            event->type() == QEvent::Show ||
            event->type() == QEvent::Hide
        )
    ) {
        QTimer::singleShot(
            0,
            this,
            [this] {
                update_show_graph_button_();
            }
        );
    }

    return QMainWindow::eventFilter(
        watched,
        event
    );
}

void MainWindow::set_running_state_(bool is_running) {
    run_button_->setEnabled(!is_running);
    cancel_button_->setEnabled(is_running);
    update_show_graph_button_();

    mode_combo_->setEnabled(!is_running);

    v1_spin_->setEnabled(!is_running);
    v2_spin_->setEnabled(!is_running);

    amplitude_spin_->setEnabled(!is_running);
    epsilon_spin_->setEnabled(!is_running);
    alpha_spin_->setEnabled(!is_running);

    dt_spin_->setEnabled(!is_running);
    total_time_spin_->setEnabled(!is_running);

    particles_spin_->setEnabled(!is_running);
    burn_in_spin_->setEnabled(!is_running);
    seed_spin_->setEnabled(!is_running);
    workers_spin_->setEnabled(!is_running);
}

void MainWindow::append_log_(
    const QString& message
) {
    log_output_->appendPlainText(message);
}