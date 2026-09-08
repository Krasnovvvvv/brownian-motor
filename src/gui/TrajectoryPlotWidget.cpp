#include "gui/TrajectoryPlotWidget.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {

constexpr int left_margin = 72;
constexpr int right_margin = 24;
constexpr int top_margin = 26;
constexpr int bottom_margin = 52;

constexpr int tick_count = 5;
constexpr int maximum_points = 5'000;

constexpr double hover_distance_pixels = 18.0;

[[nodiscard]] double expand_range(
    double minimum,
    double maximum
) {
    const double range = maximum - minimum;

    if (range > 0.0 && std::isfinite(range)) {
        return range;
    }

    const double magnitude = std::max(
        1.0,
        std::abs(minimum)
    );

    return 0.1 * magnitude;
}

} // namespace

TrajectoryPlotWidget::TrajectoryPlotWidget(
    QWidget* parent
)
    : QWidget{parent}
{
    setMinimumHeight(240);

    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
    );

    setMouseTracking(true);
}

void TrajectoryPlotWidget::clear_points() {
    points_.clear();

    hovered_index_.reset();
    selecting_trend_start_ = false;
    trend_ = {};

    unsetCursor();

    emit point_count_changed(0);
    emit trend_cleared();

    update();
}

void TrajectoryPlotWidget::append_point(
    double time,
    double mean_x
) {
    if (!std::isfinite(time) || !std::isfinite(mean_x)) {
        return;
    }

    if (!points_.empty() && time < points_.back().x()) {
        clear_points();
    }

    points_.emplace_back(time, mean_x);

    if (points_.size() > maximum_points) {
        points_.erase(points_.begin());

        if (trend_.valid) {
            if (trend_.start_index > 0) {
                --trend_.start_index;
            } else {
                trend_ = {};
                emit trend_cleared();
            }
        }
    }

    emit point_count_changed(points_.size());

    update();
}

void TrajectoryPlotWidget::begin_trend_selection() {
    if (points_.size() < 3) {
        emit trend_selection_failed(
            "At least three points are required "
            "to calculate a linear trend."
        );

        return;
    }

    selecting_trend_start_ = true;

    setCursor(Qt::CrossCursor);

    emit trend_selection_requested();

    update();
}

void TrajectoryPlotWidget::clear_trend() {
    if (!trend_.valid && !selecting_trend_start_) {
        return;
    }

    selecting_trend_start_ = false;
    trend_ = {};

    unsetCursor();

    emit trend_cleared();

    update();
}

bool TrajectoryPlotWidget::has_trend() const {
    return trend_.valid;
}

bool TrajectoryPlotWidget::is_selecting_trend_start() const {
    return selecting_trend_start_;
}

std::size_t TrajectoryPlotWidget::point_count() const {
    return points_.size();
}

std::vector<QPointF>
TrajectoryPlotWidget::points() const {
    return points_;
}

TrajectoryPlotWidget::LinearTrend
TrajectoryPlotWidget::trend() const {
    return trend_;
}

TrajectoryPlotWidget::PlotBounds
TrajectoryPlotWidget::plot_bounds_() const {
    PlotBounds bounds;

    bounds.rect = QRectF{
        static_cast<qreal>(left_margin),
        static_cast<qreal>(top_margin),
        static_cast<qreal>(
            std::max(
                1,
                width() - left_margin - right_margin
            )
        ),
        static_cast<qreal>(
            std::max(
                1,
                height() - top_margin - bottom_margin
            )
        )
    };

    if (points_.empty()) {
        return bounds;
    }

    double minimum_time =
        std::numeric_limits<double>::infinity();

    double maximum_time =
        -std::numeric_limits<double>::infinity();

    double minimum_x =
        std::numeric_limits<double>::infinity();

    double maximum_x =
        -std::numeric_limits<double>::infinity();

    for (const QPointF& point : points_) {
        minimum_time = std::min(
            minimum_time,
            point.x()
        );

        maximum_time = std::max(
            maximum_time,
            point.x()
        );

        minimum_x = std::min(
            minimum_x,
            point.y()
        );

        maximum_x = std::max(
            maximum_x,
            point.y()
        );
    }

    const double time_padding =
        0.03 * expand_range(
            minimum_time,
            maximum_time
        );

    const double x_padding =
        0.10 * expand_range(
            minimum_x,
            maximum_x
        );

    bounds.minimum_time = std::max(
        0.0,
        minimum_time - time_padding
    );

    bounds.maximum_time =
        maximum_time + time_padding;

    bounds.minimum_x =
        minimum_x - x_padding;

    bounds.maximum_x =
        maximum_x + x_padding;

    return bounds;
}

QPointF TrajectoryPlotWidget::map_to_plot_(
    const QPointF& point,
    const PlotBounds& bounds
) const {
    const double time_range =
        bounds.maximum_time - bounds.minimum_time;

    const double x_range =
        bounds.maximum_x - bounds.minimum_x;

    const double normalized_time =
        (point.x() - bounds.minimum_time) /
        time_range;

    const double normalized_x =
        (point.y() - bounds.minimum_x) /
        x_range;

    return QPointF{
        bounds.rect.left() +
            normalized_time * bounds.rect.width(),

        bounds.rect.bottom() -
            normalized_x * bounds.rect.height()
    };
}

std::optional<std::size_t>
TrajectoryPlotWidget::nearest_point_index_(
    const QPointF& mouse_position,
    const PlotBounds& bounds
) const {
    if (
        points_.empty() ||
        !bounds.rect.contains(mouse_position)
    ) {
        return std::nullopt;
    }

    std::optional<std::size_t> nearest_index;
    double minimum_distance_squared =
        hover_distance_pixels *
        hover_distance_pixels;

    for (
        std::size_t index = 0;
        index < points_.size();
        ++index
    ) {
        const QPointF screen_point =
            map_to_plot_(points_[index], bounds);

        const double dx =
            screen_point.x() - mouse_position.x();

        const double dy =
            screen_point.y() - mouse_position.y();

        const double distance_squared =
            dx * dx + dy * dy;

        if (distance_squared <= minimum_distance_squared) {
            minimum_distance_squared = distance_squared;
            nearest_index = index;
        }
    }

    return nearest_index;
}

bool TrajectoryPlotWidget::calculate_trend_(
    std::size_t start_index
) {
    constexpr std::size_t minimum_trend_points = 3;

    if (
        start_index >= points_.size() ||
        points_.size() - start_index <
            minimum_trend_points
    ) {
        emit trend_selection_failed(
            "Choose a point with at least two "
            "following data points."
        );

        return false;
    }

    long double sum_time = 0.0L;
    long double sum_x = 0.0L;
    long double sum_time_squared = 0.0L;
    long double sum_time_x = 0.0L;

    const std::size_t count =
        points_.size() - start_index;

    for (
        std::size_t index = start_index;
        index < points_.size();
        ++index
    ) {
        const long double time =
            static_cast<long double>(
                points_[index].x()
            );

        const long double mean_x =
            static_cast<long double>(
                points_[index].y()
            );

        sum_time += time;
        sum_x += mean_x;
        sum_time_squared += time * time;
        sum_time_x += time * mean_x;
    }

    const long double denominator =
        static_cast<long double>(count) *
        sum_time_squared -
        sum_time * sum_time;

    if (
        !std::isfinite(
            static_cast<double>(denominator)
        ) ||
        std::abs(denominator) < 1.0e-18L
    ) {
        emit trend_selection_failed(
            "Trend cannot be calculated: "
            "selected time values are degenerate."
        );

        return false;
    }

    const long double slope =
        (
            static_cast<long double>(count) *
            sum_time_x -
            sum_time * sum_x
        ) / denominator;

    const long double intercept =
        (
            sum_x -
            slope * sum_time
        ) / static_cast<long double>(count);

    const long double mean_x =
        sum_x / static_cast<long double>(count);

    long double residual_sum = 0.0L;
    long double total_sum = 0.0L;

    for (
        std::size_t index = start_index;
        index < points_.size();
        ++index
    ) {
        const long double time =
            static_cast<long double>(
                points_[index].x()
            );

        const long double value =
            static_cast<long double>(
                points_[index].y()
            );

        const long double fitted =
            intercept + slope * time;

        const long double residual =
            value - fitted;

        const long double deviation =
            value - mean_x;

        residual_sum += residual * residual;
        total_sum += deviation * deviation;
    }

    double r_squared = 1.0;

    if (total_sum > 1.0e-18L) {
        r_squared = static_cast<double>(
            1.0L - residual_sum / total_sum
        );
    }

    trend_ = LinearTrend{
        .valid = true,
        .start_index = start_index,
        .start_time = points_[start_index].x(),
        .intercept = static_cast<double>(intercept),
        .slope = static_cast<double>(slope),
        .r_squared = r_squared
    };

    return true;
}

void TrajectoryPlotWidget::paintEvent(
    QPaintEvent* event
) {
    Q_UNUSED(event);

    QPainter painter{this};

    painter.setRenderHint(
        QPainter::Antialiasing,
        true
    );

    painter.fillRect(
        rect(),
        palette().base()
    );

    const PlotBounds bounds = plot_bounds_();
    const QRectF& plot_rect = bounds.rect;

    const QColor text_color =
        palette().color(QPalette::Text);

    const QColor grid_color =
        palette().color(QPalette::Midlight);

    painter.setPen(QPen{grid_color, 1.0});
    painter.drawRect(plot_rect);

    painter.setPen(text_color);

    painter.drawText(
        QRectF{
            plot_rect.left(),
            2.0,
            plot_rect.width(),
            static_cast<qreal>(top_margin - 4)
        },
        Qt::AlignCenter,
        "Mean position trajectory"
    );

    if (points_.empty()) {
        painter.setPen(
            QColor{"#B8C7D9"}
        );

        painter.drawText(
            plot_rect,
            Qt::AlignCenter,
            "No live data yet.\n"
            "Select Interactive mode and run a simulation."
        );

        painter.setPen(text_color);

        painter.drawText(
            QRectF{
                plot_rect.left(),
                plot_rect.bottom() + 14.0,
                plot_rect.width(),
                24.0
            },
            Qt::AlignCenter,
            "Time"
        );

        painter.save();
        painter.translate(
            18.0,
            plot_rect.center().y()
        );
        painter.rotate(-90.0);

        painter.drawText(
            QRectF{
                -plot_rect.height() / 2.0,
                -12.0,
                plot_rect.height(),
                24.0
            },
            Qt::AlignCenter,
            "<x>"
        );

        painter.restore();

        return;
    }

    const double time_range =
        bounds.maximum_time - bounds.minimum_time;

    const double x_range =
        bounds.maximum_x - bounds.minimum_x;

    QFontMetrics metrics{font()};

    for (int index = 0; index <= tick_count; ++index) {
        const double fraction =
            static_cast<double>(index) /
            static_cast<double>(tick_count);

        const qreal x_position =
            plot_rect.left() +
            static_cast<qreal>(
                fraction * plot_rect.width()
            );

        const qreal y_position =
            plot_rect.bottom() -
            static_cast<qreal>(
                fraction * plot_rect.height()
            );

        painter.setPen(QPen{grid_color, 1.0});

        painter.drawLine(
            QPointF{x_position, plot_rect.top()},
            QPointF{x_position, plot_rect.bottom()}
        );

        painter.drawLine(
            QPointF{plot_rect.left(), y_position},
            QPointF{plot_rect.right(), y_position}
        );

        painter.setPen(text_color);

        const QString time_text = format_value_(
            bounds.minimum_time +
            fraction * time_range
        );

        painter.drawText(
            QPointF{
                x_position -
                    static_cast<qreal>(
                        metrics.horizontalAdvance(time_text)
                    ) / 2.0,

                plot_rect.bottom() + 22.0
            },
            time_text
        );

        const QString x_text = format_value_(
            bounds.minimum_x +
            fraction * x_range
        );

        painter.drawText(
            QPointF{
                plot_rect.left() -
                    static_cast<qreal>(
                        metrics.horizontalAdvance(x_text)
                    ) - 8.0,

                y_position +
                    static_cast<qreal>(
                        metrics.ascent()
                    ) / 2.0
            },
            x_text
        );
    }

    painter.drawText(
        QRectF{
            plot_rect.left(),
            plot_rect.bottom() + 30.0,
            plot_rect.width(),
            20.0
        },
        Qt::AlignCenter,
        "Time"
    );

    painter.save();

    painter.translate(
        18.0,
        plot_rect.center().y()
    );

    painter.rotate(-90.0);

    painter.drawText(
        QRectF{
            -plot_rect.height() / 2.0,
            -12.0,
            plot_rect.height(),
            24.0
        },
        Qt::AlignCenter,
        "<x>"
    );

    painter.restore();

    QPainterPath trajectory_path;

    trajectory_path.moveTo(
        map_to_plot_(points_.front(), bounds)
    );

    for (
        std::size_t index = 1;
        index < points_.size();
        ++index
    ) {
        trajectory_path.lineTo(
            map_to_plot_(points_[index], bounds)
        );
    }

    painter.setPen(
        QPen{
            QColor{35, 120, 220},
            2.0,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin
        }
    );

    painter.setBrush(Qt::NoBrush);
    painter.drawPath(trajectory_path);

    if (trend_.valid) {
        const QPointF start_data_point =
            points_[trend_.start_index];

        const QPointF trend_start{
            start_data_point.x(),
            trend_.intercept +
                trend_.slope * start_data_point.x()
        };

        const QPointF trend_end{
            points_.back().x(),
            trend_.intercept +
                trend_.slope * points_.back().x()
        };

        painter.setPen(
            QPen{
                QColor{"#F2B134"},
                2.0,
                Qt::DashLine,
                Qt::RoundCap,
                Qt::RoundJoin
            }
        );

        painter.drawLine(
            map_to_plot_(trend_start, bounds),
            map_to_plot_(trend_end, bounds)
        );
    }

    if (hovered_index_) {
        const QPointF hovered_data_point =
            points_[*hovered_index_];

        const QPointF hovered_screen_point =
            map_to_plot_(hovered_data_point, bounds);

        painter.setPen(
            QPen{
                QColor{"#D6E4F0"},
                1.0,
                Qt::DashLine
            }
        );

        painter.drawLine(
            QPointF{
                hovered_screen_point.x(),
                plot_rect.top()
            },
            QPointF{
                hovered_screen_point.x(),
                plot_rect.bottom()
            }
        );

        painter.setBrush(
            QColor{"#D6E4F0"}
        );

        painter.setPen(
            QPen{
                QColor{"#1E293B"},
                1.5
            }
        );

        painter.drawEllipse(
            hovered_screen_point,
            4.5,
            4.5
        );

        const QString hover_text = QString{
            "#%1\n"
            "t = %2\n"
            "<x> = %3"
        }
            .arg(
                static_cast<qulonglong>(
                    *hovered_index_ + 1
                )
            )
            .arg(
                format_value_(
                    hovered_data_point.x()
                )
            )
            .arg(
                format_value_(
                    hovered_data_point.y()
                )
            );

        const QRect tooltip_rect = metrics.boundingRect(
            QRect{
                0,
                0,
                220,
                100
            },
            Qt::AlignLeft | Qt::TextWordWrap,
            hover_text
        ).adjusted(-8, -6, 8, 6);

        qreal tooltip_x =
            hovered_screen_point.x() + 14.0;

        qreal tooltip_y =
            hovered_screen_point.y() - 14.0;

        if (
            tooltip_x + tooltip_rect.width() >
            plot_rect.right()
        ) {
            tooltip_x =
                hovered_screen_point.x() -
                tooltip_rect.width() - 14.0;
        }

        if (tooltip_y < plot_rect.top()) {
            tooltip_y = plot_rect.top() + 4.0;
        }

        if (
            tooltip_y + tooltip_rect.height() >
            plot_rect.bottom()
        ) {
            tooltip_y =
                plot_rect.bottom() -
                tooltip_rect.height() - 4.0;
        }

        const QRectF tooltip_draw_rect{
            tooltip_x,
            tooltip_y,
            static_cast<qreal>(tooltip_rect.width()),
            static_cast<qreal>(tooltip_rect.height())
        };

        painter.setPen(
            QPen{
                QColor{"#64748B"},
                1.0
            }
        );

        painter.setBrush(
            QColor{"#1E293B"}
        );

        painter.drawRoundedRect(
            tooltip_draw_rect,
            4.0,
            4.0
        );

        painter.setPen(
            QColor{"#E2E8F0"}
        );

        painter.drawText(
            tooltip_draw_rect.adjusted(
                8.0,
                6.0,
                -8.0,
                -6.0
            ),
            Qt::AlignLeft | Qt::TextWordWrap,
            hover_text
        );
    }

    const QPointF last_point =
        map_to_plot_(points_.back(), bounds);

    painter.setBrush(
        QColor{35, 120, 220}
    );

    painter.setPen(Qt::NoPen);

    painter.drawEllipse(
        last_point,
        3.5,
        3.5
    );

    if (selecting_trend_start_) {
        const QString instruction =
            "Click a data point to select trend start";

        const QRect instruction_rect =
            metrics.boundingRect(
                QRect{0, 0, 320, 32},
                Qt::AlignCenter,
                instruction
            ).adjusted(-10, -6, 10, 6);

        const QRectF instruction_draw_rect{
            plot_rect.center().x() -
                static_cast<qreal>(
                    instruction_rect.width()
                ) / 2.0,

            plot_rect.top() + 8.0,

            static_cast<qreal>(
                instruction_rect.width()
            ),

            static_cast<qreal>(
                instruction_rect.height()
            )
        };

        painter.setPen(
            QPen{
                QColor{"#B45309"},
                1.0
            }
        );

        painter.setBrush(
            QColor{"#FEF3C7"}
        );

        painter.drawRoundedRect(
            instruction_draw_rect,
            4.0,
            4.0
        );

        painter.setPen(
            QColor{"#78350F"}
        );

        painter.drawText(
            instruction_draw_rect,
            Qt::AlignCenter,
            instruction
        );
    }
}

void TrajectoryPlotWidget::mouseMoveEvent(
    QMouseEvent* event
) {
    if (points_.empty()) {
        return;
    }

    const std::optional<std::size_t> previous_index =
        hovered_index_;

    hovered_index_ = nearest_point_index_(
        event->position(),
        plot_bounds_()
    );

    if (hovered_index_ != previous_index) {
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void TrajectoryPlotWidget::leaveEvent(
    QEvent* event
) {
    if (hovered_index_) {
        hovered_index_.reset();
        update();
    }

    QWidget::leaveEvent(event);
}

void TrajectoryPlotWidget::mousePressEvent(
    QMouseEvent* event
) {
    if (
        event->button() != Qt::LeftButton ||
        !selecting_trend_start_
    ) {
        QWidget::mousePressEvent(event);
        return;
    }

    const std::optional<std::size_t> selected_index =
        nearest_point_index_(
            event->position(),
            plot_bounds_()
        );

    if (!selected_index) {
        emit trend_selection_failed(
            "Click closer to a data point."
        );

        return;
    }

    if (calculate_trend_(*selected_index)) {
        selecting_trend_start_ = false;
        unsetCursor();

        emit trend_changed(
            trend_.start_time,
            trend_.intercept,
            trend_.slope,
            trend_.r_squared
        );

        update();
    }
}

QString TrajectoryPlotWidget::format_value_(
    double value
) {
    if (!std::isfinite(value)) {
        return "?";
    }

    if (std::abs(value) < 1.0e-12) {
        value = 0.0;
    }

    const double magnitude = std::abs(value);

    if (
        magnitude >= 10'000.0 ||
        (magnitude > 0.0 && magnitude < 0.001)
    ) {
        return QString::number(value, 'e', 2);
    }

    int decimals = 0;

    if (magnitude < 0.01) {
        decimals = 4;
    } else if (magnitude < 0.1) {
        decimals = 3;
    } else if (magnitude < 10.0) {
        decimals = 2;
    } else if (magnitude < 100.0) {
        decimals = 1;
    }

    QString text = QString::number(
        value,
        'f',
        decimals
    );

    if (text.contains('.')) {
        while (text.endsWith('0')) {
            text.chop(1);
        }

        if (text.endsWith('.')) {
            text.chop(1);
        }
    }

    return text;
}