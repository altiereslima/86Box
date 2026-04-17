#ifndef QT_PERFDASH_BENCHMARK_HPP
#define QT_PERFDASH_BENCHMARK_HPP

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>

#include "qt_perfdash_model.hpp"

namespace perfdash {

struct PerfBenchmarkResult {
    bool      completed = false;
    int       durationSeconds = 30;
    double    score = 0.0;
    double    cpuScore = 0.0;
    double    videoScore = 0.0;
    double    soundScore = 0.0;
    double    ioScore = 0.0;
    double    speedAverage = 0.0;
    double    frameTimeAverage = 0.0;
    double    frameTimeMax = 0.0;
    double    fpsAverage = 0.0;
    double    fpsStable = 0.0;
    double    underrunRate = 0.0;
    quint64   totalGuestCycles = 0;
    quint64   dynarecBlocks = 0;
    quint64   audioUnderruns = 0;
    QByteArray jsonPayload;
};

class PerfBenchmark : public QObject {
    Q_OBJECT

public:
    explicit PerfBenchmark(QObject *parent = nullptr);

    void start(int duration_seconds);
    void stop();
    void cancel();
    void consumeSample(const DashboardViewData &data);

    bool isActive() const;
    int  remainingSeconds() const;

    const PerfBenchmarkResult &lastResult() const;
    void showResultDialog(QWidget *parent) const;

signals:
    void stateChanged();
    void finished();

private:
    void finalize(bool completed);

    bool                active = false;
    int                 duration_seconds = 30;
    QElapsedTimer       elapsed;
    QVector<double>     speed_samples;
    QVector<double>     frame_samples;
    QVector<double>     fps_samples;
    QVector<double>     cpu_samples;
    QVector<double>     video_samples;
    QVector<double>     sound_samples;
    QVector<double>     io_samples;
    quint64             total_guest_cycles = 0;
    quint64             latest_dynarec_blocks = 0;
    quint64             latest_audio_underruns = 0;
    PerfBenchmarkResult result;
};

}

#endif
