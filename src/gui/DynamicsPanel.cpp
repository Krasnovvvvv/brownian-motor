#include "gui/DynamicsPanel.h"
#include "gui/CompactDoubleSpinBox.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSizePolicy>

DynamicsPanel::DynamicsPanel(QWidget *parent) : QWidget{parent} {
    auto *form = new QFormLayout{this};
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(5);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    model_ = new QComboBox{this};
    model_->addItem("Overdamped", static_cast<int>(DynamicsModel::Overdamped));
    model_->addItem("Inertial", static_cast<int>(DynamicsModel::Inertial));
    epsilon_ = new CompactDoubleSpinBox;
    epsilon_->setParent(this);
    epsilon_->setDecimals(12);
    epsilon_->setRange(1e-12, 1e6);
    epsilon_->setSingleStep(0.01);
    epsilon_->setValue(0.1);
    epsilon_->setKeyboardTracking(false);
    epsilon_->setToolTip("Inertia epsilon = m/(gamma*tau_D). Different from modulation epsilon.");
    velocity_ = new QComboBox{this};
    velocity_->addItem("Thermal", static_cast<int>(InitialVelocityKind::Thermal));
    velocity_->addItem("Zero", static_cast<int>(InitialVelocityKind::Zero));
    velocity_->setToolTip("Thermal: v(0) ~ N(0, 1/epsilon); Zero: every initial velocity is zero.");
    epsilon_label_ = new QLabel{"Inertia epsilon", this};
    velocity_label_ = new QLabel{"Initial velocity", this};
    form->addRow("Dynamics", model_);
    form->addRow(epsilon_label_, epsilon_);
    form->addRow(velocity_label_, velocity_);
    for (QComboBox *combo: {model_, velocity_}) {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(9);
        auto policy = combo->sizePolicy();
        policy.setHorizontalPolicy(QSizePolicy::Ignored);
        combo->setSizePolicy(policy);
    }
    auto policy = epsilon_->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Ignored);
    epsilon_->setSizePolicy(policy);
    connect(model_, &QComboBox::currentIndexChanged, this, [this](int) { update_fields_(); });
    update_fields_();
}

DynamicsSettings DynamicsPanel::settings() const {
    return {
        static_cast<DynamicsModel>(model_->currentData().toInt()), epsilon_->value(),
        static_cast<InitialVelocityKind>(velocity_->currentData().toInt())
    };
}

void DynamicsPanel::update_fields_() {
    const bool inertial = static_cast<DynamicsModel>(model_->currentData().toInt()) == DynamicsModel::Inertial;
    epsilon_label_->setVisible(inertial);
    epsilon_->setVisible(inertial);
    velocity_label_->setVisible(inertial);
    velocity_->setVisible(inertial);
}

QFormLayout *DynamicsPanel::form_layout() const noexcept {
    return dynamic_cast<QFormLayout *>(layout());
}
