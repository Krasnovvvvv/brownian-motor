#include "gui/MainWindow.h"

#include <Qt>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
#include <QScrollArea>
#include <QSpinBox>
#include <QSizePolicy>
#include <QSaveFile>
#include <QThread>
#include <QTimer>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <exception>

#include "gui/CompactDoubleSpinBox.h"
#include "gui/SimulationWorker.h"
#include "gui/TrajectoryPlotWidget.h"

#include "core/PotentialConfigParser.h"
#include "gui/PotentialDropArea.h"

#include "core/ExpressionEvaluator.h"
#include "core/PotentialDerivativeCheck.h"

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

    void configure_group_title(
    QGroupBox* group
) {
    group->setAlignment(
        Qt::AlignHCenter
    );

    group->setStyleSheet(
        "QGroupBox {"
        "    font-size: 11pt;"
        "    font-weight: bold;"
        "}"
    );
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow{parent}
{
    setMinimumSize(1100, 600);

    create_interface_();
    connect_controls_();
    use_builtin_potential_();

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
        new QHBoxLayout{trajectory_dialog_};

    trajectory_layout->setContentsMargins(
        10,
        10,
        10,
        10
    );

    trajectory_layout->setSpacing(10);

    trajectory_plot_ = new TrajectoryPlotWidget{
        trajectory_dialog_
    };

auto* analysis_panel = new QWidget{
    trajectory_dialog_
};

analysis_panel->setFixedWidth(250);

auto* analysis_layout = new QVBoxLayout{
    analysis_panel
};

analysis_layout->setContentsMargins(
    0,
    0,
    0,
    0
);

analysis_layout->setSpacing(10);

auto* trend_group = new QGroupBox{
    "Trend analysis",
    analysis_panel
};

trend_group->setAlignment(
    Qt::AlignHCenter
);

auto* trend_layout = new QVBoxLayout{
    trend_group
};

trend_layout->setContentsMargins(
    10,
    12,
    10,
    10
);

trend_layout->setSpacing(8);

select_trend_button_ = new QPushButton{
    "Select trend start",
    trend_group
};

select_trend_button_->setToolTip(
    "Choose the first point used for the linear "
    "regression after the simulation has completed."
);

clear_trend_button_ = new QPushButton{
    "Clear trend",
    trend_group
};

    highlight_trend_range_check_ = new QCheckBox{
        "Highlight trend range",
        trend_group
    };

    highlight_trend_range_check_->setChecked(true);

    highlight_trend_range_check_->setToolTip(
        "Show a subtle yellow background over the "
        "trajectory range used for the linear trend."
    );

clear_trend_button_->setToolTip(
    "Remove the selected trend or cancel "
    "point selection."
);

trend_info_label_ = new QLabel{
    "Trend: available after simulation completes",
    trend_group
};

trend_info_label_->setWordWrap(true);

trend_info_label_->setMinimumHeight(84);

trend_info_label_->setAlignment(
    Qt::AlignLeft | Qt::AlignTop
);

trend_layout->addWidget(
    select_trend_button_
);

trend_layout->addWidget(
    clear_trend_button_
);

    trend_layout->addWidget(
    highlight_trend_range_check_
);

trend_layout->addWidget(
    trend_info_label_
);

auto* burn_in_group = new QGroupBox{
    "Burn-in validation",
    analysis_panel
};

burn_in_group->setAlignment(
    Qt::AlignHCenter
);

auto* burn_in_layout = new QVBoxLayout{
    burn_in_group
};

burn_in_layout->setContentsMargins(
    10,
    12,
    10,
    10
);

burn_in_layout->setSpacing(8);

burn_in_tolerance_spin_ = make_spin_box(
    5,
    50,
    25,
    5
);

burn_in_tolerance_spin_->setSuffix("%");

burn_in_tolerance_spin_->setToolTip(
    "Maximum allowed relative difference "
    "between early and late tail velocities."
);

auto* tolerance_form = new QFormLayout;

tolerance_form->setContentsMargins(
    0,
    0,
    0,
    0
);

tolerance_form->addRow(
    "Velocity tolerance:",
    burn_in_tolerance_spin_
);

validate_burn_in_button_ = new QPushButton{
    "Validate burn-in",
    burn_in_group
};

validate_burn_in_button_->setToolTip(
    "Find the earliest trajectory segment "
    "whose early and late velocities agree "
    "within the selected tolerance."
);

clear_burn_in_button_ = new QPushButton{
    "Clear validation",
    burn_in_group
};

clear_burn_in_button_->setToolTip(
    "Remove the burn-in recommendation "
    "and graph marker."
);

burn_in_info_label_ = new QLabel{
    "Burn-in: not validated",
    burn_in_group
};

burn_in_info_label_->setWordWrap(true);

burn_in_info_label_->setMinimumHeight(152);

burn_in_info_label_->setAlignment(
    Qt::AlignLeft | Qt::AlignTop
);

burn_in_layout->addLayout(
    tolerance_form
);

burn_in_layout->addWidget(
    validate_burn_in_button_
);

burn_in_layout->addWidget(
    clear_burn_in_button_
);

burn_in_layout->addWidget(
    burn_in_info_label_
);

export_button_ = new QPushButton{
    "Export...",
    analysis_panel
};

export_button_->setToolTip(
    "Export trajectory data as CSV or save "
    "the graph as PNG or JPG."
);

analysis_layout->addWidget(
    trend_group
);

analysis_layout->addWidget(
    burn_in_group
);

analysis_layout->addWidget(
    export_button_
);

analysis_layout->addStretch(1);

    trajectory_layout->addWidget(
        trajectory_plot_,
        1
    );

    trajectory_layout->addWidget(
        analysis_panel
    );

    connect(
        select_trend_button_,
        &QPushButton::clicked,
        this,
        [this] {
            begin_trend_selection_();
        }
    );

    connect(
        clear_trend_button_,
        &QPushButton::clicked,
        this,
        [this] {
            clear_trend_();
        }
    );

    connect(
    highlight_trend_range_check_,
    &QCheckBox::toggled,
    this,
    [this](bool checked) {
        if (!trajectory_plot_) {
            return;
        }

        trajectory_plot_->set_trend_range_highlight_visible(
            checked
        );
    }
);

    connect(
        validate_burn_in_button_,
        &QPushButton::clicked,
        this,
        [this] {
            validate_burn_in_();
        }
    );

    connect(
        clear_burn_in_button_,
        &QPushButton::clicked,
        this,
        [this] {
            clear_burn_in_validation_();
        }
    );

    connect(
        export_button_,
        &QPushButton::clicked,
        this,
        [this] {
            export_trajectory_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::point_count_changed,
        this,
        [this](std::size_t) {
            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::trend_selection_requested,
        this,
        [this] {
            select_trend_button_->setText(
                "Click a point on graph..."
            );

            trend_info_label_->setText(
                "Trend: click the first point "
                "of the regression range"
            );

            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::trend_changed,
        this,
        [this](
            double start_time,
            double intercept,
            double slope,
            double r_squared
        ) {
            //Q_UNUSED(intercept);

            select_trend_button_->setText(
                "Select trend start"
            );

            trend_info_label_->setText(
                QString{
                    "Trend fitted from:\n"
                    "t₀ = %1\n"
                    "v = %2\n"
                    "R² = %3"
                }
                    .arg(start_time, 0, 'g', 6)
                    .arg(slope, 0, 'e', 4)
                    .arg(r_squared, 0, 'f', 4)
            );

            log_experiment_event_(
    "trend_fitted",
    QJsonObject{
        {
            "start_time",
            start_time
        },
        {
            "intercept",
            intercept
        },
        {
            "slope",
            slope
        },
        {
            "r_squared",
            r_squared
        },
        {
            "trajectory_points",
            static_cast<qint64>(
                trajectory_plot_->point_count()
            )
        }
    }
);
            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::trend_cleared,
        this,
        [this] {
            select_trend_button_->setText(
                "Select trend start"
            );

            if (simulation_completed_) {
                trend_info_label_->setText(
                    "Trend: not calculated"
                );
            } else {
                trend_info_label_->setText(
                    "Trend: available after "
                    "simulation completes"
                );
            }

            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::trend_selection_failed,
        this,
        [this](const QString& message) {
            select_trend_button_->setText(
                "Select trend start"
            );

            QMessageBox::information(
                trajectory_dialog_,
                "Trend selection",
                message
            );

            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::burn_in_validation_changed,
        this,
        [this](
            bool stable,
            std::size_t recommended_steps,
            double start_time,
            double early_velocity,
            double late_velocity,
            double tail_velocity,
            double relative_difference,
            double tail_r_squared,
            double tolerance
        ) {

            log_experiment_event_(
    stable
        ? "burn_in_validated"
        : "burn_in_inconclusive",
    QJsonObject{
        {
            "stable",
            stable
        },
        {
            "tolerance_percent",
            100.0 * tolerance
        },

        {
            "trajectory_points",
                static_cast<qint64>(
                trajectory_plot_->point_count()
                )
          },

        {
            "recommended_burn_in_steps",
            static_cast<qint64>(
                recommended_steps
            )
        },
        {
            "start_time",
            start_time
        },
        {
            "early_velocity",
            early_velocity
        },
        {
            "late_velocity",
            late_velocity
        },
        {
            "tail_velocity",
            tail_velocity
        },
        {
            "velocity_difference_percent",
            100.0 * relative_difference
        },
        {
            "tail_r_squared",
            tail_r_squared
        }
    }
);
            if (!stable) {
    const QString tolerance_text = QString{
        "%1%"
    }.arg(
        100.0 * tolerance,
        0,
        'f',
        1
    );

    burn_in_info_label_->setText(
        QString{
            "Burn-in: inconclusive\n"
            "No stable tail found at %1 "
            "velocity tolerance.\n"
            "Increase tolerance, total time, "
            "or particle count."
        }.arg(tolerance_text)
    );

    update_plot_tools_();
    return;
}

            burn_in_info_label_->setText(
                QString{
                    "Burn-in: stable\n"
                    "Start: t = %1\n"
                    "Steps: %2\n"
                    "Early velocity: %3\n"
                    "Late velocity: %4\n"
                    "Tail velocity: %5\n"
                    "Difference: %6\n"
                    "Tail fit R²: %7"
                }
                    .arg(start_time, 0, 'g', 8)
                    .arg(
                        static_cast<qulonglong>(
                            recommended_steps
                        )
                    )
                    .arg(early_velocity, 0, 'e', 4)
                    .arg(late_velocity, 0, 'e', 4)
                    .arg(tail_velocity, 0, 'e', 4)
                    .arg(
                        QString{"%1%"}
                            .arg(
                                100.0 * relative_difference,
                                0,
                                'f',
                                2
                            )
                    )
                    .arg(tail_r_squared, 0, 'f', 4)
            );

            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::burn_in_validation_cleared,
        this,
        [this] {
            burn_in_info_label_->setText(
                "Burn-in: not validated"
            );

            update_plot_tools_();
        }
    );

    connect(
        trajectory_plot_,
        &TrajectoryPlotWidget::burn_in_validation_failed,
        this,
        [this](const QString& message) {
            QMessageBox::information(
                trajectory_dialog_,
                "Burn-in validation",
                message
            );

            update_plot_tools_();
        }
    );

    update_show_graph_button_();
    update_plot_tools_();

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

    log_experiment_event_(
    "application_started",
    QJsonObject{
        {
            "log_file",
            experiment_logger_.log_file_path()
        }
    }
    );

    append_log_(
        QString{
            "Experiment log: %1"
        }.arg(
            experiment_logger_.log_file_path()
        )
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

auto* potential_group = new QGroupBox{
    "Potential",
    central_widget
};

configure_group_title(
    potential_group
);

auto* potential_layout = new QVBoxLayout{
    potential_group
};

potential_layout->setContentsMargins(
    8,
    12,
    8,
    8
);

potential_layout->setSpacing(6);

auto* potential_buttons_layout =
    new QHBoxLayout;

open_potential_button_ = new QPushButton{
    "Open potential...",
    potential_group
};

builtin_potential_button_ = new QPushButton{
    "Use built-in",
    potential_group
};

potential_buttons_layout->addWidget(
    open_potential_button_
);

potential_buttons_layout->addWidget(
    builtin_potential_button_
);

potential_drop_area_ =
    new PotentialDropArea{
        potential_group
    };

potential_info_label_ = new QLabel{
    "Potential is not selected",
    potential_group
};

potential_info_label_->setWordWrap(
    true
);

potential_info_label_->setMinimumHeight(
    36
);

potential_parameters_scroll_ =
    new QScrollArea{
        potential_group
    };

potential_parameters_scroll_->
    setWidgetResizable(true);

potential_parameters_scroll_->
    setFrameShape(
        QFrame::NoFrame
    );

potential_parameters_scroll_->
    setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
    );

potential_parameters_scroll_->
    setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
    );

potential_parameters_scroll_->
    setMinimumHeight(70);

auto* potential_parameters_content =
    new QWidget{
        potential_parameters_scroll_
    };

potential_parameters_form_ =
    new QFormLayout{
        potential_parameters_content
    };

potential_parameters_form_->
    setContentsMargins(
        2,
        2,
        8,
        2
    );

potential_parameters_form_->
    setVerticalSpacing(6);

potential_parameters_form_->
    setRowWrapPolicy(
        QFormLayout::WrapLongRows
    );

potential_parameters_form_->
    setSizeConstraint(
        QLayout::SetMinAndMaxSize
    );

potential_parameters_scroll_->
    setWidget(
        potential_parameters_content
    );

potential_layout->addLayout(
    potential_buttons_layout
);

potential_layout->addWidget(
    potential_drop_area_
);

potential_layout->addWidget(
    potential_info_label_
);

potential_layout->addWidget(
    potential_parameters_scroll_,
    1
);

parameters_layout->addWidget(
    potential_group,
    1
);

    auto* modulation_group =
        new QGroupBox{"Modulation"};
    configure_group_title(modulation_group);
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

    parameters_layout->addWidget(modulation_group, 1);

    auto* simulation_group = new QGroupBox{
    "Simulation",
    central_widget
};
    configure_group_title(simulation_group);
auto* simulation_group_layout = new QVBoxLayout{
    simulation_group
};

simulation_group_layout->setContentsMargins(
    8,
    10,
    8,
    8
);

simulation_group_layout->setSpacing(0);

auto* simulation_scroll_area = new QScrollArea{
    simulation_group
};

simulation_scroll_area->setWidgetResizable(true);

simulation_scroll_area->setFrameShape(
    QFrame::NoFrame
);

simulation_scroll_area->setHorizontalScrollBarPolicy(
    Qt::ScrollBarAlwaysOff
);

simulation_scroll_area->setVerticalScrollBarPolicy(
    Qt::ScrollBarAsNeeded
);

auto* simulation_content = new QWidget{
    simulation_scroll_area
};

auto* simulation_form = new QFormLayout{
    simulation_content
};

simulation_form->setContentsMargins(
    4,
    2,
    8,
    2
);

simulation_form->setHorizontalSpacing(10);

simulation_form->setVerticalSpacing(7);

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

x0_spin_ = make_double_spin_box(
    -1.0e6,
    1.0e6,
    0.0,
    8,
    0.01
);

x0_spin_->setToolTip(
    "Initial coordinate x₀ used for every "
    "particle of the ensemble at t = 0."
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
    QString{
        "Auto (%1)"
    }.arg(
        static_cast<qulonglong>(
            hardware_threads
        )
    )
);

mode_combo_ = new QComboBox{
    simulation_content
};

mode_combo_->addItem(
    "Fast: final result only",
    false
);

mode_combo_->addItem(
    "Interactive: progress and live data",
    true
);

mode_combo_->setToolTip(
    "Fast mode avoids intermediate ensemble reductions. "
    "Interactive mode calculates in batches and provides "
    "real progress updates."
);

simulation_form->addRow(
    "Mode",
    mode_combo_
);

simulation_form->addRow(
    "dt",
    dt_spin_
);

simulation_form->addRow(
    "Total time",
    total_time_spin_
);

simulation_form->addRow(
    "Particles",
    particles_spin_
);

simulation_form->addRow(
    "Burn-in steps",
    burn_in_spin_
);

simulation_form->addRow(
    "Initial position x₀",
    x0_spin_
);

simulation_form->addRow(
    "Seed",
    seed_spin_
);

simulation_form->addRow(
    "Workers",
    workers_spin_
);

simulation_scroll_area->setWidget(
    simulation_content
);

simulation_group_layout->addWidget(
    simulation_scroll_area
);

parameters_layout->addWidget(
    simulation_group, 1
);

    constexpr int parameter_group_height = 300;

    potential_group->setFixedHeight(
        parameter_group_height
    );

    modulation_group->setFixedHeight(
        parameter_group_height
    );

    simulation_group->setFixedHeight(
        parameter_group_height
    );

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

    connect(
    open_potential_button_,
    &QPushButton::clicked,
    this,
    [this] {
        const QString file_name =
            QFileDialog::getOpenFileName(
                this,
                "Open potential configuration",
                QDir::homePath(),
                "Potential configuration "
                "(*.bmpotential *.json)"
            );

        if (!file_name.isEmpty()) {
            load_potential_file_(
                file_name
            );
        }
    }
);

    connect(
        builtin_potential_button_,
        &QPushButton::clicked,
        this,
        [this] {
            use_builtin_potential_();
        }
    );

    connect(
        potential_drop_area_,
        &PotentialDropArea::file_dropped,
        this,
        [this](const QString& file_name) {
            load_potential_file_(
                file_name
            );
        }
    );
}

SimulationRequest MainWindow::request_from_controls_() const {
    const bool interactive_mode =
        mode_combo_->currentData().toBool();

    return SimulationRequest{
        .v1 =
            active_potential_definition_
                ? 0.0
                : potential_parameter_spins_
                      .value("V1")
                      ->value(),

        .v2 =
            active_potential_definition_
                ? 0.0
                : potential_parameter_spins_
                      .value("V2")
                      ->value(),

        .potential_definition =
            active_potential_definition_,

        .potential_parameter_values =
            potential_values_from_controls_(),

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

        .x0 = x0_spin_->value(),

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

    if (
    potential_state_ !=
        PotentialState::Ready
    ){
        append_log_(
            "Simulation blocked: select a valid "
            "potential or use the built-in profile."
        );

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

    if (request.potential_definition) {
        try {
            const ExpressionEvaluator evaluator{
                *request.potential_definition,
                request.potential_parameter_values
            };

            const auto derivative_error =
                check_potential_derivative(
                    *request.potential_definition,
                    evaluator
                );

            if (derivative_error) {
                const QString message =
                    QString::fromStdString(
                        *derivative_error
                    );

                append_log_(
                    QString{
                        "Simulation start rejected: %1"
                    }.arg(message)
                );

                QMessageBox::warning(
                    this,
                    "Potential validation failed",
                    message
                );

                return;
            }
        } catch (
            const std::exception& exception
        ) {
            const QString message =
                QString::fromUtf8(
                    exception.what()
                );

            append_log_(
                QString{
                    "Simulation start rejected: %1"
                }.arg(message)
            );

            QMessageBox::warning(
                this,
                "Potential validation failed",
                message
            );

            return;
        }
    }

    const std::size_t total_steps =
        static_cast<std::size_t>(
            request.total_time / request.dt
        );

    const std::size_t total_updates =
        request.n_particles * total_steps;

    cancellation_source_ =
        std::make_shared<std::stop_source>();

    simulation_completed_ = false;
    trajectory_dt_ = request.dt;

    if (request.interactive_mode) {
        trajectory_plot_->clear_points();
        burn_in_info_label_->setText(
        "Burn-in: available after simulation completes/upon accumulating 40 points"
        );

        trend_info_label_->setText(
            "Trend: available after simulation completes"
        );

        select_trend_button_->setText(
            "Select trend start"
        );
    } else {
        trajectory_plot_->clear_points();

        burn_in_info_label_->setText(
            "Burn-in: unavailable in Fast mode"
        );

        trend_info_label_->setText(
            "Trend: unavailable in Fast mode"
        );

        select_trend_button_->setText(
            "Select trend start"
        );
    }

    update_plot_tools_();

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
        [this, request](
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

            simulation_completed_ =
                request.interactive_mode;

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

            log_experiment_event_(
    was_cancelled
        ? "simulation_cancelled"
        : "simulation_completed",
    QJsonObject{
        {
            "mode",
            request.interactive_mode
                ? "interactive"
                : "fast"
        },
        {
            "status",
            was_cancelled
                ? "cancelled_partial_result"
                : "completed"
        },
        {
            "mean_velocity",
            mean_velocity
        },
        {
            "mean_x_final",
            mean_x_final
        },
        {
            "elapsed_seconds",
            elapsed_seconds
        },
        {
            "throughput_million_updates_per_second",
            throughput
        },
        {
            "workers_used",
            static_cast<qint64>(workers)
        },
        {
            "particles",
            static_cast<qint64>(request.n_particles)
        },
        {
            "dt",
            request.dt
        },
        {
            "total_time_requested",
            request.total_time
        },
        {
            "burn_in_steps",
            static_cast<qint64>(
                request.burn_in_steps
            )
        },
        {
            "trajectory_points",
            request.interactive_mode
                ? static_cast<qint64>(
                    trajectory_plot_->point_count()
                )
                : QJsonValue{0}
        }
    }
);

            if (request.interactive_mode) {
                trend_info_label_->setText(
                    was_cancelled
                        ? "Trend: partial trajectory, "
                          "select a start point"
                        : "Trend: select a start point"
                );
            } else {
                trend_info_label_->setText(
                    "Trend: unavailable in Fast mode"
                );
            }

            update_plot_tools_();

            update_show_graph_button_();
        }
    );

    connect(
        simulation_worker_,
        &SimulationWorker::failed,
        this,
        [this, request](const QString& message) {
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

            log_experiment_event_(
    "simulation_failed",
    QJsonObject{
        {
            "mode",
            request.interactive_mode
                ? "interactive"
                : "fast"
        },
        {
            "error",
            message
        },
        {
            "particles",
            static_cast<qint64>(request.n_particles)
        },
        {
            "dt",
            request.dt
        },
        {
            "total_time",
            request.total_time
        },
        {
            "burn_in_steps",
            static_cast<qint64>(
                request.burn_in_steps
            )
        },
        {
            "seed",
            static_cast<qint64>(request.seed)
        }
    }
);

            simulation_completed_ = false;

            trajectory_plot_->clear_trend();

            trend_info_label_->setText(
                "Trend: unavailable because "
                  "the simulation failed"
            );

            update_plot_tools_();

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

    log_experiment_event_(
    "simulation_started",
    QJsonObject{
        {
            "mode",
            request.interactive_mode
                ? "interactive"
                : "fast"
        },
        {
            "v1",
            request.v1
        },
        {
            "v2",
            request.v2
        },
        {
            "modulation_amplitude",
            request.modulation_amplitude
        },
        {
            "epsilon",
            request.epsilon
        },
        {
            "alpha",
            request.alpha
        },
        {
            "dt",
            request.dt
        },
        {
            "total_time",
            request.total_time
        },
        {
            "x0",
            request.x0
        },
        {
            "total_steps",
            static_cast<qint64>(total_steps)
        },
        {
            "particles",
            static_cast<qint64>(request.n_particles)
        },
        {
            "burn_in_steps",
            static_cast<qint64>(
                request.burn_in_steps
            )
        },
        {
            "seed",
            static_cast<qint64>(request.seed)
        },
        {
            "requested_workers",
            request.requested_workers == 0
                ? QJsonValue{"auto"}
                : QJsonValue{
                    static_cast<qint64>(
                        request.requested_workers
                    )
                }
        },
        {
            "batch_steps",
            static_cast<qint64>(request.batch_steps)
        },
        {
            "cancellation_check_steps",
            static_cast<qint64>(
                request.cancellation_check_steps
            )
        }
    }
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

    log_experiment_event_(
    "cancellation_requested",
    QJsonObject{
        {
            "mode",
            mode_combo_->currentData().toBool()
                ? "interactive"
                : "fast"
        },
        {
            "elapsed_seconds",
            elapsed_clock_.isValid()
                ? static_cast<double>(
                    elapsed_clock_.elapsed()
                ) / 1000.0
                : 0.0
        }
    }
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

void MainWindow::begin_trend_selection_() {
    if (
        !simulation_completed_ ||
        !trajectory_plot_
    ) {
        return;
    }

    trajectory_plot_->begin_trend_selection();
}

void MainWindow::clear_trend_() {
    if (!trajectory_plot_) {
        return;
    }

    trajectory_plot_->clear_trend();
}

void MainWindow::validate_burn_in_() {
    if (
        !simulation_completed_ ||
        !trajectory_plot_ ||
        trajectory_dt_ <= 0.0
    ) {
        return;
    }

    trajectory_plot_->validate_burn_in(
    trajectory_dt_,
    burn_in_tolerance_spin_->value() / 100.0
    );
}

void MainWindow::clear_burn_in_validation_() {
    if (!trajectory_plot_) {
        return;
    }

    trajectory_plot_->clear_burn_in_validation();
}

void MainWindow::export_trajectory_() {
    if (
        !trajectory_plot_ ||
        trajectory_plot_->point_count() == 0
    ) {
        return;
    }

    const QString csv_filter =
        "CSV data (*.csv)";

    const QString png_filter =
        "PNG image (*.png)";

    const QString jpg_filter =
        "JPEG image (*.jpg *.jpeg)";

    QString selected_filter = csv_filter;

    QString file_name = QFileDialog::getSaveFileName(
        trajectory_dialog_,
        "Export trajectory",
        QDir::homePath() +
            "/mean_position_trajectory.csv",
        csv_filter +
            ";;" +
            png_filter +
            ";;" +
            jpg_filter,
        &selected_filter
    );

    if (file_name.isEmpty()) {
        return;
    }

    if (selected_filter == csv_filter) {
        if (!file_name.endsWith(
                ".csv",
                Qt::CaseInsensitive
            )) {
            file_name += ".csv";
            }

        export_trajectory_csv_(file_name);
        return;
    }

    if (selected_filter == png_filter) {
        if (!file_name.endsWith(
                ".png",
                Qt::CaseInsensitive
            )) {
            file_name += ".png";
            }

        export_plot_image_(
            file_name,
            "PNG"
        );

        return;
    }

    if (
        !file_name.endsWith(
            ".jpg",
            Qt::CaseInsensitive
        ) &&
        !file_name.endsWith(
            ".jpeg",
            Qt::CaseInsensitive
        )
    ) {
        file_name += ".jpg";
    }

    export_plot_image_(
        file_name,
        "JPG"
    );
}

void MainWindow::export_trajectory_csv_(
    const QString& file_name
) {
    QSaveFile file{file_name};

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(
            trajectory_dialog_,
            "Export CSV",
            QString{
                "Cannot write file:\n%1"
            }.arg(file.errorString())
        );

        return;
    }

    file.write("\xEF\xBB\xBF", 3);

    QTextStream stream{&file};

    stream.setEncoding(
        QStringConverter::Utf8
    );

    stream.setRealNumberNotation(
        QTextStream::ScientificNotation
    );

    stream.setRealNumberPrecision(12);

    const std::vector<QPointF> points =
        trajectory_plot_->points();

    const TrajectoryPlotWidget::LinearTrend trend =
        trajectory_plot_->trend();

    stream << "time;mean_x;trend_x;is_trend_region\n";

    for (
        std::size_t index = 0;
        index < points.size();
        ++index
    ) {
        const QPointF& point = points[index];

        stream << point.x() << ';'
               << point.y() << ';';

        if (
            trend.valid &&
            index >= trend.start_index
        ) {
            const double trend_x =
                trend.intercept +
                trend.slope * point.x();

            stream << trend_x << ";1\n";
        } else {
            stream << ";0\n";
        }
    }

    if (!file.commit()) {
        QMessageBox::warning(
            trajectory_dialog_,
            "Export CSV",
            QString{
                "Cannot save file:\n%1"
            }.arg(file.errorString())
        );

        return;
    }

    QMessageBox::information(
        trajectory_dialog_,
        "Export CSV",
        QString{
            "Trajectory exported to:\n%1"
        }.arg(
            QDir::toNativeSeparators(
                file_name
            )
        )
    );

    log_experiment_event_(
    "trajectory_exported",
    QJsonObject{
        {
            "format",
            "csv"
        },
        {
            "file_path",
            QFileInfo{file_name}.absoluteFilePath()
        },
        {
            "trajectory_points",
            static_cast<qint64>(
                trajectory_plot_->point_count()
            )
        },
        {
            "has_trend",
            trajectory_plot_->has_trend()
        },
        {
            "has_burn_in_validation",
            trajectory_plot_->has_burn_in_validation()
        }
    }
);
}

void MainWindow::export_plot_image_(
    const QString& file_name,
    const QString& format
) {
    const QPixmap plot_image =
        trajectory_plot_->grab();

    const int quality =
        format == "JPG"
            ? 95
            : -1;

    if (!plot_image.save(
            file_name,
            format.toUtf8().constData(),
            quality
        )) {
        QMessageBox::warning(
            trajectory_dialog_,
            QString{"Export %1"}.arg(format),
            QString{
                "Cannot save image:\n%1"
            }.arg(
                QDir::toNativeSeparators(
                    file_name
                )
            )
        );

        return;
        }

    QMessageBox::information(
        trajectory_dialog_,
        QString{"Export %1"}.arg(format),
        QString{
            "Graph exported to:\n%1"
        }.arg(
            QDir::toNativeSeparators(
                file_name
            )
        )
    );

    log_experiment_event_(
    "plot_exported",
    QJsonObject{
        {
            "format",
            format.toLower()
        },
        {
            "file_path",
            QFileInfo{file_name}.absoluteFilePath()
        },
        {
            "trajectory_points",
            static_cast<qint64>(
                trajectory_plot_->point_count()
            )
        },
        {
            "has_trend",
            trajectory_plot_->has_trend()
        },
        {
            "has_burn_in_validation",
            trajectory_plot_->has_burn_in_validation()
        }
    }
);

}

void MainWindow::update_plot_tools_() {
    if (
        !trajectory_plot_ ||
        !select_trend_button_ ||
        !clear_trend_button_ ||
        !highlight_trend_range_check_ ||
        !validate_burn_in_button_ ||
        !burn_in_tolerance_spin_ ||
        !clear_burn_in_button_ ||
        !export_button_
    ) {
        return;
    }

    const bool trend_is_available =
        simulation_completed_ &&
        trajectory_plot_->point_count() >= 3;

    const bool export_is_available =
    simulation_completed_ &&
    trajectory_plot_->point_count() > 0;

    const bool burn_in_validation_is_available =
    simulation_completed_ &&
    trajectory_plot_->point_count() >= 40;

    burn_in_tolerance_spin_->setEnabled(
    burn_in_validation_is_available
    );

    select_trend_button_->setEnabled(
        trend_is_available &&
        !trajectory_plot_->is_selecting_trend_start()
    );

    clear_trend_button_->setEnabled(
        trend_is_available &&
        (
            trajectory_plot_->has_trend() ||
            trajectory_plot_->is_selecting_trend_start()
        )
    );

    highlight_trend_range_check_->setEnabled(
    trajectory_plot_->has_trend()
    );

    export_button_->setEnabled(
        export_is_available
    );

    validate_burn_in_button_->setEnabled(
    burn_in_validation_is_available
    );

    clear_burn_in_button_->setEnabled(
        trajectory_plot_->has_burn_in_validation()
    );
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
    cancel_button_->setEnabled(is_running);

    mode_combo_->setEnabled(!is_running);

    amplitude_spin_->setEnabled(!is_running);
    epsilon_spin_->setEnabled(!is_running);
    alpha_spin_->setEnabled(!is_running);

    dt_spin_->setEnabled(!is_running);
    total_time_spin_->setEnabled(!is_running);
    x0_spin_->setEnabled(!is_running);

    particles_spin_->setEnabled(!is_running);
    burn_in_spin_->setEnabled(!is_running);
    seed_spin_->setEnabled(!is_running);
    workers_spin_->setEnabled(!is_running);

    update_potential_controls_(
        is_running
    );
    update_show_graph_button_();
    update_plot_tools_();
}

void MainWindow::append_log_(
    const QString& message
) {
    log_output_->appendPlainText(message);
}

void MainWindow::rebuild_potential_fields_(
    const std::vector<
        PotentialParameterDefinition
    >& parameters
) {
    potential_parameter_spins_.clear();

    while (
        potential_parameters_form_->rowCount() > 0
    ) {
        potential_parameters_form_->
            removeRow(0);
    }

    for (const auto& parameter : parameters) {
        auto* spin = make_double_spin_box(
            parameter.minimum,
            parameter.maximum,
            parameter.default_value,
            parameter.decimals,
            parameter.step
        );

        spin->setToolTip(
            QString::fromStdString(
                parameter.description
            )
        );

        const QString label =
            QString::fromStdString(
                parameter.label
            );

        const QString id =
            QString::fromStdString(
                parameter.id
            );

        potential_parameters_form_->addRow(
            label + ":",
            spin
        );

        potential_parameter_spins_.insert(
            id,
            spin
        );
    }
}

PotentialParameterValues
MainWindow::potential_values_from_controls_() const {
    PotentialParameterValues values;

    for (
        auto iterator =
            potential_parameter_spins_.cbegin();

        iterator !=
            potential_parameter_spins_.cend();

        ++iterator
    ) {
        values.emplace(
            iterator.key().toStdString(),
            iterator.value()->value()
        );
    }

    return values;
}

void MainWindow::use_builtin_potential_() {
    if (
        simulation_thread_ ||
        potential_state_ ==
            PotentialState::Validating
    ) {
        return;
    }

    const std::vector<
        PotentialParameterDefinition
    > parameters{
        PotentialParameterDefinition{
            .id = "V1",
            .label = "V₁",
            .description =
                "Amplitude of the first harmonic.",
            .default_value = 0.20,
            .minimum = -100.0,
            .maximum = 100.0,
            .step = 0.01,
            .decimals = 8
        },

        PotentialParameterDefinition{
            .id = "V2",
            .label = "V₂",
            .description =
                "Amplitude of the second harmonic.",
            .default_value = 0.10,
            .minimum = -100.0,
            .maximum = 100.0,
            .step = 0.01,
            .decimals = 8
        }
    };

    rebuild_potential_fields_(
        parameters
    );

    active_potential_definition_.reset();

    potential_state_ =
        PotentialState::Ready;

    potential_info_label_->setText(
        "Built-in biharmonic profile\n"
        "Periodic; period = 1"
    );

    potential_drop_area_->set_message(
        "Built-in profile selected. "
        "Drop a potential file to replace it."
    );

    update_potential_controls_(
        false
    );

    append_log_(
        "Selected built-in biharmonic profile."
    );

    log_experiment_event_(
        "potential_builtin_selected"
    );
}

void MainWindow::update_potential_controls_(
    bool is_running
) {
    const bool validating =
        potential_state_ ==
        PotentialState::Validating;

    run_button_->setEnabled(
        !is_running &&
        potential_state_ ==
            PotentialState::Ready
    );

    open_potential_button_->setEnabled(
        !is_running &&
        !validating
    );

    builtin_potential_button_->setEnabled(
        !is_running &&
        !validating
    );

    potential_drop_area_->setEnabled(
        !is_running &&
        !validating
    );

    potential_parameters_scroll_->setEnabled(
        !is_running &&
        potential_state_ ==
            PotentialState::Ready
    );
}

void MainWindow::load_potential_file_(
    const QString& file_name
) {
    if (
        simulation_thread_ ||
        potential_state_ ==
            PotentialState::Validating
    ) {
        return;
    }

    potential_state_ =
        PotentialState::Validating;

    potential_drop_area_->set_message(
        "Validating potential..."
    );

    update_potential_controls_(
        false
    );

    QTimer::singleShot(
        0,
        this,
        [this, file_name] {
            const QFileInfo file_info{
                file_name
            };

            QString failure_reason;

            if (
                !file_name.endsWith(
                    ".bmpotential",
                    Qt::CaseInsensitive
                ) &&
                !file_name.endsWith(
                    ".json",
                    Qt::CaseInsensitive
                )
            ) {
                failure_reason =
                    "Expected a .bmpotential "
                    "or .json file.";
            } else if (
                file_info.exists() &&
                file_info.size() >
                    1'048'576
            ) {
                failure_reason =
                    "Potential file exceeds "
                    "the 1 MiB limit.";
            }

            PotentialConfigParseResult result;

            if (failure_reason.isEmpty()) {
                result =
                    PotentialConfigParser::
                        parse_file(
                            file_name
                        );

                if (
                    !result.is_success()
                ) {
                    failure_reason =
                        result.error_message;
                }
            }

            if (
                !failure_reason.isEmpty()
            ) {
                potential_state_ =
                    PotentialState::Invalid;

                potential_drop_area_->set_message(
                    "Invalid potential. "
                    "Load another file or use built-in.",
                    failure_reason
                );

                potential_info_label_->setText(
                    "Potential validation failed.\n"
                    "Simulation is disabled."
                );

                append_log_(
                    QString{
                        "Potential rejected: %1\n%2"
                    }
                        .arg(
                            file_name,
                            failure_reason
                        )
                );

                log_experiment_event_(
                    "potential_rejected",
                    QJsonObject{
                        {
                            "file_path",
                            file_info
                                .absoluteFilePath()
                        },
                        {
                            "error",
                            failure_reason
                        }
                    }
                );

                update_potential_controls_(
                    false
                );

                return;
            }

            const PotentialDefinition&
                definition =
                    *result.definition;

            rebuild_potential_fields_(
                definition.parameters
            );

            active_potential_definition_ =
                std::move(
                    result.definition
                );

            potential_state_ =
                PotentialState::Ready;

            const auto& active =
                *active_potential_definition_;

            potential_info_label_->setText(
                QString{
                    "%1\n%2"
                }
                    .arg(
                        QString::fromStdString(
                            active.name
                        ),
                        active.profile.periodic
                            ? QString{
                                "Periodic; period = %1"
                            }.arg(
                                active.profile.period,
                                0,
                                'g',
                                12
                            )
                            : QString{
                                "Non-periodic"
                            }
                    )
            );

            potential_drop_area_->set_message(
                QString{
                    "Loaded: %1"
                }.arg(
                    QString::fromStdString(
                        active.name
                    )
                ),
                file_info.absoluteFilePath()
            );

            append_log_(
                QString{
                    "Potential loaded: %1\n%2"
                }.arg(
                    QString::fromStdString(
                        active.name
                    ),
                    file_info.absoluteFilePath()
                )
            );

            QJsonArray warnings;

            for (
                const QString& warning :
                result.warnings
            ) {
                warnings.append(
                    warning
                );

                append_log_(
                    QString{
                        "Potential warning: %1"
                    }.arg(warning)
                );
            }

            log_experiment_event_(
                "potential_loaded",
                QJsonObject{
                    {
                        "id",
                        QString::fromStdString(
                            active.id
                        )
                    },
                    {
                        "name",
                        QString::fromStdString(
                            active.name
                        )
                    },
                    {
                        "file_path",
                        file_info
                            .absoluteFilePath()
                    },
                    {
                        "warnings",
                        warnings
                    }
                }
            );

            update_potential_controls_(
                false
            );
        }
    );
}

void MainWindow::log_experiment_event_(
    const QString& event_name,
    const QJsonObject& data
) {
    if (
        experiment_logger_.write_event(
            event_name,
            data
        )
    ) {
        return;
    }

    append_log_(
        QString{
            "Warning: unable to write experiment log: %1"
        }.arg(
            experiment_logger_.last_error()
        )
    );
}