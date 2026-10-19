#include "NiApi.h"
#include <QByteArray>
#include <QHash>

#ifdef WITH_NI
#include <NIDAQmx.h>
#endif

namespace Ni {

#ifndef WITH_NI
// ====================================================================== sans NI
bool compiled() { return false; }
QString driverInfo() { return QStringLiteral("(compile sans NI-DAQmx)"); }
static const char* kNo = "Application compilee SANS NI-DAQmx (refaire cmake avec -DNIBENCH_WITH_NI=ON).";
bool detect(QVector<Dev>&, QString* err) { if (err) *err = kNo; return false; }
bool selfCal(const QString&, QString* err) { if (err) *err = kNo; return false; }
bool AiSession::openVoltage(const QString&, double, double, bool, QString* e) { if (e) *e = kNo; return false; }
bool AiSession::openThermocouple(const QString&, QChar, double, double, double, bool, QString* e) { if (e) *e = kNo; return false; }
bool AiSession::finishOpen(double, bool, QString* e) { if (e) *e = kNo; return false; }
bool AiSession::read(QVector<double>&, QString* e) { if (e) *e = kNo; return false; }
void AiSession::close() {}
bool AoSession::open(const QString&, double, double, QString* e) { if (e) *e = kNo; return false; }
bool AoSession::write(double, QString* e) { if (e) *e = kNo; return false; }
void AoSession::close() {}
bool DigitalSession::openInput(const QStringList&, QString* e) { if (e) *e = kNo; return false; }
bool DigitalSession::openOutput(const QStringList&, QString* e) { if (e) *e = kNo; return false; }
bool DigitalSession::readBits(quint32*, QString* e) { if (e) *e = kNo; return false; }
bool DigitalSession::writeBits(quint32, QString* e) { if (e) *e = kNo; return false; }
void DigitalSession::close() {}
#else
// ====================================================================== avec NI
namespace {

QString ext(int code)
{
    char buf[2048] = {0};
    DAQmxGetExtendedErrorInfo(buf, sizeof(buf));
    QString s = QString::fromLocal8Bit(buf).trimmed();
    if (s.isEmpty()) {
        char b2[1024] = {0};
        DAQmxGetErrorString(code, b2, sizeof(b2));
        s = QString::fromLocal8Bit(b2).trimmed();
    }
    return QStringLiteral("NI-DAQmx erreur %1 : %2").arg(code).arg(s);
}

void fail(int code, QString* err)
{
    if (err) *err = ext(code);
}

#define NI_TRY(call) do { int _e = (call); if (_e < 0) { fail(_e, err); return false; } } while (0)

QStringList splitList(const char* s)
{
    QStringList out;
    for (const QString& p : QString::fromLocal8Bit(s).split(',', Qt::SkipEmptyParts)) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) out << t;
    }
    return out;
}

template <typename F>
QStringList listOf(F f, const QByteArray& dev)
{
    char buf[16384] = {0};
    if (f(dev.constData(), buf, sizeof(buf)) < 0) return {};
    return splitList(buf);
}

const int kErrNotYet = -200284;   // "echantillons pas encore disponibles"
} // namespace

bool compiled() { return true; }
QString driverInfo() { return QStringLiteral("NI-DAQmx (compile avec l'en-tete installe)"); }

bool detect(QVector<Dev>& out, QString* err)
{
    out.clear();
    char names[8192] = {0};
    int e = DAQmxGetSysDevNames(names, sizeof(names));
    if (e < 0) { fail(e, err); return false; }
    for (const QString& n : splitList(names)) {
        Dev d;
        d.name = n;
        const QByteArray nb = n.toLocal8Bit();
        char type[256] = {0};
        if (DAQmxGetDevProductType(nb.constData(), type, sizeof(type)) >= 0) d.model = QString::fromLocal8Bit(type);
        uInt32 sn = 0;
        if (DAQmxGetDevSerialNum(nb.constData(), &sn) >= 0) d.serial = sn;
        d.ai      = listOf(DAQmxGetDevAIPhysicalChans, nb);
        d.ao      = listOf(DAQmxGetDevAOPhysicalChans, nb);
        d.diLines = listOf(DAQmxGetDevDILines, nb);
        d.doLines = listOf(DAQmxGetDevDOLines, nb);
        d.coChans = listOf(DAQmxGetDevCOPhysicalChans, nb);
        out << d;
    }
    return true;
}

bool selfCal(const QString& dev, QString* err)
{
    const QByteArray b = dev.toLocal8Bit();
    NI_TRY(DAQmxSelfCal(b.constData()));
    return true;
}

// ------------------------------------------------------------------ AI
bool AiSession::openVoltage(const QString& chan, double rangeV, double rate, bool hwClock, QString* err)
{
    close();
    TaskHandle t = nullptr;
    NI_TRY(DAQmxCreateTask("", &t));
    m_task = t;
    const QByteArray c = chan.toLocal8Bit();
    int e = DAQmxCreateAIVoltageChan(t, c.constData(), "", DAQmx_Val_Cfg_Default,
                                     -rangeV, rangeV, DAQmx_Val_Volts, nullptr);
    if (e < 0) { fail(e, err); close(); return false; }
    return finishOpen(rate, hwClock, err);
}

bool AiSession::openThermocouple(const QString& chan, QChar type, double minC, double maxC,
                                 double rate, bool hwClock, QString* err)
{
    close();
    int tc = DAQmx_Val_K_Type_TC;
    switch (type.toUpper().unicode()) {
    case 'J': tc = DAQmx_Val_J_Type_TC; break;
    case 'T': tc = DAQmx_Val_T_Type_TC; break;
    case 'E': tc = DAQmx_Val_E_Type_TC; break;
    case 'N': tc = DAQmx_Val_N_Type_TC; break;
    case 'R': tc = DAQmx_Val_R_Type_TC; break;
    case 'S': tc = DAQmx_Val_S_Type_TC; break;
    case 'B': tc = DAQmx_Val_B_Type_TC; break;
    default: break;
    }
    TaskHandle t = nullptr;
    NI_TRY(DAQmxCreateTask("", &t));
    m_task = t;
    const QByteArray c = chan.toLocal8Bit();
    int e = DAQmxCreateAIThrmcplChan(t, c.constData(), "", minC, maxC, DAQmx_Val_DegC,
                                     tc, DAQmx_Val_BuiltIn, 25.0, nullptr);
    if (e < 0) { fail(e, err); close(); return false; }
    return finishOpen(rate, hwClock, err);
}

bool AiSession::finishOpen(double rate, bool hwClock, QString* err)
{
    TaskHandle t = static_cast<TaskHandle>(m_task);
    m_hw = hwClock;
    m_rate = rate;
    if (hwClock) {
        int e = DAQmxCfgSampClkTiming(t, "", rate, DAQmx_Val_Rising, DAQmx_Val_ContSamps,
                                     (uInt64)qMax(1000.0, rate * 2.0));
        if (e < 0) { fail(e, err); close(); return false; }
        float64 real = 0;
        if (DAQmxGetSampClkRate(t, &real) >= 0) m_rate = real;
    }
    int e = DAQmxStartTask(t);
    if (e < 0) { fail(e, err); close(); return false; }
    return true;
}

bool AiSession::read(QVector<double>& out, QString* err)
{
    out.clear();
    if (!m_task) { if (err) *err = "session fermee"; return false; }
    TaskHandle t = static_cast<TaskHandle>(m_task);
    if (!m_hw) {                      // lecture a la demande : 1 valeur
        float64 v = 0;
        int e = DAQmxReadAnalogScalarF64(t, 2.0, &v, nullptr);
        if (e < 0) { fail(e, err); return false; }
        out << v;
        return true;
    }
    static QVector<double> buf(262144);
    int32 n = 0;
    int e = DAQmxReadAnalogF64(t, -1, 0.0, DAQmx_Val_GroupByChannel, buf.data(),
                               (uInt32)buf.size(), &n, nullptr);
    if (e == kErrNotYet) return true;
    if (e < 0) { fail(e, err); return false; }
    out = buf.mid(0, n);
    return true;
}

void AiSession::close()
{
    if (!m_task) return;
    TaskHandle t = static_cast<TaskHandle>(m_task);
    DAQmxStopTask(t);
    DAQmxClearTask(t);
    m_task = nullptr;
}

// ------------------------------------------------------------------ AO
bool AoSession::open(const QString& chan, double minV, double maxV, QString* err)
{
    close();
    TaskHandle t = nullptr;
    NI_TRY(DAQmxCreateTask("", &t));
    m_task = t;
    const QByteArray c = chan.toLocal8Bit();
    int e = DAQmxCreateAOVoltageChan(t, c.constData(), "", minV, maxV, DAQmx_Val_Volts, nullptr);
    if (e < 0) { fail(e, err); close(); return false; }
    return true;
}

bool AoSession::write(double volts, QString* err)
{
    if (!m_task) { if (err) *err = "session fermee"; return false; }
    NI_TRY(DAQmxWriteAnalogScalarF64(static_cast<TaskHandle>(m_task), 1, 2.0, volts, nullptr));
    return true;
}

void AoSession::close()
{
    if (!m_task) return;
    TaskHandle t = static_cast<TaskHandle>(m_task);
    DAQmxStopTask(t);
    DAQmxClearTask(t);
    m_task = nullptr;
}

// ------------------------------------------------------------------ Digital
static QByteArray joinLines(const QStringList& l) { return l.join(",").toLocal8Bit(); }

bool DigitalSession::openInput(const QStringList& lines, QString* err)
{
    close();
    TaskHandle t = nullptr;
    NI_TRY(DAQmxCreateTask("", &t));
    m_task = t;
    int e = DAQmxCreateDIChan(t, joinLines(lines).constData(), "", DAQmx_Val_ChanForAllLines);
    if (e < 0) { fail(e, err); close(); return false; }
    return true;
}

bool DigitalSession::openOutput(const QStringList& lines, QString* err)
{
    close();
    TaskHandle t = nullptr;
    NI_TRY(DAQmxCreateTask("", &t));
    m_task = t;
    int e = DAQmxCreateDOChan(t, joinLines(lines).constData(), "", DAQmx_Val_ChanForAllLines);
    if (e < 0) { fail(e, err); close(); return false; }
    return true;
}

bool DigitalSession::readBits(quint32* bits, QString* err)
{
    if (!m_task) { if (err) *err = "session fermee"; return false; }
    uInt32 v = 0; int32 n = 0;
    NI_TRY(DAQmxReadDigitalU32(static_cast<TaskHandle>(m_task), 1, 2.0, DAQmx_Val_GroupByChannel, &v, 1, &n, nullptr));
    *bits = v;
    return true;
}

bool DigitalSession::writeBits(quint32 bits, QString* err)
{
    if (!m_task) { if (err) *err = "session fermee"; return false; }
    uInt32 v = bits; int32 n = 0;
    NI_TRY(DAQmxWriteDigitalU32(static_cast<TaskHandle>(m_task), 1, 1, 2.0, DAQmx_Val_GroupByChannel, &v, &n, nullptr));
    return true;
}

void DigitalSession::close()
{
    if (!m_task) return;
    TaskHandle t = static_cast<TaskHandle>(m_task);
    DAQmxStopTask(t);
    DAQmxClearTask(t);
    m_task = nullptr;
}
#endif

} // namespace Ni
