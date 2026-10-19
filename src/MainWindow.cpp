#include "MainWindow.h"
#include "DriverProbe.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <cmath>

namespace {
QString portOf(const QString& line)
{
    int i = line.lastIndexOf("/line");
    return i > 0 ? line.left(i) : line;
}
QStringList portsOf(const QStringList& lines)
{
    QStringList p;
    for (const QString& l : lines) { const QString x = portOf(l); if (!p.contains(x)) p << x; }
    return p;
}
QStringList linesOfPort(const QStringList& lines, const QString& port, int maxN = 8)
{
    QStringList o;
    for (const QString& l : lines) if (portOf(l) == port && o.size() < maxN) o << l;
    return o;
}
QString roleOf(const QString& model)
{
    if (model.contains("4353")) return "Temperatures (thermocouples)";
    if (model.contains("4480")) return "Hydrophones (acoustique)";
    if (model.contains("6528")) return "Entrees/sorties numeriques";
    if (model.contains("6570") || model.contains("6537")) return "Generation MLI";
    if (model.contains("5108")) return "Oscilloscope";
    if (model.contains("8301") || model.contains("1092")) return "Chassis / liaison";
    return "";
}
QPushButton* bigButton(const QString& t, const QString& color)
{
    auto* b = new QPushButton(t);
    b->setMinimumHeight(38);
    b->setStyleSheet(QString("QPushButton{background:%1;color:white;font-weight:bold;font-size:14px;"
                             "border-radius:6px;padding:4px 18px;}QPushButton:disabled{background:#999;}").arg(color));
    return b;
}
} // namespace

// =========================================================================================
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("NiBench - banc d'essai NI (test de communication)");
    qApp->setStyle("Fusion");

    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);

    // ---- barre du haut : connexion + autorisation des sorties
    auto* top = new QHBoxLayout;
    auto* btnConn = bigButton("SE CONNECTER  (detecter les cartes)", "#1f7a4d");
    connect(btnConn, &QPushButton::clicked, this, &MainWindow::onConnect);
    m_ledConn = new Led;
    m_lblConn = new QLabel("Pas encore connecte.");
    m_lblConn->setWordWrap(true);
    m_allowOut = new QCheckBox("Autoriser les SORTIES (ecriture sur le materiel)");
    m_allowOut->setStyleSheet("QCheckBox{color:#b03030;font-weight:bold;}");
    m_allowOut->setToolTip("Tant que cette case est decochee, aucun onglet ne peut ecrire sur une sortie.");
    top->addWidget(btnConn);
    top->addWidget(m_ledConn);
    top->addWidget(m_lblConn, 1);
    top->addWidget(m_allowOut);
    root->addLayout(top);

    m_tabs = new QTabWidget;
    m_tabs->addTab(buildConnectTab(), "1. Cartes");
    m_tabs->addTab(buildAiTab(),      "2. Ecoute (entrees analogiques)");
    m_tabs->addTab(buildDioTab(),     "3. Numerique (6528)");
    m_tabs->addTab(buildAoTab(),      "4. Sortie analogique");
    m_tabs->addTab(buildDriversTab(), "5. Pilotes 5108 / 6570 / 6537");

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setStyleSheet("QPlainTextEdit{font-family:Consolas,monospace;font-size:11px;}");
    auto* logBox = new QWidget;
    auto* lv = new QVBoxLayout(logBox);
    lv->setContentsMargins(0, 0, 0, 0);
    auto* lh = new QHBoxLayout;
    lh->addWidget(new QLabel("<b>Journal</b> (a copier / m'envoyer)"));
    lh->addStretch();
    auto* bCopy = new QPushButton("Copier le rapport complet");
    auto* bSave = new QPushButton("Enregistrer le rapport");
    auto* bClr  = new QPushButton("Effacer");
    connect(bCopy, &QPushButton::clicked, this, &MainWindow::copyReport);
    connect(bSave, &QPushButton::clicked, this, &MainWindow::saveReport);
    connect(bClr, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    lh->addWidget(bCopy); lh->addWidget(bSave); lh->addWidget(bClr);
    lv->addLayout(lh);
    lv->addWidget(m_log);

    auto* split = new QSplitter(Qt::Vertical);
    split->addWidget(m_tabs);
    split->addWidget(logBox);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);
    setCentralWidget(central);

    connect(&m_aiTimer, &QTimer::timeout, this, &MainWindow::onAiPoll);
    connect(&m_diTimer, &QTimer::timeout, this, &MainWindow::onDiPoll);
    connect(&m_blinkTimer, &QTimer::timeout, this, &MainWindow::onBlinkTick);

    log(QString("NiBench demarre. %1").arg(Ni::driverInfo()));
    if (!Ni::compiled()) {
        m_ledConn->setState(2);
        m_lblConn->setText("Compile SANS NI-DAQmx : refaire cmake avec -DNIBENCH_WITH_NI=ON (voir README).");
        log("Application compilee SANS NI-DAQmx.", true);
    } else {
        log("Clique sur SE CONNECTER pour detecter les cartes.");
    }
}

MainWindow::~MainWindow()
{
    m_aiTimer.stop(); m_diTimer.stop(); m_blinkTimer.stop();
    m_ai.close(); m_di.close(); m_do.close(); m_ao.close();
}

void MainWindow::log(const QString& text, bool error)
{
    m_log->appendPlainText(QString("[%1] %2%3")
                               .arg(QTime::currentTime().toString("HH:mm:ss"), error ? "ERREUR  " : "", text));
}

const Ni::Dev* MainWindow::devByName(const QString& n) const
{
    for (const auto& d : m_devs) if (d.name == n) return &d;
    return nullptr;
}

bool MainWindow::outputsAllowed()
{
    if (m_allowOut->isChecked()) return true;
    QMessageBox::warning(this, "Sorties verrouillees",
        "Coche d'abord « Autoriser les SORTIES » en haut de la fenetre.\n"
        "Verifie que rien de dangereux n'est branche sur la sortie (amplificateur debranche du transducteur).");
    log("Ecriture refusee : sorties non autorisees (case du haut decochee).", true);
    return false;
}

// =========================================================================================
// Onglet 1 : cartes
QWidget* MainWindow::buildConnectTab()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);

    auto* exp = new QGroupBox("Cartes attendues (vues par NI-DAQmx)");
    auto* eg = new QGridLayout(exp);
    const char* names[4] = {"PXIe-4353 (temperatures)", "PXIe-4480 (hydrophones)", "PXI-6528 (E/S numeriques)",
                            "6570 / 6537 (MLI) - souvent invisible pour DAQmx : voir onglet 5"};
    for (int i = 0; i < 4; ++i) {
        m_ledExp[i] = new Led;
        eg->addWidget(m_ledExp[i], i, 0);
        eg->addWidget(new QLabel(names[i]), i, 1);
    }
    eg->setColumnStretch(1, 1);
    v->addWidget(exp);

    m_table = new QTableWidget(0, 8);
    m_table->setHorizontalHeaderLabels({"Nom (NI MAX)", "Modele", "N° serie", "Role probable",
                                        "Entrees analog.", "Sorties analog.", "Lignes DI", "Lignes DO"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_table, 1);

    auto* h = new QHBoxLayout;
    auto* cal = new QPushButton("Auto-calibrer la carte selectionnee");
    cal->setToolTip("DAQmxSelfCal : quelques secondes, l'interface se fige pendant ce temps.");
    connect(cal, &QPushButton::clicked, this, &MainWindow::onSelfCal);
    h->addWidget(cal);
    h->addStretch();
    v->addLayout(h);
    return w;
}

void MainWindow::onConnect()
{
    m_aiTimer.stop(); m_diTimer.stop(); m_blinkTimer.stop();
    stopAi(); m_di.close(); m_do.close(); m_ao.close();
    m_diBtn->setText("Demarrer la lecture"); m_doOpenBtn->setText("Ouvrir la sortie");
    for (auto* b : m_doBits) b->setEnabled(false);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    QVector<Ni::Dev> found;
    bool ok = Ni::detect(found, &err);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        m_ledConn->setState(2);
        m_lblConn->setText("Echec de la detection : " + err);
        log("Detection : " + err, true);
        return;
    }
    m_devs = found;
    m_table->setRowCount(0);
    bool has[4] = {false, false, false, false};
    for (const auto& d : m_devs) {
        int r = m_table->rowCount();
        m_table->insertRow(r);
        const QStringList cells = {d.name, d.model, d.serial ? QString::number(d.serial, 16).toUpper() : "", roleOf(d.model),
                                   QString::number(d.ai.size()), QString::number(d.ao.size()),
                                   QString::number(d.diLines.size()), QString::number(d.doLines.size())};
        for (int c = 0; c < cells.size(); ++c) m_table->setItem(r, c, new QTableWidgetItem(cells[c]));
        if (d.model.contains("4353")) has[0] = true;
        if (d.model.contains("4480")) has[1] = true;
        if (d.model.contains("6528")) has[2] = true;
        if (d.model.contains("6570") || d.model.contains("6537")) has[3] = true;
        log(QString("Carte %1 : %2  (serie %3)  AI=%4 AO=%5 DI=%6 DO=%7")
                .arg(d.name, d.model).arg(d.serial, 0, 16).arg(d.ai.size()).arg(d.ao.size())
                .arg(d.diLines.size()).arg(d.doLines.size()));
    }
    for (int i = 0; i < 4; ++i) m_ledExp[i]->setState(has[i] ? 1 : (i == 3 ? 0 : 2));

    if (m_devs.isEmpty()) {
        m_ledConn->setState(3);
        m_lblConn->setText("Pilote OK mais aucune carte vue. Verifie : chassis allume, cable Thunderbolt, NI MAX.");
        log("Aucune carte NI detectee.", true);
    } else {
        m_ledConn->setState(1);
        m_lblConn->setText(QString("Connecte : %1 carte(s) detectee(s).").arg(m_devs.size()));
        log(QString("Connecte : %1 carte(s).").arg(m_devs.size()));
    }
    refreshCombos();
}

void MainWindow::onSelfCal()
{
    const int r = m_table->currentRow();
    if (r < 0) { QMessageBox::information(this, "Auto-calibration", "Selectionne d'abord une carte dans le tableau."); return; }
    const QString dev = m_table->item(r, 0)->text();
    stopAi();
    log(QString("Auto-calibration de %1 en cours...").arg(dev));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();
    QString err;
    bool ok = Ni::selfCal(dev, &err);
    QApplication::restoreOverrideCursor();
    if (ok) log(QString("Auto-calibration de %1 terminee : OK").arg(dev));
    else log(QString("Auto-calibration de %1 : %2").arg(dev, err), true);
}

void MainWindow::refreshCombos()
{
    auto fill = [](QComboBox* c, const QStringList& items) {
        const QString cur = c->currentText();
        c->blockSignals(true);
        c->clear(); c->addItems(items);
        int i = c->findText(cur); if (i >= 0) c->setCurrentIndex(i);
        c->blockSignals(false);
    };
    QStringList ai, dio, ao, all;
    for (const auto& d : m_devs) {
        const QString t = d.name + "  (" + d.model + ")";
        all << d.name;
        if (!d.ai.isEmpty()) ai << d.name;
        if (!d.diLines.isEmpty() || !d.doLines.isEmpty()) dio << d.name;
        if (!d.ao.isEmpty()) ao << d.name;
    }
    fill(m_aiDev, ai); fill(m_dioDev, dio); fill(m_aoDev, ao);
    for (auto* c : m_rsrc) { const QString cur = c->currentText(); c->clear(); c->addItems(all); c->setEditText(cur); }
    onAiDeviceChanged(); onDioDeviceChanged();
    // voies AO
    m_aoChan->clear();
    if (const auto* d = devByName(m_aoDev->currentText())) m_aoChan->addItems(d->ao);
}

// =========================================================================================
// Onglet 2 : ecoute / entrees analogiques
QWidget* MainWindow::buildAiTab()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);

    auto* pre = new QHBoxLayout;
    auto* p1 = new QPushButton("Preset 4353 : thermocouple K, 10 Hz");
    auto* p2 = new QPushButton("Preset 4480 : tension ±10 V, 96 kHz");
    connect(p1, &QPushButton::clicked, this, &MainWindow::onAiPreset4353);
    connect(p2, &QPushButton::clicked, this, &MainWindow::onAiPreset4480);
    pre->addWidget(new QLabel("Raccourcis :")); pre->addWidget(p1); pre->addWidget(p2); pre->addStretch();
    v->addLayout(pre);

    auto* g = new QGroupBox("Reglages");
    auto* f = new QGridLayout(g);
    m_aiDev = new QComboBox; m_aiChan = new QComboBox;
    m_aiMode = new QComboBox; m_aiMode->addItems({"Tension (V)", "Thermocouple (°C)"});
    m_aiTc = new QComboBox; m_aiTc->addItems({"K", "J", "T", "E", "N", "R", "S", "B"});
    m_aiRange = new QComboBox;
    for (double r : {0.1, 0.316, 1.0, 3.16, 10.0, 42.4}) m_aiRange->addItem(QString("±%1 V").arg(r), r);
    m_aiRange->setCurrentIndex(4);
    m_aiMin = new QDoubleSpinBox; m_aiMin->setRange(-300, 2000); m_aiMin->setValue(-50);
    m_aiMax = new QDoubleSpinBox; m_aiMax->setRange(-300, 2000); m_aiMax->setValue(400);
    m_aiRate = new QDoubleSpinBox; m_aiRate->setRange(0.1, 204800); m_aiRate->setDecimals(1); m_aiRate->setValue(96000);
    m_aiRate->setSuffix(" Hz");
    m_aiHw = new QCheckBox("Horloge materielle (sinon lecture a la demande, 1 valeur/lecture)"); m_aiHw->setChecked(true);
    m_aiView = new QComboBox; m_aiView->addItems({"Temps", "Spectre (FFT)"});
    connect(m_aiView, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i) { m_plot->setMode(i == 1 ? PlotWidget::Spectrum : PlotWidget::Time); });
    f->addWidget(new QLabel("Carte"), 0, 0);      f->addWidget(m_aiDev, 0, 1);
    f->addWidget(new QLabel("Voie"), 0, 2);       f->addWidget(m_aiChan, 0, 3);
    f->addWidget(new QLabel("Mesure"), 1, 0);     f->addWidget(m_aiMode, 1, 1);
    f->addWidget(new QLabel("Type TC"), 1, 2);    f->addWidget(m_aiTc, 1, 3);
    f->addWidget(new QLabel("Plage tension"), 2, 0); f->addWidget(m_aiRange, 2, 1);
    f->addWidget(new QLabel("TC min / max (°C)"), 2, 2);
    auto* mm = new QHBoxLayout; mm->addWidget(m_aiMin); mm->addWidget(m_aiMax); f->addLayout(mm, 2, 3);
    f->addWidget(new QLabel("Frequence"), 3, 0);  f->addWidget(m_aiRate, 3, 1);
    f->addWidget(new QLabel("Affichage"), 3, 2);  f->addWidget(m_aiView, 3, 3);
    f->addWidget(m_aiHw, 4, 0, 1, 4);
    v->addWidget(g);

    connect(m_aiDev, &QComboBox::currentTextChanged, this, &MainWindow::onAiDeviceChanged);
    connect(m_aiMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAiModeChanged);

    auto* h = new QHBoxLayout;
    m_aiBtn = bigButton("DEMARRER L'ECOUTE", "#1f5fa8");
    connect(m_aiBtn, &QPushButton::clicked, this, &MainWindow::onAiStartStop);
    m_aiBig = new QLabel("---");
    m_aiBig->setStyleSheet("font-size:30px;font-weight:bold;color:#1f5fa8;");
    m_aiSat = new Led;
    h->addWidget(m_aiBtn); h->addSpacing(16); h->addWidget(m_aiBig); h->addStretch();
    h->addWidget(new QLabel("Saturation / valeur invalide :")); h->addWidget(m_aiSat);
    v->addLayout(h);

    m_plot = new PlotWidget;
    v->addWidget(m_plot, 1);
    m_aiStats = new QLabel("");
    m_aiInfo = new QLabel("");
    m_aiInfo->setStyleSheet("color:#555;");
    v->addWidget(m_aiStats);
    v->addWidget(m_aiInfo);
    onAiModeChanged();
    return w;
}

void MainWindow::onAiDeviceChanged()
{
    m_aiChan->clear();
    if (const auto* d = devByName(m_aiDev->currentText())) m_aiChan->addItems(d->ai);
}

void MainWindow::onAiModeChanged()
{
    const bool tc = m_aiMode->currentIndex() == 1;
    m_aiTc->setEnabled(tc); m_aiMin->setEnabled(tc); m_aiMax->setEnabled(tc);
    m_aiRange->setEnabled(!tc);
    m_aiRate->setValue(tc ? 10.0 : 96000.0);
}

void MainWindow::onAiPreset4353()
{
    for (const auto& d : m_devs) if (d.model.contains("4353")) {
        int i = m_aiDev->findText(d.name); if (i >= 0) m_aiDev->setCurrentIndex(i);
        m_aiMode->setCurrentIndex(1); m_aiTc->setCurrentText("K"); m_aiHw->setChecked(true);
        m_aiRate->setValue(10.0);
        return;
    }
    log("Preset 4353 : aucune PXIe-4353 detectee.", true);
}

void MainWindow::onAiPreset4480()
{
    for (const auto& d : m_devs) if (d.model.contains("4480")) {
        int i = m_aiDev->findText(d.name); if (i >= 0) m_aiDev->setCurrentIndex(i);
        m_aiMode->setCurrentIndex(0); m_aiRange->setCurrentIndex(4); m_aiHw->setChecked(true);
        m_aiRate->setValue(96000.0);
        return;
    }
    log("Preset 4480 : aucune PXIe-4480 detectee.", true);
}

void MainWindow::stopAi()
{
    m_aiTimer.stop();
    m_ai.close();
    if (m_aiBtn) m_aiBtn->setText("DEMARRER L'ECOUTE");
}

void MainWindow::onAiStartStop()
{
    if (m_ai.isOpen()) { stopAi(); log("Ecoute arretee."); return; }
    const QString chan = m_aiChan->currentText();
    if (chan.isEmpty()) { QMessageBox::information(this, "Ecoute", "Aucune voie : clique d'abord sur SE CONNECTER."); return; }

    QString err;
    const double rate = m_aiRate->value();
    const bool hw = m_aiHw->isChecked();
    m_aiIsTc = m_aiMode->currentIndex() == 1;
    bool ok;
    if (m_aiIsTc) {
        ok = m_ai.openThermocouple(chan, m_aiTc->currentText().at(0), m_aiMin->value(), m_aiMax->value(), rate, hw, &err);
        m_plot->setUnit("°C");
    } else {
        m_aiRangeV = m_aiRange->currentData().toDouble();
        ok = m_ai.openVoltage(chan, m_aiRangeV, rate, hw, &err);
        m_plot->setUnit("V");
    }
    if (!ok) { log(QString("Ouverture de %1 : %2").arg(chan, err), true); return; }

    const double fs = hw ? m_ai.actualRate() : 2.0;
    m_plot->clear();
    m_plot->setSampleRate(fs);
    m_plot->setHistory(fs <= 100 ? int(fs * 60) : 8192);
    m_aiErrCount = 0;
    m_aiTimer.start(hw ? 50 : 500);
    m_aiBtn->setText("ARRETER L'ECOUTE");
    log(QString("Ecoute %1 : %2, demande %3 Hz, reel %4 Hz%5")
            .arg(chan, m_aiIsTc ? QString("thermocouple %1").arg(m_aiTc->currentText())
                                : QString("tension ±%1 V").arg(m_aiRangeV))
            .arg(rate).arg(m_ai.actualRate()).arg(hw ? "" : " (a la demande)"));
}

void MainWindow::onAiPoll()
{
    QVector<double> blk; QString err;
    if (!m_ai.read(blk, &err)) {
        log(err, true);
        if (++m_aiErrCount >= 3) { stopAi(); log("Ecoute arretee apres 3 erreurs.", true); m_aiSat->setState(2); }
        return;
    }
    if (blk.isEmpty()) return;
    m_aiErrCount = 0;
    m_plot->append(blk);

    double mn = blk[0], mx = blk[0], s2 = 0, pk = 0; bool invalid = false;
    for (double x : blk) {
        if (!std::isfinite(x) || std::fabs(x) > 1e9) { invalid = true; continue; }
        mn = qMin(mn, x); mx = qMax(mx, x); s2 += x * x; pk = qMax(pk, std::fabs(x));
    }
    const double rms = std::sqrt(s2 / blk.size());
    const double last = blk.last();
    if (m_aiIsTc) {
        m_aiBig->setText(invalid ? "invalide" : QString("%1 °C").arg(last, 0, 'f', 2));
        m_aiSat->setState(invalid ? 2 : 1);
        m_aiStats->setText(QString("Derniere : %1 °C   min %2   max %3   (%4 echantillon(s) dans ce bloc)")
                               .arg(last, 0, 'f', 2).arg(mn, 0, 'f', 2).arg(mx, 0, 'f', 2).arg(blk.size()));
        if (invalid) m_aiInfo->setText("Valeur invalide : thermocouple debranche ou type/plage incorrect ?");
        else m_aiInfo->setText("");
    } else {
        const bool sat = pk >= 0.95 * m_aiRangeV;
        m_aiSat->setState(invalid ? 2 : (sat ? 3 : 1));
        m_aiBig->setText(QString("%1 mV rms").arg(rms * 1000.0, 0, 'f', 3));
        m_aiStats->setText(QString("RMS %1 V (%2 dBV)   pic %3 V   min %4   max %5   bloc de %6 echantillons")
                               .arg(rms, 0, 'g', 5).arg(20 * std::log10(qMax(rms, 1e-12)), 0, 'f', 1)
                               .arg(pk, 0, 'g', 4).arg(mn, 0, 'g', 4).arg(mx, 0, 'g', 4).arg(blk.size()));
        QString info = sat ? "SATURATION : pic superieur a 95 % de la plage. " : "";
        if (m_aiView->currentIndex() == 1) {
            const QVector<double> sp = PlotWidget::spectrumDbV(m_plot->data(), 4096);
            if (!sp.isEmpty()) {
                int kmax = 1;
                for (int k = 2; k < sp.size(); ++k) if (sp[k] > sp[kmax]) kmax = k;
                info += QString("Pic spectral : %1 Hz a %2 dBV").arg(kmax * m_ai.actualRate() / 4096.0, 0, 'f', 1)
                            .arg(sp[kmax], 0, 'f', 1);
            }
        }
        m_aiInfo->setText(info);
    }
}

// =========================================================================================
// Onglet 3 : numerique
QWidget* MainWindow::buildDioTab()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);

    auto* top = new QHBoxLayout;
    m_dioDev = new QComboBox;
    top->addWidget(new QLabel("Carte :")); top->addWidget(m_dioDev); top->addStretch();
    v->addLayout(top);
    connect(m_dioDev, &QComboBox::currentTextChanged, this, &MainWindow::onDioDeviceChanged);

    // entrees
    auto* gi = new QGroupBox("ENTREES : ecoute des lignes (lecture seule, sans risque)");
    auto* li = new QHBoxLayout(gi);
    m_diPort = new QComboBox;
    m_diBtn = bigButton("Demarrer la lecture", "#1f5fa8");
    connect(m_diBtn, &QPushButton::clicked, this, &MainWindow::onDiToggle);
    li->addWidget(new QLabel("Port :")); li->addWidget(m_diPort); li->addWidget(m_diBtn); li->addSpacing(12);
    for (int i = 0; i < 8; ++i) {
        auto* col = new QVBoxLayout;
        m_diLeds[i] = new Led;
        col->addWidget(m_diLeds[i], 0, Qt::AlignHCenter);
        col->addWidget(new QLabel(QString::number(i)), 0, Qt::AlignHCenter);
        li->addLayout(col);
    }
    li->addStretch();
    v->addWidget(gi);

    // sorties
    auto* go = new QGroupBox("SORTIES : pilotage des lignes (case « Autoriser les SORTIES » obligatoire)");
    auto* vo = new QVBoxLayout(go);
    auto* lo = new QHBoxLayout;
    m_doPort = new QComboBox;
    m_doOpenBtn = bigButton("Ouvrir la sortie", "#b8651b");
    connect(m_doOpenBtn, &QPushButton::clicked, this, &MainWindow::onDoOpen);
    lo->addWidget(new QLabel("Port :")); lo->addWidget(m_doPort); lo->addWidget(m_doOpenBtn); lo->addStretch();
    vo->addLayout(lo);
    auto* bits = new QHBoxLayout;
    for (int i = 0; i < 8; ++i) {
        m_doBits[i] = new QPushButton(QString("L%1").arg(i));
        m_doBits[i]->setCheckable(true);
        m_doBits[i]->setEnabled(false);
        m_doBits[i]->setMinimumSize(54, 44);
        m_doBits[i]->setStyleSheet("QPushButton:checked{background:#2e9e4f;color:white;font-weight:bold;}");
        connect(m_doBits[i], &QPushButton::toggled, this, &MainWindow::onDoBitToggled);
        bits->addWidget(m_doBits[i]);
    }
    bits->addStretch();
    vo->addLayout(bits);
    auto* bl = new QHBoxLayout;
    m_blink = new QCheckBox("Faire clignoter L0 (signal carre logiciel)");
    m_blinkHz = new QDoubleSpinBox; m_blinkHz->setRange(0.2, 5.0); m_blinkHz->setValue(1.0); m_blinkHz->setSuffix(" Hz");
    connect(m_blink, &QCheckBox::toggled, this, [this](bool on) {
        if (on && m_do.isOpen()) m_blinkTimer.start(int(500.0 / m_blinkHz->value()));
        else m_blinkTimer.stop();
    });
    bl->addWidget(m_blink); bl->addWidget(m_blinkHz); bl->addStretch();
    vo->addLayout(bl);
    vo->addWidget(new QLabel("Astuce : relie une ligne de sortie a une ligne d'entree (ou un voyant / multimetre) "
                             "pour voir le signal revenir dans la partie ENTREES."));
    v->addWidget(go);
    v->addStretch();
    return w;
}

void MainWindow::onDioDeviceChanged()
{
    m_diPort->clear(); m_doPort->clear();
    if (const auto* d = devByName(m_dioDev->currentText())) {
        m_diPort->addItems(portsOf(d->diLines));
        m_doPort->addItems(portsOf(d->doLines));
    }
}

void MainWindow::onDiToggle()
{
    if (m_di.isOpen()) {
        m_diTimer.stop(); m_di.close(); m_diBtn->setText("Demarrer la lecture");
        for (auto* l : m_diLeds) l->setState(0);
        log("Lecture des entrees numeriques arretee.");
        return;
    }
    const auto* d = devByName(m_dioDev->currentText());
    if (!d) return;
    m_diLines = linesOfPort(d->diLines, m_diPort->currentText());
    if (m_diLines.isEmpty()) { log("Aucune ligne d'entree sur ce port.", true); return; }
    QString err;
    if (!m_di.openInput(m_diLines, &err)) { log("Ouverture entrees : " + err, true); return; }
    m_diBtn->setText("Arreter la lecture");
    m_diTimer.start(100);
    log(QString("Lecture de %1 ligne(s) : %2 ... %3").arg(m_diLines.size()).arg(m_diLines.first(), m_diLines.last()));
}

void MainWindow::onDiPoll()
{
    quint32 bits = 0; QString err;
    if (!m_di.readBits(&bits, &err)) {
        log(err, true); m_diTimer.stop(); m_di.close(); m_diBtn->setText("Demarrer la lecture"); return;
    }
    for (int i = 0; i < 8; ++i)
        m_diLeds[i]->setState(i < m_diLines.size() ? ((bits >> i) & 1 ? 1 : 2) : 0);
}

void MainWindow::onDoOpen()
{
    if (m_do.isOpen()) {
        m_blinkTimer.stop(); m_do.close();
        m_doOpenBtn->setText("Ouvrir la sortie");
        for (auto* b : m_doBits) { b->blockSignals(true); b->setChecked(false); b->blockSignals(false); b->setEnabled(false); }
        log("Sortie numerique fermee (retour a l'etat par defaut de la carte).");
        return;
    }
    if (!outputsAllowed()) return;
    const auto* d = devByName(m_dioDev->currentText());
    if (!d) return;
    m_doLines = linesOfPort(d->doLines, m_doPort->currentText());
    if (m_doLines.isEmpty()) { log("Aucune ligne de sortie sur ce port.", true); return; }
    QString err;
    if (!m_do.openOutput(m_doLines, &err)) { log("Ouverture sorties : " + err, true); return; }
    m_doState = 0;
    if (!m_do.writeBits(0, &err)) { log("Ecriture initiale : " + err, true); m_do.close(); return; }
    for (int i = 0; i < 8; ++i) m_doBits[i]->setEnabled(i < m_doLines.size());
    m_doOpenBtn->setText("Fermer la sortie");
    log(QString("Sortie ouverte sur %1 ligne(s), toutes a 0.").arg(m_doLines.size()));
    if (m_blink->isChecked()) m_blinkTimer.start(int(500.0 / m_blinkHz->value()));
}

void MainWindow::onDoBitToggled()
{
    if (!m_do.isOpen()) return;
    quint32 s = 0;
    for (int i = 0; i < 8; ++i) if (m_doBits[i]->isChecked()) s |= (1u << i);
    QString err;
    if (!m_do.writeBits(s, &err)) { log("Ecriture : " + err, true); m_blinkTimer.stop(); return; }
    if (!m_blinkTimer.isActive() || (s ^ m_doState) & ~1u)
        log(QString("Sortie ecrite : 0x%1").arg(s, 2, 16, QChar('0')));
    m_doState = s;
}

void MainWindow::onBlinkTick()
{
    if (!m_do.isOpen()) { m_blinkTimer.stop(); return; }
    m_doBits[0]->setChecked(!m_doBits[0]->isChecked());
}

// =========================================================================================
// Onglet 4 : sortie analogique
QWidget* MainWindow::buildAoTab()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->addWidget(new QLabel("Sortie en tension continue (DC), sur une carte qui possede des sorties analogiques.\n"
                            "Dans ta configuration actuelle (4353, 4480, 6528, 5108, 6537/6570) il est possible qu'aucune "
                            "carte n'en ait : l'onglet restera alors vide, c'est normal."));
    auto* f = new QFormLayout;
    m_aoDev = new QComboBox; m_aoChan = new QComboBox;
    m_aoVal = new QDoubleSpinBox; m_aoVal->setRange(-10, 10); m_aoVal->setDecimals(3); m_aoVal->setSingleStep(0.1); m_aoVal->setSuffix(" V");
    connect(m_aoDev, &QComboBox::currentTextChanged, this, [this](const QString& n) {
        m_aoChan->clear();
        if (const auto* d = devByName(n)) m_aoChan->addItems(d->ao);
    });
    f->addRow("Carte", m_aoDev); f->addRow("Voie", m_aoChan); f->addRow("Tension", m_aoVal);
    v->addLayout(f);
    auto* h = new QHBoxLayout;
    auto* bw = bigButton("ECRIRE LA TENSION", "#b8651b");
    auto* bz = new QPushButton("Remettre a 0 V et fermer");
    connect(bw, &QPushButton::clicked, this, &MainWindow::onAoWrite);
    connect(bz, &QPushButton::clicked, this, &MainWindow::onAoZero);
    h->addWidget(bw); h->addWidget(bz); h->addStretch();
    v->addLayout(h);
    m_aoState = new QLabel("Sortie fermee.");
    v->addWidget(m_aoState);
    v->addStretch();
    return w;
}

void MainWindow::onAoWrite()
{
    if (!outputsAllowed()) return;
    const QString chan = m_aoChan->currentText();
    if (chan.isEmpty()) { log("Aucune voie de sortie analogique (aucune carte AO).", true); return; }
    QString err;
    if (!m_ao.isOpen() && !m_ao.open(chan, -10, 10, &err)) { log("Ouverture AO : " + err, true); return; }
    if (!m_ao.write(m_aoVal->value(), &err)) { log("Ecriture AO : " + err, true); return; }
    m_aoState->setText(QString("%1 = %2 V").arg(chan).arg(m_aoVal->value()));
    log(QString("AO %1 <- %2 V").arg(chan).arg(m_aoVal->value()));
}

void MainWindow::onAoZero()
{
    QString err;
    if (m_ao.isOpen()) { m_ao.write(0.0, &err); m_ao.close(); }
    m_aoVal->setValue(0);
    m_aoState->setText("Sortie fermee (0 V).");
    log("AO remise a 0 V et fermee.");
}

// =========================================================================================
// Onglet 5 : pilotes hors DAQmx
QWidget* MainWindow::buildDriversTab()
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->addWidget(new QLabel("Ces cartes ne sont pas pilotees par DAQmx. On teste ici seulement que le pilote est installe et "
                            "qu'une session s'ouvre puis se referme (aucune sortie n'est modifiee, pas de reset).\n"
                            "Entre le nom EXACT de la ressource tel qu'il apparait dans NI MAX (ex. PXI1Slot5)."));
    const char* labels[3] = {"PXIe-5108 (oscilloscope, NI-SCOPE)", "PXIe-6570 (digital pattern, niDigital)",
                             "PXIe-6537 (HSDIO, NI-HSDIO)"};
    for (int k = 0; k < 3; ++k) {
        auto* gb = new QGroupBox(labels[k]);
        auto* h = new QHBoxLayout(gb);
        m_rsrc[k] = new QComboBox; m_rsrc[k]->setEditable(true); m_rsrc[k]->setMinimumWidth(180);
        auto* b1 = new QPushButton("Tester la DLL");
        auto* b2 = new QPushButton("Ouvrir une session");
        connect(b1, &QPushButton::clicked, this, [this, k]() { onProbe(k, false); });
        connect(b2, &QPushButton::clicked, this, [this, k]() { onProbe(k, true); });
        h->addWidget(new QLabel("Ressource :")); h->addWidget(m_rsrc[k]); h->addWidget(b1); h->addWidget(b2); h->addStretch();
        v->addWidget(gb);
    }
    m_drvOut = new QPlainTextEdit; m_drvOut->setReadOnly(true);
    v->addWidget(m_drvOut, 1);
    return w;
}

void MainWindow::onProbe(int kind, bool open)
{
    const DriverProbe::Kind k = static_cast<DriverProbe::Kind>(kind);
    const QString rs = m_rsrc[kind]->currentText().trimmed();
    if (open && rs.isEmpty()) { m_drvOut->setPlainText("Entre d'abord le nom de la ressource."); return; }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QString out = DriverProbe::probe(k, rs, open);
    QApplication::restoreOverrideCursor();
    m_drvOut->setPlainText(out);
    log(QString("Pilote %1 : %2").arg(DriverProbe::dllName(k), out.simplified()));
}

// =========================================================================================
// Rapport
QString MainWindow::buildReport() const
{
    QString r;
    QTextStream s(&r);
    s << "=== NiBench rapport " << QDateTime::currentDateTime().toString(Qt::ISODate) << " ===\n";
    s << "Systeme : " << QSysInfo::prettyProductName() << "  Qt " << qVersion() << "\n";
    s << "NI-DAQmx compile : " << (Ni::compiled() ? "oui" : "NON") << "\n\n";
    s << "Cartes detectees (" << m_devs.size() << ") :\n";
    for (const auto& d : m_devs) {
        s << "  " << d.name << " | " << d.model << " | serie " << QString::number(d.serial, 16).toUpper() << "\n";
        if (!d.ai.isEmpty()) s << "     AI : " << d.ai.first() << " ... " << d.ai.last() << " (" << d.ai.size() << ")\n";
        if (!d.ao.isEmpty()) s << "     AO : " << d.ao.join(", ") << "\n";
        if (!d.diLines.isEmpty()) s << "     DI : " << d.diLines.first() << " ... " << d.diLines.last() << " (" << d.diLines.size() << ")\n";
        if (!d.doLines.isEmpty()) s << "     DO : " << d.doLines.first() << " ... " << d.doLines.last() << " (" << d.doLines.size() << ")\n";
        if (!d.coChans.isEmpty()) s << "     CO : " << d.coChans.size() << " compteur(s)\n";
    }
    s << "\n--- Journal ---\n" << m_log->toPlainText() << "\n";
    return r;
}

void MainWindow::copyReport()
{
    QApplication::clipboard()->setText(buildReport());
    log("Rapport copie dans le presse-papiers : colle-le dans la conversation.");
}

void MainWindow::saveReport()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    const QString path = QDir(dir).filePath(QString("NiBench_rapport_%1.txt")
                                                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(buildReport().toUtf8());
        log("Rapport enregistre : " + QDir::toNativeSeparators(path));
    } else {
        log("Impossible d'ecrire " + path, true);
    }
}
