#include "gui/InitialPositionPanel.h"
#include "gui/CompactDoubleSpinBox.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QString>
#include <cmath>
#include <stdexcept>

namespace {
    QDoubleSpinBox *make_initial_spin(QWidget *parent, double value) {
        auto *spin = new CompactDoubleSpinBox;
        spin->setParent(parent);
        spin->setDecimals(12);
        spin->setRange(-1.0e12, 1.0e12);
        spin->setSingleStep(0.01);
        spin->setValue(value);
        spin->setKeyboardTracking(false);
        spin->setMinimumWidth(0);
        auto policy = spin->sizePolicy();
        policy.setHorizontalPolicy(QSizePolicy::Ignored);
        spin->setSizePolicy(policy);
        return spin;
    }
}

InitialPositionPanel::InitialPositionPanel(
    QDoubleSpinBox *point_position,
    QWidget *parent
) : QWidget{parent}, point_{point_position} {
    if (!point_) throw std::invalid_argument("Initial position control must not be null");
    auto *form = new QFormLayout{this};
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(5);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);

    kind_ = new QComboBox{this};
    kind_->addItem("Point", static_cast<int>(InitialPositionKind::Point));
    kind_->addItem("Uniform", static_cast<int>(InitialPositionKind::Uniform));
    kind_->addItem("Gaussian", static_cast<int>(InitialPositionKind::Gaussian));
    kind_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    kind_->setMinimumContentsLength(9);
    auto combo_policy=kind_->sizePolicy();
    combo_policy.setHorizontalPolicy(QSizePolicy::Ignored);
    kind_->setSizePolicy(combo_policy);
    form->addRow("Initial distribution", kind_);

    point_->setParent(this);
    point_label_=new QLabel{"Position", this};
    lower_label_=new QLabel{"From", this};
    upper_label_=new QLabel{"To", this};
    mean_label_=new QLabel{"Mean", this};
    sigma_label_=new QLabel{"Std. deviation", this};
    lower_=make_initial_spin(this, 0.0);
    upper_=make_initial_spin(this, 1.0);
    mean_=make_initial_spin(this, 0.0);
    sigma_=make_initial_spin(this, 0.1);
    sigma_->setRange(1.0e-12, 1.0e12);

    form->addRow(point_label_, point_);
    form->addRow(lower_label_, lower_);
    form->addRow(upper_label_, upper_);
    form->addRow(mean_label_, mean_);
    form->addRow(sigma_label_, sigma_);

    one_period_=new QPushButton{"One period", this};
    one_period_->setToolTip("Fill From = 0 and To = the current spatial period");
    form->addRow(one_period_);

    connect(kind_, &QComboBox::currentIndexChanged, this, [this](int) {
        update_fields_();
    });
    connect(one_period_, &QPushButton::clicked, this, [this] {
        if (!period_available_) return;
        lower_->setValue(0.0);
        upper_->setValue(period_);
    });
    update_fields_();
}

InitialPositionDistribution InitialPositionPanel::settings() const {
    InitialPositionDistribution result;
    result.kind=static_cast<InitialPositionKind>(kind_->currentData().toInt());
    result.lower=lower_->value();
    result.upper=upper_->value();
    result.mean=mean_->value();
    result.standard_deviation=sigma_->value();
    return result;
}

void InitialPositionPanel::set_period_available(bool available, double period) {
    period_available_=available && std::isfinite(period) && period > 0.0 && period <= 1.0e12;
    period_=period;
    if (period_available_) {
        // Avoid silently rounding a very small period to zero in the UI.
        if (period < 1.0e-12) period_available_=false;
    }
    update_fields_();
}

void InitialPositionPanel::update_fields_() {
    const auto kind=static_cast<InitialPositionKind>(kind_->currentData().toInt());
    const bool point=kind==InitialPositionKind::Point;
    const bool uniform=kind==InitialPositionKind::Uniform;
    const bool gaussian=kind==InitialPositionKind::Gaussian;
    point_label_->setVisible(point); point_->setVisible(point);
    lower_label_->setVisible(uniform); lower_->setVisible(uniform);
    upper_label_->setVisible(uniform); upper_->setVisible(uniform);
    mean_label_->setVisible(gaussian); mean_->setVisible(gaussian);
    sigma_label_->setVisible(gaussian); sigma_->setVisible(gaussian);
    one_period_->setVisible(uniform);
    one_period_->setEnabled(period_available_);
}
