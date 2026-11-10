#ifndef BROWNIAN_MOTOR_POTENTIALPREVIEWDIALOG_H
#define BROWNIAN_MOTOR_POTENTIALPREVIEWDIALOG_H
#pragma once

#include <QDialog>

#include "core/PotentialDefinition.h"

class PotentialPreviewDialog final : public QDialog {
public:
    explicit PotentialPreviewDialog(
        const PotentialDefinition &definition,
        const PotentialParameterValues &parameter_values,
        QWidget *parent = nullptr
    );
};

#endif // BROWNIAN_MOTOR_POTENTIALPREVIEWDIALOG_H
