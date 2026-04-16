/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Placeholder performance dashboard window.
 */

#include "qt_perfdash_window.hpp"

#include <QCloseEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>

extern "C" {
#include <86box/perf_dashboard.h>
}

namespace {

constexpr auto kSettingsKey = "PerformanceDashboard/geometry";

auto makeMetricCard(const QString &title, const QString &value, const QString &detail) -> QFrame *
{
    auto *card = new QFrame;
    card->setProperty("perfCard", true);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);

    auto *title_label = new QLabel(title);
    title_label->setProperty("metricTitle", true);
    layout->addWidget(title_label);

    auto *value_label = new QLabel(value);
    value_label->setProperty("metricValue", true);
    layout->addWidget(value_label);

    auto *detail_label = new QLabel(detail);
    detail_label->setProperty("metricMeta", true);
    detail_label->setWordWrap(true);
    layout->addWidget(detail_label);

    layout->addStretch(1);
    return card;
}

auto makeSection(const QString &title, const QStringList &lines) -> QGroupBox *
{
    auto *section = new QGroupBox(title);
    section->setProperty("perfPanel", true);

    auto *layout = new QVBoxLayout(section);
    layout->setSpacing(8);

    for (const auto &line : lines) {
        auto *label = new QLabel(line);
        label->setProperty("metricMeta", true);
        label->setWordWrap(true);
        layout->addWidget(label);
    }

    layout->addStretch(1);
    return section;
}

}

PerfDashboardWindow::PerfDashboardWindow(QWidget *parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setModal(false);
    setWindowFlag(Qt::Window, true);
    setMinimumSize(980, 640);
    resize(1100, 720);
    setWindowTitle(tr("86Box Performance Dashboard"));
    setWindowIcon(QIcon(":/themes/icons/benchmark.svg"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto *header_layout = new QHBoxLayout;
    header_layout->setSpacing(12);

    auto *title_layout = new QVBoxLayout;
    auto *title_label = new QLabel(tr("86Box Performance Dashboard"));
    title_label->setProperty("sectionTitle", true);
    title_layout->addWidget(title_label);

    status_value_label = new QLabel;
    status_value_label->setProperty("metricValue", true);
    title_layout->addWidget(status_value_label);

    status_detail_label = new QLabel;
    status_detail_label->setProperty("metricMeta", true);
    title_layout->addWidget(status_detail_label);

    header_layout->addLayout(title_layout, 1);

    auto *controls_layout = new QHBoxLayout;
    controls_layout->setSpacing(8);

    start_button = new QPushButton(QIcon(":/themes/icons/play.svg"), tr("Start"));
    stop_button = new QPushButton(QIcon(":/themes/icons/stop.svg"), tr("Stop"));
    benchmark_button = new QPushButton(QIcon(":/themes/icons/benchmark.svg"), tr("Benchmark"));
    reset_button = new QPushButton(QIcon(":/themes/icons/reset.svg"), tr("Reset"));

    benchmark_button->setEnabled(false);
    benchmark_button->setToolTip(tr("Benchmark mode will be enabled in F7."));

    controls_layout->addWidget(start_button);
    controls_layout->addWidget(stop_button);
    controls_layout->addWidget(benchmark_button);
    controls_layout->addWidget(reset_button);
    header_layout->addLayout(controls_layout);

    root->addLayout(header_layout);

    auto *cards_layout = new QHBoxLayout;
    cards_layout->setSpacing(12);
    cards_layout->addWidget(makeMetricCard(tr("Speed"), tr("100%"), tr("Placeholder cadence for F3 preview.")));
    cards_layout->addWidget(makeMetricCard(tr("Frame Time"), tr("8.2 ms"), tr("Real timing deltas land in F5.")));
    cards_layout->addWidget(makeMetricCard(tr("Host CPU"), tr("32%"), tr("Will reflect host-side load later.")));
    cards_layout->addWidget(makeMetricCard(tr("FPS"), tr("60"), tr("Rendering counters will replace this placeholder.")));
    cards_layout->addWidget(makeMetricCard(tr("Cycles/s"), tr("33.0 M"), tr("CPU throughput binding arrives in F5/F6.")));
    root->addLayout(cards_layout);

    auto *sections_layout = new QGridLayout;
    sections_layout->setHorizontalSpacing(12);
    sections_layout->setVerticalSpacing(12);
    sections_layout->addWidget(makeSection(
        tr("CPU Distribution"),
        {
            tr("Placeholder panel for interpreter vs dynarec split."),
            tr("Pie and bar widgets arrive in F4."),
            tr("Live snapshot integration arrives in F5.")
        }),
        0, 0);
    sections_layout->addWidget(makeSection(
        tr("Video Pipeline"),
        {
            tr("Reserved for blit, pageflip and renderer domains."),
            tr("This section already inherits the Black Modern theme."),
            tr("Charts remain intentionally static in F3.")
        }),
        0, 1);
    sections_layout->addWidget(makeSection(
        tr("Audio, Timing and I/O"),
        {
            tr("Reserved for sound, IRQ, DMA and storage counters."),
            tr("Call-site instrumentation is planned for F6."),
            tr("The layout is ready for placeholder-to-live migration.")
        }),
        1, 0);
    sections_layout->addWidget(makeSection(
        tr("Session Summary"),
        {
            tr("Dashboard opens as a non-modal window."),
            tr("Closing it stops and resets measurement state."),
            tr("Geometry is persisted with QSettings.")
        }),
        1, 1);
    root->addLayout(sections_layout, 1);

    connect(start_button, &QPushButton::clicked, this, &PerfDashboardWindow::startCapture);
    connect(stop_button, &QPushButton::clicked, this, &PerfDashboardWindow::stopCapture);
    connect(reset_button, &QPushButton::clicked, this, &PerfDashboardWindow::resetCapture);

    perfdash_stop();
    perfdash_reset();
    restoreWindowSettings();
    updateControlState();
}

void
PerfDashboardWindow::closeEvent(QCloseEvent *event)
{
    perfdash_stop();
    perfdash_reset();
    running = false;
    saveWindowSettings();
    QDialog::closeEvent(event);
}

void
PerfDashboardWindow::startCapture()
{
    perfdash_start();
    running = true;
    updateControlState();
}

void
PerfDashboardWindow::stopCapture()
{
    perfdash_stop();
    running = false;
    updateControlState();
}

void
PerfDashboardWindow::resetCapture()
{
    perfdash_reset();
    updateControlState();
}

void
PerfDashboardWindow::restoreWindowSettings()
{
    QSettings settings(QStringLiteral("86Box"), QStringLiteral("86Box"));
    const auto geometry = settings.value(QStringLiteral(kSettingsKey)).toByteArray();

    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

void
PerfDashboardWindow::saveWindowSettings() const
{
    QSettings settings(QStringLiteral("86Box"), QStringLiteral("86Box"));
    settings.setValue(QStringLiteral(kSettingsKey), saveGeometry());
}

void
PerfDashboardWindow::updateControlState()
{
    start_button->setEnabled(!running);
    stop_button->setEnabled(running);
    reset_button->setEnabled(true);

    if (running) {
        status_value_label->setText(tr("Running"));
        status_detail_label->setText(tr("Instrumentation is armed; placeholder cards stay static until F5."));
        return;
    }

    status_value_label->setText(tr("Stopped"));
    status_detail_label->setText(tr("Preview layout ready. Live metrics and charts arrive in the next phases."));
}
