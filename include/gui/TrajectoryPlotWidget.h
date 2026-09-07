#ifndef BROWNIAN_MOTOR_TRAJECTORYPLOTWIDGET_H
#define BROWNIAN_MOTOR_TRAJECTORYPLOTWIDGET_H
#pragma once

#include <QPointF>
#include <QWidget>

#include <vector>

class QPaintEvent;

class TrajectoryPlotWidget final : public QWidget {
public:
    explicit TrajectoryPlotWidget(
        QWidget* parent = nullptr
    );

    void clear_points();
    void append_point(double time, double mean_x);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] static QString format_value_(
        double value
    );

    std::vector<QPointF> points_;
};

#endif // BROWNIAN_MOTOR_TRAJECTORYPLOTWIDGET_H