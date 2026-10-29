#include "gui/ModulationPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <numbers>
#include <stdexcept>

namespace {
QDoubleSpinBox* spin(QWidget* parent, double lo, double hi,
                     double value, int decimals, double step) {
    auto* box = new QDoubleSpinBox(parent);
    box->setLocale(QLocale::c());
    box->setDecimals(decimals);
    box->setRange(lo, hi);
    box->setValue(value);
    box->setSingleStep(step);
    box->setKeyboardTracking(false);
    return box;
}
QFormLayout* form(QWidget* page) {
    auto* layout = new QFormLayout(page);
    layout->setContentsMargins(0, 4, 4, 4);
    layout->setVerticalSpacing(6);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    return layout;
}
}

ModulationPanel::ModulationPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    category_ = new QComboBox(this);
    category_->addItem("Stochastic");
    category_->addItem("Deterministic");
    auto* category_form = new QFormLayout;
    category_form->setContentsMargins(0, 0, 0, 0);
    category_form->addRow("Mode", category_);
    root->addLayout(category_form);

    deterministic_selector_ = new QWidget(this);
    auto* selector_form = form(deterministic_selector_);
    deterministic_kind_ = new QComboBox(deterministic_selector_);
    deterministic_kind_->addItem("Constant", static_cast<int>(ModulationKind::Constant));
    deterministic_kind_->addItem("Sinusoidal", static_cast<int>(ModulationKind::Sinusoidal));
    selector_form->addRow("Profile", deterministic_kind_);
    root->addWidget(deterministic_selector_);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pages_ = new QStackedWidget(scroll);
    scroll->setWidget(pages_);
    root->addWidget(scroll, 1);

    auto* noise_page = new QWidget(pages_);
    auto* noise_form = form(noise_page);
    factor_a_ = spin(noise_page, -1000, 1000, 1, 8, 0.1);
    factor_b_ = spin(noise_page, -1000, 1000, -1.0 / 3.0, 8, 0.1);
    lifetime_a_ = spin(noise_page, 1e-8, 1e6, 0.15, 8, 0.01);
    lifetime_b_ = spin(noise_page, 1e-8, 1e6, 0.15, 8, 0.01);
    lifetime_a_->setToolTip("Mean residence time in A, not a fixed duration. Rate A -> B = 1 / mean lifetime A.");
    lifetime_b_->setToolTip("Mean residence time in B, not a fixed duration. Rate B -> A = 1 / mean lifetime B.");
    noise_form->addRow("Factor A", factor_a_);
    noise_form->addRow("Factor B", factor_b_);
    noise_form->addRow("Mean lifetime A", lifetime_a_);
    noise_form->addRow("Mean lifetime B", lifetime_b_);
    noise_summary_ = new QLabel(noise_page);
    noise_summary_->setWordWrap(true);
    noise_form->addRow(noise_summary_);
    pages_->addWidget(noise_page);

    auto* constant_page = new QWidget(pages_);
    auto* constant_form = form(constant_page);
    constant_factor_ = spin(constant_page, -1000, 1000, 1, 8, 0.1);
    constant_form->addRow("Factor", constant_factor_);
    pages_->addWidget(constant_page);

    auto* sine_page = new QWidget(pages_);
    auto* sine_form = form(sine_page);
    sine_offset_ = spin(sine_page, -1000, 1000, 0, 8, 0.1);
    sine_amplitude_ = spin(sine_page, 0, 1000, 1, 8, 0.1);
    sine_period_ = spin(sine_page, 1e-8, 1e6, 1, 8, 0.1);
    sine_phase_ = spin(sine_page, -360, 360, 0, 6, 15);
    sine_phase_->setSuffix(" deg");
    sine_period_->setToolTip("Period in model time; independent of solver dt.");
    sine_form->addRow("Offset", sine_offset_);
    sine_form->addRow("Amplitude", sine_amplitude_);
    sine_form->addRow("Period", sine_period_);
    sine_form->addRow("Phase", sine_phase_);
    auto* equation = new QLabel("f(t) = offset + amplitude * sin(2*pi*t / period + phase)", sine_page);
    equation->setWordWrap(true);
    sine_form->addRow(equation);
    pages_->addWidget(sine_page);

    connect(category_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { update_pages_(); });
    connect(deterministic_kind_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { update_pages_(); });
    for (auto* box : {factor_a_, factor_b_, lifetime_a_, lifetime_b_}) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { update_noise_summary_(); });
    }
    update_pages_();
    update_noise_summary_();
}

void ModulationPanel::update_pages_() {
    const bool deterministic = category_->currentIndex() == 1;
    deterministic_selector_->setVisible(deterministic);
    if (!deterministic) {
        pages_->setCurrentIndex(0);
    } else {
        const auto kind = static_cast<ModulationKind>(deterministic_kind_->currentData().toInt());
        pages_->setCurrentIndex(kind == ModulationKind::Constant ? 1 : 2);
    }
}

void ModulationPanel::update_noise_summary_() {
    const double a = lifetime_a_->value();
    const double b = lifetime_b_->value();
    const double p_a = a / (a + b);
    const double mean = p_a * factor_a_->value() + (1.0 - p_a) * factor_b_->value();
    noise_summary_->setText(
        QString("Stationary: A %1%, B %2%\nMean factor: %3")
            .arg(100.0 * p_a, 0, 'f', 2)
            .arg(100.0 * (1.0 - p_a), 0, 'f', 2)
            .arg(mean, 0, 'g', 8));
}

ModulationSettings ModulationPanel::settings() const {
    ModulationSettings result;
    if (category_->currentIndex() == 0) {
        result.kind = ModulationKind::MarkovTwoState;
        result.factor_a = factor_a_->value();
        result.factor_b = factor_b_->value();
        result.gamma_a = 1.0 / lifetime_a_->value();
        result.gamma_b = 1.0 / lifetime_b_->value();
    } else {
        result.kind = static_cast<ModulationKind>(deterministic_kind_->currentData().toInt());
        if (result.kind == ModulationKind::Constant) {
            result.constant_factor = constant_factor_->value();
        } else {
            result.sinusoidal_offset = sine_offset_->value();
            result.sinusoidal_amplitude = sine_amplitude_->value();
            result.sinusoidal_period = sine_period_->value();
            result.sinusoidal_phase = sine_phase_->value() * std::numbers::pi_v<double> / 180.0;
        }
    }
    result.validate();
    return result;
}

QJsonObject ModulationPanel::settings_json(const ModulationSettings& s) {
    s.validate();
    switch (s.kind) {
    case ModulationKind::MarkovTwoState:
        return {
            {"mode", "stochastic"}, {"modulation_kind", "markov_two_state"},
            {"factor_a", s.factor_a}, {"factor_b", s.factor_b},
            {"mean_lifetime_a", 1.0 / s.gamma_a}, {"mean_lifetime_b", 1.0 / s.gamma_b},
            {"gamma_a", s.gamma_a}, {"gamma_b", s.gamma_b},
            {"initial_state", "stationary"}, {"scope", "independent_per_particle"}
        };
    case ModulationKind::Constant:
        return {{"mode", "deterministic"}, {"modulation_kind", "constant"},
                {"factor", s.constant_factor}};
    case ModulationKind::Sinusoidal:
        return {
            {"mode", "deterministic"}, {"modulation_kind", "sinusoidal"},
            {"offset", s.sinusoidal_offset}, {"amplitude", s.sinusoidal_amplitude},
            {"period", s.sinusoidal_period},
            {"omega", 2.0 * std::numbers::pi_v<double> / s.sinusoidal_period},
            {"phase_radians", s.sinusoidal_phase}, {"initial_time", 0.0}
        };
    default:
        throw std::invalid_argument("This panel does not expose legacy modulation kinds");
    }
}
