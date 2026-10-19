#pragma once
// Petits widgets du banc d'essai : LED et graphe (temps / spectre).
#include <QWidget>
#include <QVector>
#include <QColor>
#include <QString>

class Led : public QWidget
{
    Q_OBJECT
public:
    explicit Led(QWidget* parent = nullptr) : QWidget(parent) { setFixedSize(18, 18); }
    void setState(int s) { if (s != m_s) { m_s = s; update(); } }   // 0 gris, 1 vert, 2 rouge, 3 orange
protected:
    void paintEvent(QPaintEvent*) override;
private:
    int m_s = 0;
};

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Time, Spectrum };
    explicit PlotWidget(QWidget* parent = nullptr);
    void setMode(Mode m) { m_mode = m; update(); }
    void setSampleRate(double fs) { m_fs = fs; update(); }
    void setHistory(int n) { m_hist = qMax(16, n); trim(); update(); }
    void setUnit(const QString& u) { m_unit = u; update(); }
    void append(const QVector<double>& v);
    void clear() { m_data.clear(); update(); }
    const QVector<double>& data() const { return m_data; }
    static QVector<double> spectrumDbV(const QVector<double>& x, int n);   // |FFT| en dBV crete
protected:
    void paintEvent(QPaintEvent*) override;
    QSize minimumSizeHint() const override { return QSize(300, 180); }
private:
    void trim() { if (m_data.size() > m_hist) m_data.remove(0, m_data.size() - m_hist); }
    QVector<double> m_data;
    int m_hist = 4096;
    double m_fs = 1.0;
    Mode m_mode = Time;
    QString m_unit = "V";
};
