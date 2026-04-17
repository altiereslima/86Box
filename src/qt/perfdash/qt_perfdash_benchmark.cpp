#include "qt_perfdash_benchmark.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace perfdash {

namespace {

double
average(const QVector<double> &values)
{
    if (values.isEmpty())
        return 0.0;

    double sum = 0.0;
    for (const auto &value : values)
        sum += value;

    return sum / (double) values.size();
}

double
maximum(const QVector<double> &values)
{
    if (values.isEmpty())
        return 0.0;

    double max_value = values.front();
    for (const auto &value : values)
        max_value = std::max(max_value, value);

    return max_value;
}

double
stability_score(const QVector<double> &values)
{
    if (values.size() < 2)
        return values.isEmpty() ? 0.0 : 100.0;

    const double avg = average(values);
    if (avg <= 0.0)
        return 0.0;

    double variance = 0.0;
    for (const auto &value : values) {
        const double diff = value - avg;
        variance += diff * diff;
    }
    variance /= (double) values.size();

    const double deviation = std::sqrt(variance);
    return std::clamp(100.0 - ((deviation / avg) * 100.0), 0.0, 100.0);
}

double
sum_section(const QVector<SectionEntry> &entries)
{
    double total = 0.0;
    for (const auto &entry : entries)
        total += entry.percent;
    return total;
}

}

PerfBenchmark::PerfBenchmark(QObject *parent)
    : QObject(parent)
{
}

void
PerfBenchmark::start(int duration_seconds_value)
{
    active = true;
    duration_seconds = duration_seconds_value;
    elapsed.restart();
    speed_samples.clear();
    frame_samples.clear();
    fps_samples.clear();
    cpu_samples.clear();
    video_samples.clear();
    sound_samples.clear();
    io_samples.clear();
    total_guest_cycles = 0;
    latest_dynarec_blocks = 0;
    latest_audio_underruns = 0;
    result = {};
    emit stateChanged();
}

void
PerfBenchmark::stop()
{
    if (!active)
        return;

    finalize(false);
}

void
PerfBenchmark::cancel()
{
    if (!active)
        return;

    active = false;
    emit stateChanged();
}

void
PerfBenchmark::consumeSample(const DashboardViewData &data)
{
    if (!active)
        return;

    speed_samples.append(data.speedPercent);
    frame_samples.append(data.frameTimeMs);
    fps_samples.append(data.framesPerSecond);
    cpu_samples.append(sum_section(data.cpuEntries));
    video_samples.append(sum_section(data.videoEntries));
    sound_samples.append(sum_section(data.audioEntries));
    io_samples.append(sum_section(data.ioEntries));
    total_guest_cycles += data.guestCyclesDelta;
    latest_dynarec_blocks = data.dynarecBlocks;
    latest_audio_underruns = data.audioUnderruns;

    if (elapsed.elapsed() >= (duration_seconds * 1000))
        finalize(true);
    else
        emit stateChanged();
}

bool
PerfBenchmark::isActive() const
{
    return active;
}

int
PerfBenchmark::remainingSeconds() const
{
    if (!active)
        return 0;

    const int elapsed_ms = (int) elapsed.elapsed();
    const int remaining_ms = std::max(0, (duration_seconds * 1000) - elapsed_ms);
    return (remaining_ms + 999) / 1000;
}

const PerfBenchmarkResult &
PerfBenchmark::lastResult() const
{
    return result;
}

void
PerfBenchmark::showResultDialog(QWidget *parent) const
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Benchmark Result"));
    dialog.setModal(true);
    dialog.resize(620, 520);

    auto *layout = new QVBoxLayout(&dialog);
    auto *summary = new QLabel(
        QObject::tr("Score %1 / 100")
            .arg(QString::number(result.score, 'f', 1)));
    summary->setProperty("sectionTitle", true);
    layout->addWidget(summary);

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel(QObject::tr("CPU")), 0, 0);
    grid->addWidget(new QLabel(QString::number(result.cpuScore, 'f', 1)), 0, 1);
    grid->addWidget(new QLabel(QObject::tr("Video")), 1, 0);
    grid->addWidget(new QLabel(QString::number(result.videoScore, 'f', 1)), 1, 1);
    grid->addWidget(new QLabel(QObject::tr("Sound")), 2, 0);
    grid->addWidget(new QLabel(QString::number(result.soundScore, 'f', 1)), 2, 1);
    grid->addWidget(new QLabel(QObject::tr("I/O")), 3, 0);
    grid->addWidget(new QLabel(QString::number(result.ioScore, 'f', 1)), 3, 1);
    grid->addWidget(new QLabel(QObject::tr("Average Speed")), 0, 2);
    grid->addWidget(new QLabel(QStringLiteral("%1 %").arg(QString::number(result.speedAverage, 'f', 1))), 0, 3);
    grid->addWidget(new QLabel(QObject::tr("Average Frame Time")), 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("%1 ms").arg(QString::number(result.frameTimeAverage, 'f', 2))), 1, 3);
    grid->addWidget(new QLabel(QObject::tr("Max Frame Time")), 2, 2);
    grid->addWidget(new QLabel(QStringLiteral("%1 ms").arg(QString::number(result.frameTimeMax, 'f', 2))), 2, 3);
    grid->addWidget(new QLabel(QObject::tr("Average FPS")), 3, 2);
    grid->addWidget(new QLabel(QString::number(result.fpsAverage, 'f', 1)), 3, 3);
    grid->addWidget(new QLabel(QObject::tr("Total Cycles")), 4, 0);
    grid->addWidget(new QLabel(QString::number(result.totalGuestCycles)), 4, 1, 1, 3);
    layout->addLayout(grid);

    auto *formula = new QLabel(
        QObject::tr("Formula: 0.4*Speed + 0.25*Inverse Frame Time + 0.2*FPS Stability + 0.15*(1 - underrun rate).")
    );
    formula->setWordWrap(true);
    formula->setProperty("metricMeta", true);
    layout->addWidget(formula);

    auto *json_view = new QPlainTextEdit;
    json_view->setReadOnly(true);
    json_view->setPlainText(QString::fromUtf8(result.jsonPayload));
    layout->addWidget(json_view, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *copy_button = buttons->addButton(QObject::tr("Copy Result"), QDialogButtonBox::ActionRole);
    QObject::connect(copy_button, &QPushButton::clicked, [&]() {
        QApplication::clipboard()->setText(QString::fromUtf8(result.jsonPayload));
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void
PerfBenchmark::finalize(bool completed)
{
    active = false;

    const double avg_speed = average(speed_samples);
    const double avg_frame = average(frame_samples);
    const double max_frame = maximum(frame_samples);
    const double avg_fps = average(fps_samples);
    const double fps_stable = stability_score(fps_samples);
    const double cpu_score = std::clamp(avg_speed, 0.0, 100.0);
    const double video_score = (max_frame > 0.0) ? std::clamp((16.6667 / max_frame) * 100.0, 0.0, 100.0) : 0.0;
    const double sound_score = std::clamp(100.0 - (double) latest_audio_underruns, 0.0, 100.0);
    const double io_score = io_samples.isEmpty() ? 100.0 : std::clamp(average(io_samples), 0.0, 100.0);
    const double underrun_rate = speed_samples.isEmpty() ? 0.0 : ((double) latest_audio_underruns / (double) speed_samples.size());
    const double audio_component = std::clamp(100.0 * (1.0 - underrun_rate), 0.0, 100.0);
    const double score = std::clamp(
        (0.4 * std::clamp(avg_speed, 0.0, 100.0))
        + (0.25 * video_score)
        + (0.2 * fps_stable)
        + (0.15 * audio_component),
        0.0,
        100.0);

    result = {};
    result.completed = completed;
    result.durationSeconds = duration_seconds;
    result.score = score;
    result.cpuScore = cpu_score;
    result.videoScore = video_score;
    result.soundScore = sound_score;
    result.ioScore = io_score;
    result.speedAverage = avg_speed;
    result.frameTimeAverage = avg_frame;
    result.frameTimeMax = max_frame;
    result.fpsAverage = avg_fps;
    result.fpsStable = fps_stable;
    result.underrunRate = underrun_rate;
    result.totalGuestCycles = total_guest_cycles;
    result.dynarecBlocks = latest_dynarec_blocks;
    result.audioUnderruns = latest_audio_underruns;

    QJsonObject json;
    json.insert(QStringLiteral("completed"), result.completed);
    json.insert(QStringLiteral("duration_seconds"), result.durationSeconds);
    json.insert(QStringLiteral("score"), result.score);
    json.insert(QStringLiteral("cpu_score"), result.cpuScore);
    json.insert(QStringLiteral("video_score"), result.videoScore);
    json.insert(QStringLiteral("sound_score"), result.soundScore);
    json.insert(QStringLiteral("io_score"), result.ioScore);
    json.insert(QStringLiteral("speed_avg"), result.speedAverage);
    json.insert(QStringLiteral("frame_time_avg_ms"), result.frameTimeAverage);
    json.insert(QStringLiteral("frame_time_max_ms"), result.frameTimeMax);
    json.insert(QStringLiteral("fps_avg"), result.fpsAverage);
    json.insert(QStringLiteral("fps_stability"), result.fpsStable);
    json.insert(QStringLiteral("audio_underruns"), QString::number(result.audioUnderruns));
    json.insert(QStringLiteral("underrun_rate"), result.underrunRate);
    json.insert(QStringLiteral("guest_cycles"), QString::number(result.totalGuestCycles));
    json.insert(QStringLiteral("dynarec_blocks"), QString::number(result.dynarecBlocks));
    result.jsonPayload = QJsonDocument(json).toJson(QJsonDocument::Indented);

    emit stateChanged();
    emit finished();
}

}
