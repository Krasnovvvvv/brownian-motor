#include "gui/TrajectoryPlotWidget.h"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int left_margin = 72;
constexpr int right_margin = 24;
constexpr int top_margin = 26;
constexpr int bottom_margin = 52;

constexpr int tick_count = 5;

constexpr int maximum_points = 5'000;

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
}

void TrajectoryPlotWidget::clear_points() {
    points_.clear();
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
    }

    update();
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

    const QRectF plot_rect{
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

    const QColor text_color =
        palette().color(QPalette::Text);

    const QColor grid_color =
        palette().color(QPalette::Midlight);

    painter.setPen(QPen{grid_color, 1.0});
    painter.drawRect(plot_rect);

    painter.setPen(text_color);

    const QString title =
        "Mean position trajectory";

    painter.drawText(
        QRectF{
            plot_rect.left(),
            2.0,
            plot_rect.width(),
            static_cast<qreal>(top_margin - 4)
        },
        Qt::AlignCenter,
        title
    );

    if (points_.empty()) {
        painter.setPen(
            QColor{0x8FA3B8}
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

    const double time_range = expand_range(
        minimum_time,
        maximum_time
    );

    const double x_range = expand_range(
        minimum_x,
        maximum_x
    );

    const double time_padding = 0.03 * time_range;
    const double x_padding = 0.10 * x_range;

    minimum_time = std::max(
        0.0,
        minimum_time - time_padding
    );

    maximum_time += time_padding;

    minimum_x -= x_padding;
    maximum_x += x_padding;

    const double padded_time_range =
        maximum_time - minimum_time;

    const double padded_x_range =
        maximum_x - minimum_x;

    const auto map_to_plot =
        [&](
            const QPointF& point
        ) -> QPointF {
            const double normalized_time =
                (point.x() - minimum_time) /
                padded_time_range;

            const double normalized_x =
                (point.y() - minimum_x) /
                padded_x_range;

            return QPointF{
                plot_rect.left() +
                    normalized_time * plot_rect.width(),

                plot_rect.bottom() -
                    normalized_x * plot_rect.height()
            };
        };

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

        const double time_value =
            minimum_time +
            fraction * padded_time_range;

        const QString time_text =
            format_value_(time_value);

        const int time_width =
            metrics.horizontalAdvance(time_text);

        painter.drawText(
            QPointF{
                x_position -
                    static_cast<qreal>(time_width) / 2.0,
                plot_rect.bottom() + 22.0
            },
            time_text
        );

        const double x_value =
            minimum_x +
            fraction * padded_x_range;

        const QString x_text =
            format_value_(x_value);

        const int x_width =
            metrics.horizontalAdvance(x_text);

        painter.drawText(
            QPointF{
                plot_rect.left() -
                    static_cast<qreal>(x_width) - 8.0,
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

    if (points_.size() == 1) {
        painter.setPen(
            QPen{
                QColor{35, 120, 220},
                7.0,
                Qt::SolidLine,
                Qt::RoundCap
            }
        );

        painter.drawPoint(
            map_to_plot(points_.front())
        );

        return;
    }

    QPainterPath path;

    path.moveTo(
        map_to_plot(points_.front())
    );

    for (
        std::size_t index = 1;
        index < points_.size();
        ++index
    ) {
        path.lineTo(
            map_to_plot(points_[index])
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

    painter.drawPath(path);

    const QPointF last_point =
        map_to_plot(points_.back());

    painter.setBrush(
        QColor{35, 120, 220}
    );

    painter.setPen(Qt::NoPen);

    painter.drawEllipse(
        last_point,
        3.5,
        3.5
    );
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
    } else if (magnitude < 1.0) {
        decimals = 2;
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