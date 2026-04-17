#ifndef QT_PERFDASH_WINDOW_HPP
#define QT_PERFDASH_WINDOW_HPP

#include <QDialog>
#include <QVector>

#include "qt_perfdash_model.hpp"

class QCloseEvent;
class QComboBox;
class QLabel;
class QPushButton;

class MainWindow;

namespace perfdash {
class BarRow;
class MetricCard;
class PerfBenchmark;
class PerfDashModel;
class PieWidget;
}

class PerfDashboardWindow : public QDialog {
    Q_OBJECT

public:
    explicit PerfDashboardWindow(QWidget *parent = nullptr);
    ~PerfDashboardWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startCapture();
    void stopCapture();
    void resetCapture();
    void toggleBenchmark();
    void handleModelUpdated();
    void handleBenchmarkFinished();

private:
    void buildUi();
    void restoreWindowSettings();
    void saveWindowSettings() const;
    void updateControlState();
    void applyViewData();
    void applySectionRows(const QVector<perfdash::SectionEntry> &entries,
                          const QVector<perfdash::BarRow *> &rows,
                          perfdash::PieWidget *pie,
                          const QString &center_text);
    int selectedRefreshIntervalMs() const;
    int selectedBenchmarkDurationSeconds() const;
    void lockBenchmarkControls(bool locked);
    MainWindow *mainWindow() const;

    perfdash::PerfDashModel      *model = nullptr;
    perfdash::PerfBenchmark      *benchmark = nullptr;
    QLabel                       *status_value_label = nullptr;
    QLabel                       *status_detail_label = nullptr;
    QPushButton                  *start_button = nullptr;
    QPushButton                  *stop_button = nullptr;
    QPushButton                  *reset_button = nullptr;
    QPushButton                  *benchmark_button = nullptr;
    QComboBox                    *refresh_combo = nullptr;
    QComboBox                    *duration_combo = nullptr;
    perfdash::MetricCard         *speed_card = nullptr;
    perfdash::MetricCard         *frame_time_card = nullptr;
    perfdash::MetricCard         *host_cpu_card = nullptr;
    perfdash::MetricCard         *fps_card = nullptr;
    perfdash::MetricCard         *cycles_card = nullptr;
    perfdash::PieWidget          *cpu_pie = nullptr;
    perfdash::PieWidget          *video_pie = nullptr;
    QVector<perfdash::BarRow *>   cpu_rows;
    QVector<perfdash::BarRow *>   video_rows;
    QVector<perfdash::BarRow *>   audio_rows;
    QVector<perfdash::BarRow *>   timing_rows;
    QVector<perfdash::BarRow *>   io_rows;
    QLabel                       *mode_value_label = nullptr;
    QLabel                       *refresh_value_label = nullptr;
    QLabel                       *guest_cycles_value_label = nullptr;
    QLabel                       *dynarec_blocks_value_label = nullptr;
    QLabel                       *audio_underruns_value_label = nullptr;
    QLabel                       *benchmark_value_label = nullptr;
    bool                          benchmark_locked_controls = false;
};

#endif
