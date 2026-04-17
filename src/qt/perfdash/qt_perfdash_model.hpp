#ifndef QT_PERFDASH_MODEL_HPP
#define QT_PERFDASH_MODEL_HPP

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

extern "C" {
#include <86box/perf_dashboard.h>
}

namespace perfdash {

struct SectionEntry {
    QString label;
    QColor  color;
    double  percent = 0.0;
};

struct DashboardViewData {
    bool     running = false;
    bool     usingDynarec = false;
    int      refreshIntervalMs = 200;
    double   speedPercent = 0.0;
    double   frameTimeMs = 0.0;
    double   hostCpuPercent = 0.0;
    double   framesPerSecond = 0.0;
    double   cyclesPerSecond = 0.0;
    quint64  guestCyclesDelta = 0;
    quint64  guestCyclesTotal = 0;
    quint64  dynarecBlocks = 0;
    quint64  audioUnderruns = 0;
    QVector<double> speedHistory;
    QVector<SectionEntry> cpuEntries;
    QVector<SectionEntry> videoEntries;
    QVector<SectionEntry> audioEntries;
    QVector<SectionEntry> timingEntries;
    QVector<SectionEntry> ioEntries;
};

class PerfDashModel : public QObject {
    Q_OBJECT

public:
    explicit PerfDashModel(QObject *parent = nullptr);

    void start();
    void stop();
    void reset();
    void setRefreshIntervalMs(int interval_ms);

    bool isRunning() const;
    int  refreshIntervalMs() const;

    const DashboardViewData &viewData() const;

signals:
    void dataUpdated();

private slots:
    void refresh();

private:
    void resetViewData();
    void appendSpeedSample(double speed_percent, quint64 delta_wall_ns);

    QTimer           *refresh_timer = nullptr;
    perf_snapshot_t   previous_snapshot = {};
    bool              have_previous_snapshot = false;
    quint64           pending_speed_sample_ns = 0;
    DashboardViewData current_view;
};

}

#endif
