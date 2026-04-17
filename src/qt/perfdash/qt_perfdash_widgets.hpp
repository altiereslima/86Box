#ifndef QT_PERFDASH_WIDGETS_HPP
#define QT_PERFDASH_WIDGETS_HPP

#include <QColor>
#include <QFrame>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;

namespace perfdash {

struct PieSlice {
    QString label;
    QColor  color;
    double  value = 0.0;
};

class SparklineWidget : public QWidget {
    Q_OBJECT

public:
    explicit SparklineWidget(QWidget *parent = nullptr);

    void setSamples(const QVector<double> &next_samples);
    void setLineColor(const QColor &next_color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> samples;
    QColor          line_color;
};

class MetricCard : public QFrame {
    Q_OBJECT

public:
    explicit MetricCard(const QString &title, QWidget *parent = nullptr);

    void setValueText(const QString &text);
    void setDetailText(const QString &text);
    void setSparklineSamples(const QVector<double> &samples);
    void setAccentColor(const QColor &color);

private:
    QLabel          *title_label = nullptr;
    QLabel          *value_label = nullptr;
    QLabel          *detail_label = nullptr;
    SparklineWidget *sparkline_widget = nullptr;
};

class BarRow : public QWidget {
    Q_OBJECT

public:
    explicit BarRow(QWidget *parent = nullptr);

    void setLabel(const QString &text);
    void setValue(double next_value);
    void setColor(const QColor &next_color);
    void setSuffix(const QString &text);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString label;
    QString suffix;
    QColor  fill_color;
    double  value = 0.0;
};

class PieWidget : public QWidget {
    Q_OBJECT

public:
    explicit PieWidget(QWidget *parent = nullptr);

    void setSlices(const QVector<PieSlice> &next_slices);
    void setCenterText(const QString &text);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<PieSlice> slices;
    QString           center_text;
};

}

#endif
