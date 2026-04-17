#include "qt_perfdash_model.hpp"

#include <QTimer>

#include <algorithm>

#include "qt_perfdash_theme.hpp"

namespace perfdash {

namespace {

quint64
delta_u64(quint64 current, quint64 previous)
{
    return (current >= previous) ? (current - previous) : 0;
}

double
percent_of_wall(quint64 current, quint64 previous, quint64 wall_delta)
{
    if (!wall_delta)
        return 0.0;

    return std::clamp((double) delta_u64(current, previous) * 100.0 / (double) wall_delta, 0.0, 100.0);
}

QVector<SectionEntry>
make_cpu_entries()
{
    return {
        { QObject::tr("Interpreter"), theme::accentBlue(), 0.0 },
        { QObject::tr("Dynarec"), theme::accentGreen(), 0.0 },
        { QObject::tr("Memory"), theme::accentYellow(), 0.0 },
        { QObject::tr("TLB"), theme::accentOrange(), 0.0 },
        { QObject::tr("Branch"), theme::accentRed(), 0.0 }
    };
}

QVector<SectionEntry>
make_video_entries()
{
    return {
        { QObject::tr("S3"), theme::accentBlue(), 0.0 },
        { QObject::tr("Matrox"), theme::accentPurple(), 0.0 },
        { QObject::tr("Voodoo FIFO"), theme::accentOrange(), 0.0 },
        { QObject::tr("Voodoo Render"), theme::accentRed(), 0.0 },
        { QObject::tr("Blit/Page Flip"), theme::accentGreen(), 0.0 }
    };
}

QVector<SectionEntry>
make_audio_entries()
{
    return {
        { QObject::tr("Mixer"), theme::accentBlue(), 0.0 },
        { QObject::tr("EMU8K"), theme::accentGreen(), 0.0 },
        { QObject::tr("OPL"), theme::accentYellow(), 0.0 },
        { QObject::tr("PC Speaker"), theme::accentOrange(), 0.0 }
    };
}

QVector<SectionEntry>
make_timing_entries()
{
    return {
        { QObject::tr("PIT"), theme::accentBlue(), 0.0 },
        { QObject::tr("IRQ"), theme::accentYellow(), 0.0 },
        { QObject::tr("DMA"), theme::accentRed(), 0.0 }
    };
}

QVector<SectionEntry>
make_io_entries()
{
    return {
        { QObject::tr("HDD"), theme::accentBlue(), 0.0 },
        { QObject::tr("CD-ROM"), theme::accentYellow(), 0.0 },
        { QObject::tr("Floppy"), theme::accentOrange(), 0.0 }
    };
}

}

PerfDashModel::PerfDashModel(QObject *parent)
    : QObject(parent)
    , refresh_timer(new QTimer(this))
{
    refresh_timer->setInterval(200);
    connect(refresh_timer, &QTimer::timeout, this, &PerfDashModel::refresh);
    resetViewData();
}

void
PerfDashModel::start()
{
    if (refresh_timer->isActive())
        return;

    perfdash_start();
    have_previous_snapshot = false;
    pending_speed_sample_ns = 0;
    current_view.running = true;
    refresh_timer->start();
    refresh();
}

void
PerfDashModel::stop()
{
    perfdash_stop();
    refresh_timer->stop();
    current_view.running = false;
    refresh();
    emit dataUpdated();
}

void
PerfDashModel::reset()
{
    perfdash_reset();
    have_previous_snapshot = false;
    pending_speed_sample_ns = 0;
    resetViewData();
    current_view.running = refresh_timer->isActive();
    emit dataUpdated();
}

void
PerfDashModel::setRefreshIntervalMs(int interval_ms)
{
    refresh_timer->setInterval(interval_ms);
    current_view.refreshIntervalMs = interval_ms;
}

bool
PerfDashModel::isRunning() const
{
    return refresh_timer->isActive();
}

int
PerfDashModel::refreshIntervalMs() const
{
    return refresh_timer->interval();
}

const DashboardViewData &
PerfDashModel::viewData() const
{
    return current_view;
}

void
PerfDashModel::refresh()
{
    perf_snapshot_t current_snapshot = {};
    perfdash_take_snapshot(&current_snapshot);

    current_view.running = refresh_timer->isActive();
    current_view.refreshIntervalMs = refresh_timer->interval();
    current_view.usingDynarec = current_snapshot.using_dynarec != 0;
    current_view.speedPercent = (double) current_snapshot.speed_percent;
    current_view.framesPerSecond = (double) current_snapshot.render_fps;
    current_view.frameTimeMs = current_snapshot.render_fps ? (1000.0 / (double) current_snapshot.render_fps) : 0.0;
    current_view.dynarecBlocks = current_snapshot.dynarec_blocks;
    current_view.audioUnderruns = current_snapshot.audio_underruns;

    if (!have_previous_snapshot) {
        current_view.cyclesPerSecond = (double) current_snapshot.guest_cycles_per_sec;
        current_view.hostCpuPercent = 0.0;
        appendSpeedSample(current_view.speedPercent, 1000000000ULL);
        previous_snapshot = current_snapshot;
        have_previous_snapshot = true;
        emit dataUpdated();
        return;
    }

    const auto delta_wall_ns = delta_u64(current_snapshot.wall_ns, previous_snapshot.wall_ns);
    const auto delta_guest_tsc = delta_u64(current_snapshot.guest_tsc, previous_snapshot.guest_tsc);

    current_view.guestCyclesDelta = delta_guest_tsc;
    current_view.guestCyclesTotal += delta_guest_tsc;
    if (delta_wall_ns)
        current_view.cyclesPerSecond = (double) delta_guest_tsc * 1000000000.0 / (double) delta_wall_ns;
    else
        current_view.cyclesPerSecond = (double) current_snapshot.guest_cycles_per_sec;

    current_view.hostCpuPercent = percent_of_wall(
        current_snapshot.ns_accum[PERF_DOMAIN_MAIN_LOOP],
        previous_snapshot.ns_accum[PERF_DOMAIN_MAIN_LOOP],
        delta_wall_ns);

    current_view.cpuEntries[0].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_CPU_INTERP], previous_snapshot.ns_accum[PERF_DOMAIN_CPU_INTERP], delta_wall_ns);
    current_view.cpuEntries[1].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_CPU_DYNAREC], previous_snapshot.ns_accum[PERF_DOMAIN_CPU_DYNAREC], delta_wall_ns);
    current_view.cpuEntries[2].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_CPU_MEM_FAST], previous_snapshot.ns_accum[PERF_DOMAIN_CPU_MEM_FAST], delta_wall_ns)
                                      + percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_CPU_MEM_SLOW], previous_snapshot.ns_accum[PERF_DOMAIN_CPU_MEM_SLOW], delta_wall_ns);
    current_view.cpuEntries[3].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_CPU_TLB], previous_snapshot.ns_accum[PERF_DOMAIN_CPU_TLB], delta_wall_ns);
    current_view.cpuEntries[4].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_CPU_BRANCH_ABORT], previous_snapshot.ns_accum[PERF_DOMAIN_CPU_BRANCH_ABORT], delta_wall_ns);

    current_view.videoEntries[0].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_VIDEO_S3], previous_snapshot.ns_accum[PERF_DOMAIN_VIDEO_S3], delta_wall_ns);
    current_view.videoEntries[1].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_VIDEO_MATROX], previous_snapshot.ns_accum[PERF_DOMAIN_VIDEO_MATROX], delta_wall_ns);
    current_view.videoEntries[2].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_VIDEO_VOODOO_FIFO], previous_snapshot.ns_accum[PERF_DOMAIN_VIDEO_VOODOO_FIFO], delta_wall_ns);
    current_view.videoEntries[3].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_VIDEO_VOODOO_RENDER], previous_snapshot.ns_accum[PERF_DOMAIN_VIDEO_VOODOO_RENDER], delta_wall_ns);
    current_view.videoEntries[4].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_VIDEO_BLIT], previous_snapshot.ns_accum[PERF_DOMAIN_VIDEO_BLIT], delta_wall_ns)
                                        + percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_VIDEO_PAGEFLIP], previous_snapshot.ns_accum[PERF_DOMAIN_VIDEO_PAGEFLIP], delta_wall_ns);

    current_view.audioEntries[0].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_SOUND_MIX], previous_snapshot.ns_accum[PERF_DOMAIN_SOUND_MIX], delta_wall_ns);
    current_view.audioEntries[1].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_SOUND_EMU8K], previous_snapshot.ns_accum[PERF_DOMAIN_SOUND_EMU8K], delta_wall_ns);
    current_view.audioEntries[2].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_SOUND_OPL], previous_snapshot.ns_accum[PERF_DOMAIN_SOUND_OPL], delta_wall_ns);
    current_view.audioEntries[3].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_SOUND_SPK], previous_snapshot.ns_accum[PERF_DOMAIN_SOUND_SPK], delta_wall_ns);

    current_view.timingEntries[0].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_TIMING_PIT], previous_snapshot.ns_accum[PERF_DOMAIN_TIMING_PIT], delta_wall_ns);
    current_view.timingEntries[1].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_TIMING_IRQ], previous_snapshot.ns_accum[PERF_DOMAIN_TIMING_IRQ], delta_wall_ns);
    current_view.timingEntries[2].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_TIMING_DMA], previous_snapshot.ns_accum[PERF_DOMAIN_TIMING_DMA], delta_wall_ns);

    current_view.ioEntries[0].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_IO_HDD], previous_snapshot.ns_accum[PERF_DOMAIN_IO_HDD], delta_wall_ns);
    current_view.ioEntries[1].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_IO_CDROM], previous_snapshot.ns_accum[PERF_DOMAIN_IO_CDROM], delta_wall_ns);
    current_view.ioEntries[2].percent = percent_of_wall(current_snapshot.ns_accum[PERF_DOMAIN_IO_FLOPPY], previous_snapshot.ns_accum[PERF_DOMAIN_IO_FLOPPY], delta_wall_ns);

    appendSpeedSample(current_view.speedPercent, delta_wall_ns);
    previous_snapshot = current_snapshot;
    emit dataUpdated();
}

void
PerfDashModel::resetViewData()
{
    current_view = {};
    current_view.refreshIntervalMs = refresh_timer->interval();
    current_view.cpuEntries = make_cpu_entries();
    current_view.videoEntries = make_video_entries();
    current_view.audioEntries = make_audio_entries();
    current_view.timingEntries = make_timing_entries();
    current_view.ioEntries = make_io_entries();
    current_view.speedHistory.clear();
}

void
PerfDashModel::appendSpeedSample(double speed_percent, quint64 delta_wall_ns)
{
    Q_UNUSED(delta_wall_ns);

    current_view.speedHistory.append(speed_percent);

    while (current_view.speedHistory.size() > 120)
        current_view.speedHistory.removeFirst();
}

}
