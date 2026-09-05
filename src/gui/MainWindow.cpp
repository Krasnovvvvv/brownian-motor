#include "gui/MainWindow.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

#include "gui/SimulationWorker.h"
#include "gui/CompactDoubleSpinBox.h"

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

    setWindowTitle("Brownian Motor");
    resize(900, 680);

    append_log_(
        "Ready. The current mode runs the optimized "
        "headless fast solver in a background thread."
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
        -100.0,
        100.0,
        0.2,
        8,
        0.01
    );

    v2_spin_ = make_double_spin_box(
        -100.0,
        100.0,
        0.1,
        8,
        0.01
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
        1.0e-8,
        1'000.0,
        1.0,
        8,
        0.1
    );

    epsilon_spin_ = make_double_spin_box(
        1.0e-8,
        1'000.0,
        0.075,
        8,
        0.01
    );

    alpha_spin_ = make_double_spin_box(
        -1.0,
        1.0,
        -1.0 / 3.0,
        8,
        0.01
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
        1.0e-8,
        1.0,
        0.001,
        10,
        1.0e-4
    );

    total_time_spin_ = make_double_spin_box(
        1.0e-6,
        1'000'000.0,
        100.0,
        6,
        10.0
    );

    particles_spin_ = make_spin_box(
        1,
        10'000'000,
        2'000,
        1'000
    );

    burn_in_spin_ = make_spin_box(
        0,
        2'000'000'000,
        10'000,
        1'000
    );

    seed_spin_ = make_spin_box(
        0,
        2'147'483'647,
        42
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

    cancel_button_->setEnabled(false);

    status_label_ = new QLabel{"Ready"};
    status_label_->setFrameStyle(
        QFrame::Panel | QFrame::Sunken
    );
    status_label_->setMinimumWidth(300);

    controls_layout->addWidget(run_button_);
    controls_layout->addWidget(cancel_button_);
    controls_layout->addSpacing(16);
    controls_layout->addWidget(status_label_);
    controls_layout->addStretch();

    root_layout->addLayout(controls_layout);

    auto* results_group = new QGroupBox{"Fast-solver result"};
    auto* results_layout = new QGridLayout{results_group};

    velocity_value_label_ = make_result_label();
    final_x_value_label_ = make_result_label();
    elapsed_value_label_ = make_result_label();
    throughput_value_label_ = make_result_label();
    workers_value_label_ = make_result_label();

    results_layout->addWidget(
        new QLabel{"Mean velocity:"},
        0,
        0
    );
    results_layout->addWidget(
        velocity_value_label_,
        0,
        1
    );

    results_layout->addWidget(
        new QLabel{"<x_final>:"},
        1,
        0
    );
    results_layout->addWidget(
        final_x_value_label_,
        1,
        1
    );

    results_layout->addWidget(
        new QLabel{"Elapsed time:"},
        0,
        2
    );
    results_layout->addWidget(
        elapsed_value_label_,
        0,
        3
    );

    results_layout->addWidget(
        new QLabel{"Throughput:"},
        1,
        2
    );
    results_layout->addWidget(
        throughput_value_label_,
        1,
        3
    );

    results_layout->addWidget(
        new QLabel{"Workers used:"},
        2,
        0
    );
    results_layout->addWidget(
        workers_value_label_,
        2,
        1
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
}

SimulationRequest MainWindow::request_from_controls_() const {
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

        .cancellation_check_steps = 4'096
    };
}

void MainWindow::start_simulation_() {
    if (simulation_thread_) {
        return;
    }

    const SimulationRequest request =
        request_from_controls_();

    cancellation_source_ =
        std::make_shared<std::stop_source>();

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
        &SimulationWorker::completed,
        this,
        [this](
            double mean_velocity,
            double mean_x_final,
            double elapsed_seconds,
            double throughput,
            std::size_t workers
        ) {
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
                    .arg(
                        elapsed_seconds,
                        0,
                        'f',
                        3
                    )
            );

            throughput_value_label_->setText(
                QString{"%1 M updates/s"}
                    .arg(
                        throughput,
                        0,
                        'f',
                        3
                    )
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
        }
    );

    connect(
        simulation_worker_,
        &SimulationWorker::failed,
        this,
        [this](const QString& message) {
            status_label_->setText("Failed");

            append_log_(
                QString{"Simulation failed: %1"}
                    .arg(message)
            );
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
        "Running optimized fast solver..."
    );

    append_log_(
        QString{
            "Run started: particles = %1, T = %2, dt = %3, "
            "seed = %4, workers = %5"
        }
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

    set_running_state_(true);
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

void MainWindow::set_running_state_(bool is_running) {
    run_button_->setEnabled(!is_running);
    cancel_button_->setEnabled(is_running);

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