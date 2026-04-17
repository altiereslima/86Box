#include "qt_perfdash_widgets.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QVBoxLayout>

#include <algorithm>

extern "C" {
#include <86box/perf_dashboard.h>
}

#include "qt_perfdash_theme.hpp"

namespace perfdash {

namespace {

double
clamp01(double value)
{
    if (value < 0.0)
        return 0.0;
    if (value > 100.0)
        return 100.0;
    return value;
}

}

SparklineWidget::SparklineWidget(QWidget *parent)
    : QWidget(parent)
    , line_color(theme::accentBlue())
{
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumHeight(52);
}

void
SparklineWidget::setSamples(const QVector<double> &next_samples)
{
    samples = next_samples;
    update();
}

void
SparklineWidget::setLineColor(const QColor &next_color)
{
    line_color = next_color;
    update();
}

void
SparklineWidget::paintEvent(QPaintEvent *event)
{
    PERF_SCOPE_BEGIN(PERF_DOMAIN_QT_UI);
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto area = rect().adjusted(6, 6, -6, -6);
    painter.setPen(QPen(theme::border(), 1.0));
    painter.drawRoundedRect(area, 6.0, 6.0);

    if (samples.size() < 2) {
        painter.setPen(QPen(theme::textSecondary(), 1.0, Qt::DashLine));
        painter.drawLine(area.left() + 4, area.center().y(), area.right() - 4, area.center().y());
        PERF_SCOPE_END(PERF_DOMAIN_QT_UI);
        return;
    }

    double min_value = samples.front();
    double max_value = samples.front();
    for (const auto &sample : samples) {
        min_value = std::min(min_value, sample);
        max_value = std::max(max_value, sample);
    }

    if (qFuzzyCompare(min_value, max_value))
        max_value = min_value + 1.0;

    QPainterPath path;
    for (int i = 0; i < samples.size(); ++i) {
        const double x = area.left() + (double) i * area.width() / double(samples.size() - 1);
        const double normalized = (samples.at(i) - min_value) / (max_value - min_value);
        const double y = area.bottom() - normalized * area.height();
        if (!i)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }

    painter.setPen(QPen(line_color, 2.0));
    painter.drawPath(path);
    PERF_SCOPE_END(PERF_DOMAIN_QT_UI);
}

MetricCard::MetricCard(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("perfCard", true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

    title_label = new QLabel(title);
    title_label->setProperty("metricTitle", true);
    layout->addWidget(title_label);

    value_label = new QLabel;
    value_label->setProperty("metricValue", true);
    layout->addWidget(value_label);

    detail_label = new QLabel;
    detail_label->setProperty("metricMeta", true);
    detail_label->setWordWrap(true);
    layout->addWidget(detail_label);

    sparkline_widget = new SparklineWidget(this);
    layout->addWidget(sparkline_widget);
}

void
MetricCard::setValueText(const QString &text)
{
    value_label->setText(text);
}

void
MetricCard::setDetailText(const QString &text)
{
    detail_label->setText(text);
}

void
MetricCard::setSparklineSamples(const QVector<double> &samples)
{
    sparkline_widget->setSamples(samples);
}

void
MetricCard::setAccentColor(const QColor &color)
{
    sparkline_widget->setLineColor(color);
}

BarRow::BarRow(QWidget *parent)
    : QWidget(parent)
    , fill_color(theme::accentBlue())
{
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumHeight(46);
}

void
BarRow::setLabel(const QString &text)
{
    label = text;
    update();
}

void
BarRow::setValue(double next_value)
{
    value = clamp01(next_value);
    update();
}

void
BarRow::setColor(const QColor &next_color)
{
    fill_color = next_color;
    update();
}

void
BarRow::setSuffix(const QString &text)
{
    suffix = text;
    update();
}

QSize
BarRow::minimumSizeHint() const
{
    return { 180, 46 };
}

QSize
BarRow::sizeHint() const
{
    return { 240, 46 };
}

void
BarRow::paintEvent(QPaintEvent *event)
{
    PERF_SCOPE_BEGIN(PERF_DOMAIN_QT_UI);
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto card = rect().adjusted(1, 1, -1, -1);
    painter.setPen(QPen(theme::border(), 1.0));
    painter.setBrush(theme::surface());
    painter.drawRoundedRect(card, 8.0, 8.0);

    const auto content = card.adjusted(12, 8, -12, -10);
    painter.setPen(theme::textPrimary());
    painter.drawText(content.left(), content.top(), content.width(), 16, Qt::AlignLeft | Qt::AlignVCenter, label);

    const auto value_text = suffix.isEmpty() ? QString::number(value, 'f', 0)
                                             : QStringLiteral("%1 %2").arg(QString::number(value, 'f', 0), suffix);
    painter.setPen(theme::textSecondary());
    painter.drawText(content.left(), content.top(), content.width(), 16, Qt::AlignRight | Qt::AlignVCenter, value_text);

    QRectF track_rect(content.left(), content.bottom() - 14, content.width(), 10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::surfaceElevated());
    painter.drawRoundedRect(track_rect, 5.0, 5.0);

    if (value > 0.0) {
        QRectF fill_rect = track_rect;
        fill_rect.setWidth(track_rect.width() * (value / 100.0));
        painter.setBrush(fill_color);
        painter.drawRoundedRect(fill_rect, 5.0, 5.0);
    }

    PERF_SCOPE_END(PERF_DOMAIN_QT_UI);
}

PieWidget::PieWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
}

void
PieWidget::setSlices(const QVector<PieSlice> &next_slices)
{
    slices = next_slices;
    update();
}

void
PieWidget::setCenterText(const QString &text)
{
    center_text = text;
    update();
}

QSize
PieWidget::minimumSizeHint() const
{
    return { 180, 180 };
}

QSize
PieWidget::sizeHint() const
{
    return { 220, 220 };
}

void
PieWidget::paintEvent(QPaintEvent *event)
{
    PERF_SCOPE_BEGIN(PERF_DOMAIN_QT_UI);
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto outer = rect().adjusted(8, 8, -8, -8);
    painter.setPen(QPen(theme::border(), 1.0));
    painter.setBrush(theme::surface());
    painter.drawRoundedRect(outer, 10.0, 10.0);

    QRectF pie_rect = outer.adjusted(18, 18, -18, -18);
    const qreal side = std::min(pie_rect.width(), pie_rect.height());
    pie_rect.setSize({ side, side });
    pie_rect.moveCenter(outer.center());

    double total = 0.0;
    for (const auto &slice : slices)
        total += std::max(0.0, slice.value);

    if (total <= 0.0) {
        painter.setPen(QPen(theme::textSecondary(), 2.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(pie_rect);
    } else {
        int start_angle = 90 * 16;
        for (const auto &slice : slices) {
            const double value = std::max(0.0, slice.value);
            if (value <= 0.0)
                continue;

            const int span = int(-5760.0 * (value / total));
            painter.setPen(Qt::NoPen);
            painter.setBrush(slice.color);
            painter.drawPie(pie_rect, start_angle, span);
            start_angle += span;
        }
    }

    const QRectF inner = pie_rect.adjusted(pie_rect.width() * 0.24,
                                           pie_rect.height() * 0.24,
                                           -pie_rect.width() * 0.24,
                                           -pie_rect.height() * 0.24);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::surfaceElevated());
    painter.drawEllipse(inner);

    painter.setPen(theme::textPrimary());
    painter.drawText(inner, Qt::AlignCenter | Qt::TextWordWrap, center_text);
    PERF_SCOPE_END(PERF_DOMAIN_QT_UI);
}

}
