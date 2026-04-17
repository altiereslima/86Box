/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Performance dashboard window.
 */

#include "qt_perfdash_window.hpp"

#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "qt_mainwindow.hpp"
#include "qt_perfdash_benchmark.hpp"
#include "qt_perfdash_theme.hpp"
#include "qt_perfdash_widgets.hpp"

namespace {

constexpr auto kSettingsKey = "PerformanceDashboard/geometry";

QString
format_rate(double value)
{
    double scaled = value;
    QString suffix;

    if (scaled >= 1000000000.0) {
        scaled /= 1000000000.0;
        suffix = QStringLiteral(" G/s");
    } else if (scaled >= 1000000.0) {
        scaled /= 1000000.0;
        suffix = QStringLiteral(" M/s");
    } else if (scaled >= 1000.0) {
        scaled /= 1000.0;
        suffix = QStringLiteral(" K/s");
    } else {
        suffix = QStringLiteral(" /s");
    }

    return QStringLiteral("%1%2")
        .arg(QString::number(scaled, 'f', scaled >= 100.0 ? 0 : (scaled >= 10.0 ? 1 : 2)),
             suffix);
}

QString
format_refresh_hz(int interval_ms)
{
    if (interval_ms <= 0)
        return QObject::tr("n/a");

    const double hz = 1000.0 / (double) interval_ms;
    return QObject::tr("%1 Hz").arg(QString::number(hz, 'f', hz >= 10.0 ? 0 : 1));
}

QFrame *
make_panel(const QString &title, QVBoxLayout **content_layout)
{
    auto *panel = new QFrame;
    panel->setProperty("perfPanel", true);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *title_label = new QLabel(title);
    title_label->setProperty("sectionTitle", true);
    layout->addWidget(title_label);

    *content_layout = layout;
    return panel;
}

void
add_summary_row(QGridLayout *layout, int row, const QString &label_text, QLabel **value_label)
{
    auto *label = new QLabel(label_text);
    label->setProperty("metricTitle", true);
    layout->addWidget(label, row, 0);

    *value_label = new QLabel;
    (*value_label)->setProperty("metricMeta", true);
    (*value_label)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(*value_label, row, 1);
}

QVector<perfdash::BarRow *>
create_rows(QVBoxLayout *layout, const QVector<perfdash::SectionEntry> &entries)
{
    QVector<perfdash::BarRow *> rows;
    rows.reserve(entries.size());

    for (const auto &entry : entries) {
        auto *row = new perfdash::BarRow;
        row->setLabel(entry.label);
        row->setColor(entry.color);
        row->setSuffix(QObject::tr("%"));
        layout->addWidget(row);
        rows.append(row);
    }

    return rows;
}

double
sum_entries(const QVector<perfdash::SectionEntry> &entries)
{
    double total = 0.0;
    for (const auto &entry : entries)
        total += entry.percent;
    return total;
}

}

PerfDashboardWindow::PerfDashboardWindow(QWidget *parent)
    : QDialog(parent)
    , model(new perfdash::PerfDashModel(this))
    , benchmark(new perfdash::PerfBenchmark(this))
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setModal(false);
    setWindowFlag(Qt::Window, true);
    setMinimumSize(980, 640);
    resize(1180, 760);
    setWindowTitle(tr("86Box Performance Dashboard"));
    setWindowIcon(QIcon(":/themes/icons/benchmark.svg"));

    buildUi();

    connect(start_button, &QPushButton::clicked, this, &PerfDashboardWindow::startCapture);
    connect(stop_button, &QPushButton::clicked, this, &PerfDashboardWindow::stopCapture);
    connect(reset_button, &QPushButton::clicked, this, &PerfDashboardWindow::resetCapture);
    connect(benchmark_button, &QPushButton::clicked, this, &PerfDashboardWindow::toggleBenchmark);
    connect(refresh_combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        model->setRefreshIntervalMs(selectedRefreshIntervalMs());
        updateControlState();
        applyViewData();
    });

    connect(model, &perfdash::PerfDashModel::dataUpdated, this, &PerfDashboardWindow::handleModelUpdated);
    connect(benchmark, &perfdash::PerfBenchmark::stateChanged, this, &PerfDashboardWindow::updateControlState);
    connect(benchmark, &perfdash::PerfBenchmark::finished, this, &PerfDashboardWindow::handleBenchmarkFinished);

    model->setRefreshIntervalMs(selectedRefreshIntervalMs());
    restoreWindowSettings();
    applyViewData();
    updateControlState();
}

void
PerfDashboardWindow::closeEvent(QCloseEvent *event)
{
    benchmark->cancel();
    lockBenchmarkControls(false);
    model->stop();
    model->reset();
    saveWindowSettings();
    QDialog::closeEvent(event);
}

void
PerfDashboardWindow::startCapture()
{
    model->setRefreshIntervalMs(selectedRefreshIntervalMs());
    model->start();
}

void
PerfDashboardWindow::stopCapture()
{
    if (benchmark->isActive())
        return;

    model->stop();
}

void
PerfDashboardWindow::resetCapture()
{
    if (benchmark->isActive())
        return;

    model->reset();
    applyViewData();
    updateControlState();
}

void
PerfDashboardWindow::toggleBenchmark()
{
    if (benchmark->isActive()) {
        benchmark->stop();
        return;
    }

    const bool was_running = model->isRunning();

    model->setRefreshIntervalMs(selectedRefreshIntervalMs());
    model->reset();
    benchmark->start(selectedBenchmarkDurationSeconds());
    lockBenchmarkControls(true);

    if (!was_running)
        model->start();

    updateControlState();
}

void
PerfDashboardWindow::handleModelUpdated()
{
    if (benchmark->isActive())
        benchmark->consumeSample(model->viewData());

    applyViewData();
    updateControlState();
}

void
PerfDashboardWindow::handleBenchmarkFinished()
{
    lockBenchmarkControls(false);
    applyViewData();
    updateControlState();
    benchmark->showResultDialog(this);
}

void
PerfDashboardWindow::buildUi()
{
    const auto &view = model->viewData();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto *header_layout = new QHBoxLayout;
    header_layout->setSpacing(16);

    auto *title_layout = new QVBoxLayout;
    title_layout->setSpacing(6);

    auto *title_label = new QLabel(tr("86Box Performance Dashboard"));
    title_label->setProperty("sectionTitle", true);
    title_layout->addWidget(title_label);

    status_value_label = new QLabel;
    status_value_label->setProperty("metricValue", true);
    title_layout->addWidget(status_value_label);

    status_detail_label = new QLabel;
    status_detail_label->setProperty("metricMeta", true);
    status_detail_label->setWordWrap(true);
    title_layout->addWidget(status_detail_label);
    title_layout->addStretch(1);

    header_layout->addLayout(title_layout, 1);

    auto *controls_panel = new QFrame;
    controls_panel->setProperty("perfPanel", true);
    auto *controls_layout = new QGridLayout(controls_panel);
    controls_layout->setContentsMargins(14, 12, 14, 12);
    controls_layout->setHorizontalSpacing(10);
    controls_layout->setVerticalSpacing(8);

    auto *refresh_label = new QLabel(tr("Refresh"));
    refresh_label->setProperty("metricTitle", true);
    controls_layout->addWidget(refresh_label, 0, 0);

    refresh_combo = new QComboBox;
    refresh_combo->addItem(tr("1 Hz"), 1000);
    refresh_combo->addItem(tr("2 Hz"), 500);
    refresh_combo->addItem(tr("5 Hz"), 200);
    refresh_combo->addItem(tr("10 Hz"), 100);
    refresh_combo->setCurrentIndex(2);
    controls_layout->addWidget(refresh_combo, 0, 1);

    auto *duration_label = new QLabel(tr("Benchmark"));
    duration_label->setProperty("metricTitle", true);
    controls_layout->addWidget(duration_label, 0, 2);

    duration_combo = new QComboBox;
    duration_combo->addItem(tr("15 s"), 15);
    duration_combo->addItem(tr("30 s"), 30);
    duration_combo->addItem(tr("60 s"), 60);
    duration_combo->addItem(tr("120 s"), 120);
    duration_combo->setCurrentIndex(1);
    controls_layout->addWidget(duration_combo, 0, 3);

    start_button = new QPushButton(QIcon(":/themes/icons/play.svg"), tr("Start"));
    stop_button = new QPushButton(QIcon(":/themes/icons/stop.svg"), tr("Stop"));
    benchmark_button = new QPushButton(QIcon(":/themes/icons/benchmark.svg"), tr("Run Benchmark"));
    reset_button = new QPushButton(QIcon(":/themes/icons/reset.svg"), tr("Reset"));

    controls_layout->addWidget(start_button, 1, 0);
    controls_layout->addWidget(stop_button, 1, 1);
    controls_layout->addWidget(benchmark_button, 1, 2);
    controls_layout->addWidget(reset_button, 1, 3);

    header_layout->addWidget(controls_panel);
    root->addLayout(header_layout);

    auto *cards_layout = new QHBoxLayout;
    cards_layout->setSpacing(12);

    speed_card = new perfdash::MetricCard(tr("Speed"));
    speed_card->setAccentColor(perfdash::theme::accentBlue());
    cards_layout->addWidget(speed_card);

    frame_time_card = new perfdash::MetricCard(tr("Frame Time"));
    frame_time_card->setAccentColor(perfdash::theme::accentOrange());
    cards_layout->addWidget(frame_time_card);

    host_cpu_card = new perfdash::MetricCard(tr("Host CPU"));
    host_cpu_card->setAccentColor(perfdash::theme::accentGreen());
    cards_layout->addWidget(host_cpu_card);

    fps_card = new perfdash::MetricCard(tr("FPS"));
    fps_card->setAccentColor(perfdash::theme::accentPurple());
    cards_layout->addWidget(fps_card);

    cycles_card = new perfdash::MetricCard(tr("Cycles/s"));
    cycles_card->setAccentColor(perfdash::theme::accentYellow());
    cards_layout->addWidget(cycles_card);

    root->addLayout(cards_layout);

    auto *body_layout = new QGridLayout;
    body_layout->setHorizontalSpacing(12);
    body_layout->setVerticalSpacing(12);

    QVBoxLayout *cpu_panel_layout = nullptr;
    auto *cpu_panel = make_panel(tr("CPU Distribution"), &cpu_panel_layout);
    auto *cpu_content = new QHBoxLayout;
    cpu_content->setSpacing(12);
    cpu_pie = new perfdash::PieWidget;
    cpu_content->addWidget(cpu_pie, 0);
    auto *cpu_rows_layout = new QVBoxLayout;
    cpu_rows_layout->setSpacing(8);
    cpu_rows = create_rows(cpu_rows_layout, view.cpuEntries);
    cpu_rows_layout->addStretch(1);
    cpu_content->addLayout(cpu_rows_layout, 1);
    cpu_panel_layout->addLayout(cpu_content, 1);
    body_layout->addWidget(cpu_panel, 0, 0);

    QVBoxLayout *video_panel_layout = nullptr;
    auto *video_panel = make_panel(tr("Video Pipeline"), &video_panel_layout);
    auto *video_content = new QHBoxLayout;
    video_content->setSpacing(12);
    video_pie = new perfdash::PieWidget;
    video_content->addWidget(video_pie, 0);
    auto *video_rows_layout = new QVBoxLayout;
    video_rows_layout->setSpacing(8);
    video_rows = create_rows(video_rows_layout, view.videoEntries);
    video_rows_layout->addStretch(1);
    video_content->addLayout(video_rows_layout, 1);
    video_panel_layout->addLayout(video_content, 1);
    body_layout->addWidget(video_panel, 0, 1);

    QVBoxLayout *av_panel_layout = nullptr;
    auto *av_panel = make_panel(tr("Audio, Timing and I/O"), &av_panel_layout);

    auto *audio_title = new QLabel(tr("Audio"));
    audio_title->setProperty("metricTitle", true);
    av_panel_layout->addWidget(audio_title);
    audio_rows = create_rows(av_panel_layout, view.audioEntries);

    auto *timing_title = new QLabel(tr("Timing"));
    timing_title->setProperty("metricTitle", true);
    av_panel_layout->addWidget(timing_title);
    timing_rows = create_rows(av_panel_layout, view.timingEntries);

    auto *io_title = new QLabel(tr("I/O"));
    io_title->setProperty("metricTitle", true);
    av_panel_layout->addWidget(io_title);
    io_rows = create_rows(av_panel_layout, view.ioEntries);
    av_panel_layout->addStretch(1);
    body_layout->addWidget(av_panel, 1, 0);

    QVBoxLayout *summary_panel_layout = nullptr;
    auto *summary_panel = make_panel(tr("Session Summary"), &summary_panel_layout);
    auto *summary_grid = new QGridLayout;
    summary_grid->setHorizontalSpacing(12);
    summary_grid->setVerticalSpacing(8);
    add_summary_row(summary_grid, 0, tr("Execution Mode"), &mode_value_label);
    add_summary_row(summary_grid, 1, tr("Refresh Cadence"), &refresh_value_label);
    add_summary_row(summary_grid, 2, tr("Guest Cycles"), &guest_cycles_value_label);
    add_summary_row(summary_grid, 3, tr("Dynarec Blocks"), &dynarec_blocks_value_label);
    add_summary_row(summary_grid, 4, tr("Audio Underruns"), &audio_underruns_value_label);
    add_summary_row(summary_grid, 5, tr("Benchmark"), &benchmark_value_label);
    summary_panel_layout->addLayout(summary_grid);
    summary_panel_layout->addStretch(1);
    body_layout->addWidget(summary_panel, 1, 1);

    root->addLayout(body_layout, 1);
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
    const bool running = model->isRunning();
    const bool benchmark_running = benchmark->isActive();

    start_button->setEnabled(!running && !benchmark_running);
    stop_button->setEnabled(running && !benchmark_running);
    reset_button->setEnabled(!benchmark_running);
    refresh_combo->setEnabled(!benchmark_running);
    duration_combo->setEnabled(!benchmark_running);
    benchmark_button->setText(benchmark_running ? tr("Stop Benchmark") : tr("Run Benchmark"));

    if (benchmark_running) {
        status_value_label->setText(tr("Benchmark Running"));
        status_detail_label->setText(
            tr("%1 remaining. Pause, fast-forward, reset and guest control actions are locked.")
                .arg(tr("%1 s").arg(QString::number(benchmark->remainingSeconds())))
        );
        return;
    }

    if (running) {
        const auto &data = model->viewData();
        status_value_label->setText(tr("Running"));
        status_detail_label->setText(
            tr("Live capture at %1 with %2 active.")
                .arg(format_refresh_hz(data.refreshIntervalMs),
                     data.usingDynarec ? tr("dynarec") : tr("the interpreter"))
        );
        return;
    }

    status_value_label->setText(tr("Stopped"));
    status_detail_label->setText(
        tr("Start capture to refresh the dashboard or run a benchmark from a clean snapshot.")
    );
}

void
PerfDashboardWindow::applyViewData()
{
    const auto &data = model->viewData();
    const auto locale = QLocale();
    const auto speed_text = tr("%1%").arg(QString::number(data.speedPercent, 'f', 1));
    const auto host_cpu_text = tr("%1%").arg(QString::number(data.hostCpuPercent, 'f', 1));
    const auto fps_text = data.framesPerSecond > 0.0 ? QString::number(data.framesPerSecond, 'f', 0) : tr("0");
    const auto frame_time_text = data.frameTimeMs > 0.0 ? tr("%1 ms").arg(QString::number(data.frameTimeMs, 'f', 2)) : tr("n/a");

    speed_card->setValueText(speed_text);
    speed_card->setDetailText(tr("Target speed from the live performance snapshot."));
    speed_card->setSparklineSamples(data.speedHistory);

    frame_time_card->setValueText(frame_time_text);
    frame_time_card->setDetailText(
        data.framesPerSecond > 0.0
            ? tr("Derived from actual rendered frames per second.")
            : tr("Waiting for rendered-frame samples.")
    );
    frame_time_card->setSparklineSamples(data.speedHistory);

    host_cpu_card->setValueText(host_cpu_text);
    host_cpu_card->setDetailText(tr("Main loop wall-time share on the host."));
    host_cpu_card->setSparklineSamples(data.speedHistory);

    fps_card->setValueText(fps_text);
    fps_card->setDetailText(tr("Rendered frames reported by the primary monitor."));
    fps_card->setSparklineSamples(data.speedHistory);

    cycles_card->setValueText(format_rate(data.cyclesPerSecond));
    cycles_card->setDetailText(
        tr("%1 guest cycles accumulated in the latest sample.")
            .arg(locale.toString(data.guestCyclesDelta))
    );
    cycles_card->setSparklineSamples(data.speedHistory);

    applySectionRows(
        data.cpuEntries,
        cpu_rows,
        cpu_pie,
        tr("%1% host")
            .arg(QString::number(sum_entries(data.cpuEntries), 'f', 0))
    );
    applySectionRows(
        data.videoEntries,
        video_rows,
        video_pie,
        tr("%1 FPS")
            .arg(QString::number(data.framesPerSecond, 'f', 0))
    );
    applySectionRows(data.audioEntries, audio_rows, nullptr, QString());
    applySectionRows(data.timingEntries, timing_rows, nullptr, QString());
    applySectionRows(data.ioEntries, io_rows, nullptr, QString());

    mode_value_label->setText(
        data.running ? (data.usingDynarec ? tr("Dynarec") : tr("Interpreter")) : tr("Idle")
    );
    refresh_value_label->setText(
        tr("%1 (%2 ms)")
            .arg(format_refresh_hz(data.refreshIntervalMs),
                 locale.toString(data.refreshIntervalMs))
    );
    guest_cycles_value_label->setText(locale.toString(data.guestCyclesTotal));
    dynarec_blocks_value_label->setText(locale.toString(data.dynarecBlocks));
    audio_underruns_value_label->setText(locale.toString(data.audioUnderruns));

    if (benchmark->isActive()) {
        benchmark_value_label->setText(
            tr("Running, %1 left").arg(tr("%1 s").arg(QString::number(benchmark->remainingSeconds())))
        );
    } else if (!benchmark->lastResult().jsonPayload.isEmpty()) {
        benchmark_value_label->setText(
            tr("Last score %1 / 100")
                .arg(QString::number(benchmark->lastResult().score, 'f', 1))
        );
    } else {
        benchmark_value_label->setText(tr("Idle"));
    }
}

void
PerfDashboardWindow::applySectionRows(const QVector<perfdash::SectionEntry> &entries,
                                      const QVector<perfdash::BarRow *> &rows,
                                      perfdash::PieWidget *pie,
                                      const QString &center_text)
{
    QVector<perfdash::PieSlice> slices;
    slices.reserve(entries.size());

    const auto count = std::min(entries.size(), rows.size());
    for (int i = 0; i < count; ++i) {
        rows[i]->setLabel(entries[i].label);
        rows[i]->setColor(entries[i].color);
        rows[i]->setValue(entries[i].percent);
        rows[i]->setSuffix(tr("%"));

        if (entries[i].percent > 0.0)
            slices.push_back({ entries[i].label, entries[i].color, entries[i].percent });
    }

    if (!pie)
        return;

    pie->setSlices(slices);
    pie->setCenterText(center_text.isEmpty() ? tr("No activity") : center_text);
}

int
PerfDashboardWindow::selectedRefreshIntervalMs() const
{
    return refresh_combo->currentData().toInt();
}

int
PerfDashboardWindow::selectedBenchmarkDurationSeconds() const
{
    return duration_combo->currentData().toInt();
}

void
PerfDashboardWindow::lockBenchmarkControls(bool locked)
{
    if (benchmark_locked_controls == locked)
        return;

    benchmark_locked_controls = locked;

    if (auto *window = mainWindow())
        window->setBenchmarkControlsLocked(locked);
}

MainWindow *
PerfDashboardWindow::mainWindow() const
{
    return qobject_cast<MainWindow *>(parentWidget());
}
