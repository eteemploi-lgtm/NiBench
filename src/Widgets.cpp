#include "Widgets.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <complex>

void Led::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c(150, 150, 150);
    if (m_s == 1) c = QColor(40, 190, 80);
    else if (m_s == 2) c = QColor(220, 50, 50);
    else if (m_s == 3) c = QColor(240, 160, 30);
    p.setPen(QPen(c.darker(150), 1.5));
    p.setBrush(c);
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));
}

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PlotWidget::append(const QVector<double>& v)
{
    m_data += v;
    trim();
    update();
}

QVector<double> PlotWidget::spectrumDbV(const QVector<double>& x, int n)
{
    QVector<double> out;
    if (x.size() < n || n < 16) return out;
    int N = 1; while ((N << 1) <= n) N <<= 1;
    std::vector<std::complex<double>> a(N);
    const int off = x.size() - N;
    double sumw = 0;
    for (int i = 0; i < N; ++i) {
        double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1));
        a[i] = x[off + i] * w;
        sumw += w;
    }
    for (int i = 1, j = 0; i < N; ++i) {          // bit reversal
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= N; len <<= 1) {
        double ang = -2 * M_PI / len;
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<double> w(1);
            for (int k = 0; k < len / 2; ++k) {
                auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    out.resize(N / 2);
    for (int k = 0; k < N / 2; ++k) {
        double amp = 2.0 * std::abs(a[k]) / sumw;           // crete en V
        out[k] = 20.0 * std::log10(qMax(amp, 1e-12));
    }
    return out;
}

void PlotWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(18, 22, 28));
    const QRect plot = rect().adjusted(58, 12, -12, -26);
    p.setPen(QColor(70, 78, 90));
    p.drawRect(plot);

    QVector<double> y; double x0 = 0, x1 = 1; QString xlab, ylab;
    double ymin = 0, ymax = 1;
    if (m_mode == Spectrum) {
        y = spectrumDbV(m_data, 4096);
        x1 = m_fs / 2.0; xlab = "Hz"; ylab = "dBV";
        ymin = -140; ymax = 0;
    } else {
        y = m_data;
        x1 = (m_fs > 0 ? m_hist / m_fs : m_hist); xlab = (m_fs > 0 ? "s" : "pts"); ylab = m_unit;
        if (!y.isEmpty()) {
            ymin = *std::min_element(y.begin(), y.end());
            ymax = *std::max_element(y.begin(), y.end());
            if (ymax - ymin < 1e-9) { ymin -= 0.5; ymax += 0.5; }
            double pad = (ymax - ymin) * 0.1; ymin -= pad; ymax += pad;
        } else { ymin = -1; ymax = 1; }
    }

    p.setPen(QColor(40, 48, 58));
    for (int i = 1; i < 5; ++i) {
        int yy = plot.top() + plot.height() * i / 5;
        p.drawLine(plot.left(), yy, plot.right(), yy);
        int xx = plot.left() + plot.width() * i / 5;
        p.drawLine(xx, plot.top(), xx, plot.bottom());
    }
    p.setPen(QColor(170, 180, 195));
    QFont f = font(); f.setPointSize(8); p.setFont(f);
    for (int i = 0; i <= 5; ++i) {
        double v = ymax - (ymax - ymin) * i / 5.0;
        p.drawText(QRect(0, plot.top() + plot.height() * i / 5 - 8, 54, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'g', 4));
        double xv = x0 + (x1 - x0) * i / 5.0;
        p.drawText(QRect(plot.left() + plot.width() * i / 5 - 30, plot.bottom() + 4, 60, 16), Qt::AlignHCenter,
                   QString::number(xv, 'g', 4));
    }
    p.drawText(QRect(plot.right() - 70, plot.bottom() + 4, 70, 16), Qt::AlignRight, xlab);
    p.drawText(QRect(4, 0, 50, 12), Qt::AlignLeft, ylab);

    if (y.isEmpty()) {
        p.setPen(QColor(140, 150, 165));
        p.drawText(plot, Qt::AlignCenter, "(pas de donnees)");
        return;
    }
    QPainterPath path;
    const int n = y.size();
    const int step = qMax(1, n / (plot.width() * 2));
    bool first = true;
    for (int i = 0; i < n; i += step) {
        double fx = (m_mode == Spectrum) ? double(i) / n : double(i) / qMax(1, m_hist - 1);
        double px = plot.left() + fx * plot.width();
        double py = plot.bottom() - (y[i] - ymin) / (ymax - ymin) * plot.height();
        py = qBound<double>(plot.top(), py, plot.bottom());
        if (first) { path.moveTo(px, py); first = false; } else path.lineTo(px, py);
    }
    p.setPen(QPen(m_mode == Spectrum ? QColor(255, 170, 60) : QColor(70, 200, 255), 1.4));
    p.drawPath(path);
}
