#ifndef BROWNIAN_MOTOR_TRAJECTORYPLOTWIDGET_H
#define BROWNIAN_MOTOR_TRAJECTORYPLOTWIDGET_H
#pragma once

#include <QPointF>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <optional>
#include <vector>

class QMouseEvent;
class QPaintEvent;

class TrajectoryPlotWidget final : public QWidget {
    Q_OBJECT

public:
    struct LinearTrend {
        bool valid{false};

        std::size_t start_index{0};

        double start_time{0.0};
        double intercept{0.0};
        double slope{0.0};
        double r_squared{0.0};
    };

    explicit TrajectoryPlotWidget(
        QWidget* parent = nullptr
    );

    void clear_points();
    void append_point(double time, double mean_x);

    void begin_trend_selection();
    void clear_trend();

    [[nodiscard]] bool has_trend() const;
    [[nodiscard]] LinearTrend trend() const;

signals:
    void trend_selection_requested();
    void trend_changed(
        double start_time,
        double intercept,
        double slope,
        double r_squared
    );
    void trend_cleared();
    void trend_selection_failed(
        const QString& message
    );

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct PlotBounds {
        QRectF rect;

        double minimum_time{0.0};
        double maximum_time{1.0};

        double minimum_x{0.0};
        double maximum_x{1.0};
    };

    [[nodiscard]] static QString format_value_(
        double value
    );

    [[nodiscard]] PlotBounds plot_bounds_() const;

    [[nodiscard]] QPointF map_to_plot_(
        const QPointF& point,
        const PlotBounds& bounds
    ) const;

    [[nodiscard]] std::optional<std::size_t>
    nearest_point_index_(
        const QPointF& mouse_position,
        const PlotBounds& bounds
    ) const;

    [[nodiscard]] bool calculate_trend_(
        std::size_t start_index
    );

    std::vector<QPointF> points_;

    std::optional<std::size_t> hovered_index_;
    bool selecting_trend_start_{false};

    LinearTrend trend_;
};

#endif // BROWNIAN_MOTOR_TRAJECTORYPLOTWIDGET_H