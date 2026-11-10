#include "gui/PotentialPreviewDialog.h"

#include "core/ExpressionEvaluator.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    struct ProfilePlotData final {
        std::vector<QPointF> points;
        QString coordinate_label;
        QString potential_label;
        double period{};
        double lower{};
        double upper{};
    };

    [[nodiscard]] ProfilePlotData build_plot_data(
        const PotentialDefinition &definition,
        const PotentialParameterValues &parameter_values
    ) {
        const double period = definition.profile.period;

        if (
            !definition.profile.periodic ||
            !std::isfinite(period) ||
            period <= 0.0
        ) {
            throw std::invalid_argument{
                "A finite positive spatial period is required for the preview."
            };
        }

        const ExpressionEvaluator evaluator{
            definition,
            parameter_values
        };

        ProfilePlotData data;
        data.period = period;
        data.coordinate_label = QString::fromStdString(
            definition.coordinate.symbol
        );
        data.potential_label = QStringLiteral("V(%1)").arg(
            data.coordinate_label
        );

        std::vector<double> edges{0.0};
        if (evaluator.piecewise()) {
            edges.insert(
                edges.end(),
                evaluator.breakpoints().begin(),
                evaluator.breakpoints().end()
            );
        }
        edges.push_back(period);

        constexpr std::size_t base_intervals = 4096;
        constexpr std::size_t minimum_segment_intervals = 8;
        constexpr std::size_t maximum_points = 65536;

        data.points.reserve(
            base_intervals + 12 * edges.size()
        );

        const auto append = [&](double x) {
            if (
                !data.points.empty() &&
                x <= data.points.back().x()
            ) {
                return;
            }

            const double value = evaluator.value(x);

            if (!std::isfinite(value)) {
                throw std::domain_error{
                    "Potential preview encountered a non-finite profile value."
                };
            }

            if (data.points.size() >= maximum_points) {
                throw std::length_error{
                    "Potential preview contains too many sample points."
                };
            }

            data.points.emplace_back(x, value);
        };

        for (std::size_t i = 0; i + 1 < edges.size(); ++i) {
            const double left = edges[i];
            const double right = edges[i + 1];

            if (
                !std::isfinite(left) ||
                !std::isfinite(right) ||
                !(left < right)
            ) {
                throw std::invalid_argument{
                    "Invalid segment boundaries for potential preview."
                };
            }

            const double share =
                    static_cast<double>(base_intervals) *
                    ((right - left) / period);

            const auto intervals = std::max(
                minimum_segment_intervals,
                static_cast<std::size_t>(std::ceil(share))
            );

            append(left);

            const double left_inside =
                    std::nextafter(left, right);
            if (left_inside < right) {
                append(left_inside);
            }

            for (std::size_t j = 1; j < intervals; ++j) {
                append(
                    std::lerp(
                        left,
                        right,
                        static_cast<double>(j) /
                        static_cast<double>(intervals)
                    )
                );
            }

            const double right_inside =
                    std::nextafter(right, left);
            if (right_inside > left) {
                append(right_inside);
            }

            append(right);
        }

        if (data.points.size() < 2) {
            throw std::domain_error{
                "Not enough representable coordinates to draw the potential."
            };
        }

        const auto extremes = std::minmax_element(
            data.points.begin(),
            data.points.end(),
            [](const QPointF &a, const QPointF &b) {
                return a.y() < b.y();
            }
        );

        const double minimum = extremes.first->y();
        const double maximum = extremes.second->y();
        const double span = maximum - minimum;

        if (!std::isfinite(span)) {
            throw std::domain_error{
                "Potential range is too large to display."
            };
        }

        const double padding = span > 0.0
                ? 0.06 * span
                : 0.05 * std::max(1.0, std::abs(minimum));

        data.lower = minimum - padding;
        data.upper = maximum + padding;

        if (
            !std::isfinite(data.lower) ||
            !std::isfinite(data.upper) ||
            !std::isfinite(data.upper - data.lower) ||
            !(data.lower < data.upper)
        ) {
            throw std::domain_error{
                "Cannot construct a finite vertical axis for this potential."
            };
        }

        return data;
    }

    [[nodiscard]] QString tick_text(double value) {
        return QLocale::c().toString(
            value == 0.0 ? 0.0 : value,
            'g',
            6
        );
    }

    class PotentialProfilePlot final : public QWidget {
    public:
        explicit PotentialProfilePlot(
            ProfilePlotData data,
            QWidget *parent
        )
            : QWidget{parent}
              , data_{std::move(data)} {
            setMinimumSize(600, 360);
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            Q_UNUSED(event);

            QPainter painter{this};
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.fillRect(rect(), Qt::white);

            QFont plot_font = painter.font();
            plot_font.setPointSizeF(11.0);
            painter.setFont(plot_font);

            constexpr int divisions = 6;
            const QFontMetrics metrics{painter.font()};
            const int text_height = metrics.height();

            QFont axis_label_font = painter.font();
            axis_label_font.setPointSizeF(12.0);
            axis_label_font.setBold(true);

            const QFontMetrics axis_label_metrics{
                axis_label_font
            };

            const int axis_label_height =
                    axis_label_metrics.height();

            int y_label_width = 0;
            for (int i = 0; i <= divisions; ++i) {
                const double value = std::lerp(
                    data_.lower,
                    data_.upper,
                    static_cast<double>(i) / divisions
                );
                y_label_width = std::max(
                    y_label_width,
                    metrics.horizontalAdvance(tick_text(value))
                );
            }

            const double left_margin =
                    y_label_width + axis_label_height + 32.0;
            const double right_margin = std::max(
                24.0,
                0.5 * metrics.horizontalAdvance(
                    tick_text(data_.period)
                ) + 8.0
            );
            const double bottom_margin =
                    text_height + axis_label_height + 26.0;

            const QRectF plot{
                left_margin,
                18.0,
                width() - left_margin - right_margin,
                height() - 18.0 - bottom_margin
            };

            if (plot.width() < 20.0 || plot.height() < 20.0) {
                return;
            }

            QPen grid_pen{QColor{222, 227, 233}};
            grid_pen.setCosmetic(true);
            const QColor text_color{42, 49, 58};

            for (int i = 0; i <= divisions; ++i) {
                const double fraction =
                        static_cast<double>(i) / divisions;
                const double x =
                        plot.left() + fraction * plot.width();
                const double y =
                        plot.bottom() - fraction * plot.height();

                painter.setPen(grid_pen);
                painter.drawLine(
                    QPointF{x, plot.top()},
                    QPointF{x, plot.bottom()}
                );
                painter.drawLine(
                    QPointF{plot.left(), y},
                    QPointF{plot.right(), y}
                );

                const QString x_tick = tick_text(
                    data_.period * fraction
                );
                const QString y_tick = tick_text(
                    std::lerp(data_.lower, data_.upper, fraction)
                );

                painter.setPen(text_color);
                painter.drawText(
                    QRectF{
                        x - 80.0,
                        plot.bottom() + 7.0,
                        160.0,
                        static_cast<double>(text_height + 3)
                    },
                    Qt::AlignHCenter | Qt::AlignTop,
                    x_tick
                );
                painter.drawText(
                QRectF{
                    static_cast<double>(axis_label_height + 14),
                    y - 0.5 * text_height - 2.0,
                    plot.left() - axis_label_height - 24.0,
                    static_cast<double>(text_height + 4)
                },
                    Qt::AlignRight | Qt::AlignVCenter,
                    y_tick
                );
            }

            QPainterPath curve;
            bool first = true;
            for (const QPointF &point : data_.points) {
                const QPointF pixel{
                    plot.left() +
                    (point.x() / data_.period) * plot.width(),
                    plot.bottom() -
                    ((point.y() - data_.lower) /
                     (data_.upper - data_.lower)) * plot.height()
                };
                if (first) {
                    curve.moveTo(pixel);
                    first = false;
                } else {
                    curve.lineTo(pixel);
                }
            }

            painter.save();
            painter.setClipRect(plot);
            QPen curve_pen{QColor{38, 105, 190}};
            curve_pen.setWidthF(3.5);
            curve_pen.setCosmetic(true);
            painter.setPen(curve_pen);
            painter.drawPath(curve);
            painter.restore();

            painter.setPen(QPen{QColor{100, 109, 120}, 1.0});
            painter.drawRect(plot);

            painter.save();

            painter.setPen(text_color);
            painter.setFont(axis_label_font);

            // Название горизонтальной оси.
            painter.drawText(
                QRectF{
                    plot.left(),
                    plot.bottom() + text_height + 17.0,
                    plot.width(),
                    static_cast<double>(axis_label_height + 4)
                },
                Qt::AlignHCenter | Qt::AlignTop,
                data_.coordinate_label
            );

            // Название вертикальной оси.
            painter.translate(
                0.5 * axis_label_height + 5.0,
                plot.center().y()
            );

            painter.rotate(-90.0);

            painter.drawText(
                QRectF{
                    -0.5 * plot.height(),
                    -0.5 * axis_label_height - 2.0,
                    plot.height(),
                    static_cast<double>(axis_label_height + 4)
                },
                Qt::AlignCenter,
                data_.potential_label
            );

            painter.restore();
        }

    private:
        ProfilePlotData data_;
    };
} // namespace

PotentialPreviewDialog::PotentialPreviewDialog(
    const PotentialDefinition &definition,
    const PotentialParameterValues &parameter_values,
    QWidget *parent
)
    : QDialog{parent} {
    ProfilePlotData data = build_plot_data(
        definition,
        parameter_values
    );

    setWindowTitle(
        QStringLiteral("Potential profile - %1").arg(
            QString::fromStdString(definition.name)
        )
    );
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setSizeGripEnabled(true);
    resize(840, 520);

    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addWidget(
        new PotentialProfilePlot{
            std::move(data),
            this
        }
    );
}
