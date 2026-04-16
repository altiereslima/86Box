#ifndef QT_PERFDASH_WINDOW_HPP
#define QT_PERFDASH_WINDOW_HPP

#include <QDialog>

class QCloseEvent;
class QLabel;
class QPushButton;

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

private:
    void restoreWindowSettings();
    void saveWindowSettings() const;
    void updateControlState();

    bool         running = false;
    QLabel      *status_value_label = nullptr;
    QLabel      *status_detail_label = nullptr;
    QPushButton *start_button = nullptr;
    QPushButton *stop_button = nullptr;
    QPushButton *reset_button = nullptr;
    QPushButton *benchmark_button = nullptr;
};

#endif
