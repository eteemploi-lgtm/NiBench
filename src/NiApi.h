#pragma once
// NiApi : fine couche autour de NI-DAQmx pour le banc d'essai. Aucune logique metier.
#include <QString>
#include <QStringList>
#include <QVector>
#include <QChar>

namespace Ni {

bool compiled();                         // true si l'application a ete compilee avec NI-DAQmx
QString driverInfo();                    // version du pilote si disponible

struct Dev {
    QString name;        // "Dev1", "PXI1Slot3"...
    QString model;       // "PXIe-4353"
    quint32 serial = 0;
    QStringList ai, ao, diLines, doLines, coChans;
};

// Detecte les cartes vues par NI-DAQmx. Renvoie false + err si le pilote est en erreur.
bool detect(QVector<Dev>& out, QString* err);
bool selfCal(const QString& dev, QString* err);

// ------------------------------------------------------------ Entree analogique (V ou thermocouple)
class AiSession {
public:
    ~AiSession() { close(); }
    bool openVoltage(const QString& chan, double rangeV, double rate, bool hwClock, QString* err);
    bool openThermocouple(const QString& chan, QChar type, double minC, double maxC,
                          double rate, bool hwClock, QString* err);
    bool read(QVector<double>& out, QString* err);   // lit ce qui est disponible (peut etre vide)
    double actualRate() const { return m_rate; }
    bool isOpen() const { return m_task != nullptr; }
    void close();
private:
    bool finishOpen(double rate, bool hwClock, QString* err);
    void* m_task = nullptr;
    double m_rate = 0.0;
    bool m_hw = true;
};

// ------------------------------------------------------------ Sortie analogique
class AoSession {
public:
    ~AoSession() { close(); }
    bool open(const QString& chan, double minV, double maxV, QString* err);
    bool write(double volts, QString* err);
    bool isOpen() const { return m_task != nullptr; }
    void close();
private:
    void* m_task = nullptr;
};

// ------------------------------------------------------------ Lignes numeriques (un mot de bits)
class DigitalSession {
public:
    ~DigitalSession() { close(); }
    bool openInput(const QStringList& lines, QString* err);
    bool openOutput(const QStringList& lines, QString* err);
    bool readBits(quint32* bits, QString* err);
    bool writeBits(quint32 bits, QString* err);
    bool isOpen() const { return m_task != nullptr; }
    void close();
private:
    void* m_task = nullptr;
};

} // namespace Ni
