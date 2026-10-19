#pragma once
#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include "NiApi.h"
#include "Widgets.h"

class QPushButton; class QLabel; class QComboBox; class QDoubleSpinBox; class QCheckBox;
class QTableWidget; class QPlainTextEdit; class QTabWidget; class QLineEdit; class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // connexion
    void onConnect();
    void onSelfCal();
    void copyReport();
    void saveReport();
    // analogique
    void onAiDeviceChanged();
    void onAiModeChanged();
    void onAiStartStop();
    void onAiPoll();
    void onAiPreset4353();
    void onAiPreset4480();
    // numerique
    void onDioDeviceChanged();
    void onDiToggle();
    void onDiPoll();
    void onDoOpen();
    void onDoBitToggled();
    void onBlinkTick();
    // analogique sortie
    void onAoWrite();
    void onAoZero();
    // pilotes
    void onProbe(int kind, bool open);

private:
    void log(const QString& text, bool error = false);
    QString buildReport() const;
    QWidget* buildConnectTab();
    QWidget* buildAiTab();
    QWidget* buildDioTab();
    QWidget* buildAoTab();
    QWidget* buildDriversTab();
    const Ni::Dev* devByName(const QString& n) const;
    bool outputsAllowed();
    void refreshCombos();
    void stopAi();

    // etat
    QVector<Ni::Dev> m_devs;
    // haut
    QCheckBox* m_allowOut = nullptr;
    Led* m_ledConn = nullptr;
    QLabel* m_lblConn = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QTabWidget* m_tabs = nullptr;
    // connexion
    QTableWidget* m_table = nullptr;
    Led* m_ledExp[4] = {nullptr, nullptr, nullptr, nullptr};
    // ai
    QComboBox *m_aiDev = nullptr, *m_aiChan = nullptr, *m_aiMode = nullptr, *m_aiTc = nullptr, *m_aiRange = nullptr, *m_aiView = nullptr;
    QDoubleSpinBox *m_aiRate = nullptr, *m_aiMin = nullptr, *m_aiMax = nullptr;
    QCheckBox* m_aiHw = nullptr;
    QPushButton* m_aiBtn = nullptr;
    QLabel *m_aiBig = nullptr, *m_aiStats = nullptr, *m_aiInfo = nullptr;
    Led* m_aiSat = nullptr;
    PlotWidget* m_plot = nullptr;
    Ni::AiSession m_ai;
    QTimer m_aiTimer;
    double m_aiRangeV = 10.0;
    bool m_aiIsTc = false;
    int m_aiErrCount = 0;
    // dio
    QComboBox *m_dioDev = nullptr, *m_diPort = nullptr, *m_doPort = nullptr;
    QPushButton *m_diBtn = nullptr, *m_doOpenBtn = nullptr;
    Led* m_diLeds[8] = {};
    QPushButton* m_doBits[8] = {};
    QCheckBox* m_blink = nullptr;
    QDoubleSpinBox* m_blinkHz = nullptr;
    Ni::DigitalSession m_di, m_do;
    QTimer m_diTimer, m_blinkTimer;
    quint32 m_doState = 0;
    QStringList m_diLines, m_doLines;
    // ao
    QComboBox *m_aoDev = nullptr, *m_aoChan = nullptr;
    QDoubleSpinBox* m_aoVal = nullptr;
    QLabel* m_aoState = nullptr;
    Ni::AoSession m_ao;
    // pilotes
    QComboBox* m_rsrc[3] = {};
    QPlainTextEdit* m_drvOut = nullptr;
};
