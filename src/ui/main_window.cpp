#include "main_window.h"
#include "log_view.h"
#include "qso_entry.h"
#include "cluster_view.h"
#include "callbook_panel.h"
#include "contest_panel.h"
#include "settings_dialog.h"
#include "stats_dialog.h"
#include "awards_dialog.h"
#include "online_dialog.h"
#include "dashboard_dialog.h"
#include "integrity_dialog.h"
#include "duplicates_dialog.h"
#include "log_manager_dialog.h"
#include "dx_hunter_dialog.h"
#include "qsl_dialog.h"
#include "world_map_widget.h"
#include "../database.h"
#include "../services.h"
#include "../cat_service.h"
#include "../udp_listener.h"
#include "../adif.h"
#include "../cabrillo.h"
#include "../theme_manager.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QTabWidget>
#include <QDockWidget>
#include <QLabel>
#include <QTimer>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QFile>
#include <QStandardPaths>
#include <QCloseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QProgressDialog>
#include <QEventLoop>
#include <QStyle>
#include <QShortcut>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("HamLog");
    setMinimumSize(1024, 600);

    buildCentralWidget();
    buildMenu();
    buildToolBar();
    buildStatusBar();
    buildDocks();
    connectSignals();

    loadSettings();
    restoreLayout();
    updateWindowTitle();
    updateStatusBar();
}

MainWindow::~MainWindow()
{
    saveLayout();
    saveSettings();
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
    saveLayout();
    saveSettings();
    ev->accept();
}

// ---------------------------------------------------------------------------
// Build UI
// ---------------------------------------------------------------------------
void MainWindow::buildCentralWidget()
{
    m_splitter    = new QSplitter(Qt::Vertical, this);
    m_logView     = new LogView(this);
    m_clusterView = new ClusterView(this);
    m_mapWidget   = new WorldMapWidget(this);

    // Bottom pane: tabbed cluster + live world map
    auto* bottomTabs = new QTabWidget(this);
    bottomTabs->setObjectName("bottomTabs");
    bottomTabs->addTab(m_clusterView, tr("Cluster DX"));
    bottomTabs->addTab(m_mapWidget,   tr("Mappa Mondo"));

    m_splitter->addWidget(m_logView);
    m_splitter->addWidget(bottomTabs);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);
}

void MainWindow::buildMenu()
{
    // File
    QMenu* file = menuBar()->addMenu(tr("&File"));
    m_actNewQso = file->addAction(tr("&Nuovo QSO"), this, &MainWindow::onNewQso,
                                  QKeySequence::New);
    file->addSeparator();
    m_actImport = file->addAction(tr("Importa &ADIF…"),  this, &MainWindow::onImportAdif);
    m_actExport = file->addAction(tr("Esporta A&DIF…"),  this, &MainWindow::onExportAdif);
    file->addAction(tr("Esporta &Cabrillo…"), this, &MainWindow::onExportCabrillo);
    file->addSeparator();
    file->addAction(tr("&Nuovo Log…"),     this, &MainWindow::onNewLog);
    file->addAction(tr("&Gestisci Log…"),  this, &MainWindow::onManageLogs);
    file->addSeparator();
    m_actSettings = file->addAction(tr("&Impostazioni…"), this, &MainWindow::onSettings,
                                    QKeySequence::Preferences);
    file->addSeparator();
    file->addAction(tr("E&sci"), this, &MainWindow::onExit, QKeySequence::Quit);

    // Log
    QMenu* log = menuBar()->addMenu(tr("&Log"));
    m_actEditQso   = log->addAction(tr("&Modifica QSO"),  this, &MainWindow::onEditQso,   Qt::Key_Return);
    m_actDeleteQso = log->addAction(tr("&Elimina QSO"),   this, &MainWindow::onDeleteQso, Qt::Key_Delete);
    log->addAction(tr("D&uplica QSO"), this, &MainWindow::onDuplicateQso);
    log->addSeparator();
    m_actSearch = log->addAction(tr("&Cerca / Filtra…"), this, &MainWindow::onSearch,
                                 QKeySequence::Find);

    // Diplomi
    QMenu* awards = menuBar()->addMenu(tr("&Diplomi"));
    m_actStats    = awards->addAction(tr("&Statistiche…"),    this, &MainWindow::onStats);
    m_actAwards   = awards->addAction(tr("&Diplomi DXCC…"),   this, &MainWindow::onAwards);
    m_actDxHunter = awards->addAction(tr("D&X Hunter…"),      this, &MainWindow::onDxHunter);
    awards->addAction(tr("&Mappa Mondo…"), this, &MainWindow::onWorldMap);
    awards->addSeparator();
    m_actDashboard = awards->addAction(tr("&Dashboard…"), this, &MainWindow::onDashboard);

    // Online
    QMenu* online = menuBar()->addMenu(tr("&Online"));
    m_actUploadLotw = online->addAction(tr("Carica su &LoTW…"),   this, &MainWindow::onUploadLotw);
    online->addAction(tr("Scarica QSL &LoTW…"),   this, &MainWindow::onDownloadLotw);
    online->addSeparator();
    m_actUploadEqsl = online->addAction(tr("Carica su &eQSL…"),   this, &MainWindow::onUploadEqsl);
    online->addAction(tr("Casella eQSL…"),         this, &MainWindow::onDownloadEqsl);
    online->addSeparator();
    m_actUploadClub = online->addAction(tr("Carica su &ClubLog…"),this, &MainWindow::onUploadClublog);
    online->addSeparator();
    online->addAction(tr("&Account Online…"), this, &MainWindow::onOnlineDialog);

    // Strumenti
    QMenu* tools = menuBar()->addMenu(tr("&Strumenti"));
    m_actIntegrity = tools->addAction(tr("&Controllo Integrità…"), this, &MainWindow::onIntegrity);
    m_actQslDialog = tools->addAction(tr("&Gestione QSL…"),        this, &MainWindow::onQslDialog);
    tools->addSeparator();
    tools->addAction(tr("Gestione &Duplicati…"),               this, &MainWindow::onDuplicates);
    tools->addAction(tr("Recupera Nome/&QTH dal Callbook…"),   this, &MainWindow::onBackfillNames);
    tools->addSeparator();

    QMenu* themeMenu = tools->addMenu(tr("T&ema"));
    for (const QString& t : ThemeManager::availableThemes())
        themeMenu->addAction(t, this, [this, t]{ onChangeTheme(t); });

    // Aiuto
    QMenu* help = menuBar()->addMenu(tr("&Aiuto"));
    help->addAction(tr("&Informazioni su HamLog…"), this, [this]{
        QMessageBox::about(this, "HamLog",
            "<b>HamLog v1.0</b><br>Giornale radio amatoriale – Qt6 C++<br><br>"
            "de IW8XOU");
    });
}

void MainWindow::buildToolBar()
{
    QToolBar* tb = addToolBar(tr("Principale"));
    tb->setObjectName("mainToolBar");
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto* sty = QApplication::style();
    m_actNewQso->setIcon(sty->standardIcon(QStyle::SP_FileIcon));
    m_actSearch->setIcon(sty->standardIcon(QStyle::SP_FileDialogContentsView));
    m_actImport->setIcon(sty->standardIcon(QStyle::SP_DialogOpenButton));
    m_actExport->setIcon(sty->standardIcon(QStyle::SP_DialogSaveButton));
    m_actStats->setIcon(sty->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_actAwards->setIcon(sty->standardIcon(QStyle::SP_MessageBoxInformation));
    m_actDashboard->setIcon(sty->standardIcon(QStyle::SP_DesktopIcon));
    m_actUploadLotw->setIcon(sty->standardIcon(QStyle::SP_ArrowUp));
    m_actUploadEqsl->setIcon(sty->standardIcon(QStyle::SP_ArrowUp));
    m_actSettings->setIcon(sty->standardIcon(QStyle::SP_ComputerIcon));
    m_actDxHunter->setIcon(sty->standardIcon(QStyle::SP_DriveNetIcon));
    m_actIntegrity->setIcon(sty->standardIcon(QStyle::SP_MessageBoxWarning));
    m_actQslDialog->setIcon(sty->standardIcon(QStyle::SP_DirOpenIcon));

    tb->addAction(m_actNewQso);
    tb->addAction(m_actSearch);
    tb->addSeparator();
    tb->addAction(m_actImport);
    tb->addAction(m_actExport);
    tb->addSeparator();
    tb->addAction(m_actDashboard);
    tb->addAction(m_actStats);
    tb->addAction(m_actAwards);
    tb->addAction(m_actDxHunter);
    tb->addSeparator();
    tb->addAction(m_actUploadLotw);
    tb->addAction(m_actUploadEqsl);
    tb->addSeparator();
    tb->addAction(m_actSettings);
}

void MainWindow::buildStatusBar()
{
    m_lblLog   = new QLabel(tr("Log: —"), this);
    m_lblCount = new QLabel(tr("QSO: 0"), this);
    m_lblFreq  = new QLabel(tr("Freq: —"), this);
    m_lblMode  = new QLabel(tr("Modo: —"), this);
    m_lblCat   = new QLabel(tr("CAT: spento"), this);
    m_lblWsjtx = new QLabel(tr("WSJT-X: spento"), this);
    m_lblClock = new QLabel(this);
    m_lblClock->setMinimumWidth(130);
    m_lblClock->setAlignment(Qt::AlignCenter);
    {
        QFont cf = m_lblClock->font();
        cf.setFamily("Courier New");
        cf.setPointSize(cf.pointSize() + 4);
        cf.setBold(true);
        m_lblClock->setFont(cf);
        m_lblClock->setStyleSheet(
            "color:#40e0ff; background:#0a1a2e; border:1px solid #2980b9;"
            "border-radius:5px; padding:3px 12px; margin:1px 3px;");
    }

    for (QLabel* l : {m_lblLog, m_lblCount, m_lblFreq, m_lblMode, m_lblCat, m_lblWsjtx})
        l->setFrameShape(QFrame::Panel);

    statusBar()->addPermanentWidget(m_lblLog);
    statusBar()->addPermanentWidget(m_lblCount);
    statusBar()->addPermanentWidget(m_lblFreq);
    statusBar()->addPermanentWidget(m_lblMode);
    statusBar()->addPermanentWidget(m_lblCat);
    statusBar()->addPermanentWidget(m_lblWsjtx);
    statusBar()->addPermanentWidget(m_lblClock);
}

void MainWindow::buildDocks()
{
    m_callbook     = new CallbookPanel(this);
    m_callbookDock = new QDockWidget(tr("Callbook / Nominativo"), this);
    m_callbookDock->setObjectName("callbookDock");
    m_callbookDock->setWidget(m_callbook);
    addDockWidget(Qt::RightDockWidgetArea, m_callbookDock);

    m_contest     = new ContestPanel(this);
    m_contestDock = new QDockWidget(tr("Contest"), this);
    m_contestDock->setObjectName("contestDock");
    m_contestDock->setWidget(m_contest);
    addDockWidget(Qt::RightDockWidgetArea, m_contestDock);
    m_contestDock->hide();

    m_qsoEntry = new QsoEntry(this);
    m_qsoEntry->setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
}

// ---------------------------------------------------------------------------
// Signal connections
// ---------------------------------------------------------------------------
void MainWindow::connectSignals()
{
    // Database -> UI refresh
    connect(&Database::instance(), &Database::qsoSaved,   this, &MainWindow::onQsoSaved);
    connect(&Database::instance(), &Database::qsoUpdated, this, [this]{ updateStatusBar(); });
    connect(&Database::instance(), &Database::qsoDeleted, this, &MainWindow::onQsoDeleted);

    // LogView -> callbook: show stored data immediately, then trigger network lookup
    connect(m_logView, &LogView::selectionChanged, this, [this](const QString& cs) {
        Qso q = m_logView->selectedQso();
        if (q.isValid()) m_callbook->showFromQso(q);
        m_callbook->lookup(cs);
    });

    // QSO entry -> cluster highlight on every character change
    connect(m_qsoEntry, &QsoEntry::callsignChanged, m_clusterView, &ClusterView::highlightCall);

    // Auto callbook lookup: fires only when callsign not already in cache
    // (cache hits are handled directly inside QsoEntry::onCallsignChanged)
    connect(m_qsoEntry, &QsoEntry::callsignForLookup, m_callbook, &CallbookPanel::lookup);

    // Callbook result -> auto-fill QSO entry name/QTH/grid
    connect(m_callbook, &CallbookPanel::callbookData, m_qsoEntry, &QsoEntry::fillFromCallbook);

    // Cluster spot clicked -> fill QSO entry
    connect(m_clusterView, &ClusterView::spotClicked, m_qsoEntry, &QsoEntry::fillFromSpot);

    // Status + log refresh timer (every 3 seconds)
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(3000);
    connect(m_statusTimer, &QTimer::timeout, this, [this]{
        updateStatusBar();
        m_logView->refresh();
    });
    m_statusTimer->start();

    // Global keyboard shortcuts
    auto* scF1 = new QShortcut(Qt::Key_F1, this);
    connect(scF1, &QShortcut::activated, this, &MainWindow::onNewQso);
    auto* scF5 = new QShortcut(Qt::Key_F5, this);
    connect(scF5, &QShortcut::activated, m_logView, &LogView::refresh);

    // UTC clock (every second)
    auto updateClock = [this]{
        m_lblClock->setText(QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " UTC");
    };
    updateClock();
    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, updateClock);
    m_clockTimer->start();
}

// ---------------------------------------------------------------------------
// Settings / Layout persistence
// ---------------------------------------------------------------------------
void MainWindow::loadSettings()
{
    QSettings cfg;
    m_currentTheme = cfg.value("ui/theme", "Default").toString();

    // Start CAT if configured
    bool catEnabled = cfg.value("cat/enabled", false).toBool();
    if (catEnabled) startCat();

    // Start WSJT-X listener if enabled
    bool wsjtxEnabled = cfg.value("wsjtx/enabled", true).toBool();
    if (wsjtxEnabled) startWsjtx();

    // Decodium ADIF watcher — cerca prima percorso configurato, poi default Decodium
    QString decodiumFile = cfg.value("adifwatcher/path", QString()).toString();
    if (decodiumFile.isEmpty()) {
        // Percorso default: %LOCALAPPDATA%\IU8LMC\Decodium\decodium_log.adi
        QString localApp = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                           + "/AppData/Local";
        decodiumFile = localApp + "/IU8LMC/Decodium/decodium_log.adi";
    }
    // Avvia sempre il watcher — se il file non esiste ancora, il poll timer (5 s)
    // lo rileverà quando Decodium lo creerà
    m_adifWatcher = new AdifWatcher(this);
    connect(m_adifWatcher, &AdifWatcher::qsosFound, this, [this](const QList<Qso>& qsos){
        int dups = 0;
        QList<Qso> batch = qsos;
        int total = batch.size();
        Database::instance().importQsos(batch, &dups);
        int saved = total - dups;
        if (saved > 0) {
            m_logView->refresh();
            updateStatusBar();
            statusBar()->showMessage(
                tr("Decodium: +%1 QSO%2").arg(saved)
                    .arg(dups ? tr(" (%1 dup)").arg(dups) : QString()),
                4000);
        }
    });
    connect(m_adifWatcher, &AdifWatcher::fileAppeared, this, [this]{
        m_lblWsjtx->setText(tr("Decodium: attivo"));
    });
    m_adifWatcher->watch(decodiumFile);

    // Server TCP ADIF — Decodium si connette QUI (ADIFTcpEnabled=true, ADIFTcpServer=127.0.0.1:2333)
    m_adifTcp = new AdifTcpServer(this);
    connect(m_adifTcp, &AdifTcpServer::qsosReceived, this, [this](const QList<Qso>& qsos){
        int dups = 0;
        QList<Qso> batch = qsos;
        int total = batch.size();
        Database::instance().importQsos(batch, &dups);
        int saved = total - dups;
        if (saved > 0) {
            m_logView->refresh();
            updateStatusBar();
            statusBar()->showMessage(tr("Decodium TCP: +%1 QSO").arg(saved), 4000);
        }
    });
    connect(m_adifTcp, &AdifTcpServer::clientConnected, this, [this]{
        m_lblWsjtx->setText(tr("Decodium: connesso TCP ✓"));
    });
    quint16 adifTcpPort = cfg.value("decodium/tcpPort", 2333).toUInt();
    m_adifTcp->listen(adifTcpPort);

    m_lblWsjtx->setText(QFile::exists(decodiumFile)
                        ? tr("Decodium: attivo")
                        : tr("Decodium: in attesa file…"));
}

void MainWindow::saveSettings()
{
    QSettings cfg;
    cfg.setValue("ui/theme", m_currentTheme);
}

void MainWindow::restoreLayout()
{
    QSettings cfg;
    restoreGeometry(cfg.value("window/geometry").toByteArray());
    restoreState(cfg.value("window/state").toByteArray());

    QList<int> sizes = {3, 1};
    QVariantList vs = cfg.value("splitter/sizes").toList();
    if (vs.size() == 2) sizes = {vs[0].toInt(), vs[1].toInt()};
    m_splitter->setSizes(sizes);
}

void MainWindow::saveLayout()
{
    QSettings cfg;
    cfg.setValue("window/geometry", saveGeometry());
    cfg.setValue("window/state",    saveState());

    QVariantList vs;
    for (int s : m_splitter->sizes()) vs.append(s);
    cfg.setValue("splitter/sizes", vs);
}

void MainWindow::updateStatusBar()
{
    int count = Database::instance().countQsos();
    m_lblCount->setText(tr("QSO: %1").arg(count));

    auto logs = Database::instance().getLogs();
    int lid = Database::instance().currentLogId();
    for (const LogInfo& l : logs) {
        if (l.id == lid) {
            m_lblLog->setText(tr("Log: %1").arg(l.name));
            break;
        }
    }
}

void MainWindow::updateWindowTitle()
{
    QSettings cfg;
    QString call = cfg.value("station/callsign", "HamLog").toString();
    setWindowTitle(tr("HamLog – %1").arg(call));
}

// ---------------------------------------------------------------------------
// CAT / WSJT-X
// ---------------------------------------------------------------------------
void MainWindow::startCat()
{
    QSettings cfg;
    m_cat = new CatService(this);

    QString backend = cfg.value("cat/backend", "rigctld").toString();
    m_cat->setBackend(backend == "omnirig" ? CatService::OmniRig : CatService::Rigctld);
    m_cat->setRigctldHost(cfg.value("cat/host","127.0.0.1").toString(),
                          cfg.value("cat/port",4532).toUInt());

    connect(m_cat, &CatService::stateChanged, this, &MainWindow::onCatStateChanged);
    connect(m_cat, &CatService::error,        this, &MainWindow::onCatError);
    connect(m_cat, &CatService::connected,    this, [this]{ m_lblCat->setText("CAT: on"); });
    connect(m_cat, &CatService::disconnected, this, [this]{ m_lblCat->setText("CAT: off"); });
    connect(m_cat, &CatService::connected, m_qsoEntry, &QsoEntry::onCatConnected);

    m_cat->connectRig();
}

void MainWindow::startWsjtx()
{
    QSettings cfg;
    quint16 port = cfg.value("wsjtx/port", 2237).toUInt();
    m_wsjtx = new WsjtxListener(this);
    connect(m_wsjtx, &WsjtxListener::qsoLogged, this, &MainWindow::onWsjtxQso);
    connect(m_wsjtx, &WsjtxListener::statusChanged, this, [this](const QString& cs, double freq, const QString& mode){
        m_lblFreq->setText(tr("Freq: %1 MHz").arg(freq, 0, 'f', 3));
        m_lblMode->setText(tr("Modo: %1").arg(mode));
        if (!cs.isEmpty()) m_qsoEntry->prefill(cs, freq, mode);
    });
    if (m_wsjtx->start(port))
        m_lblWsjtx->setText(tr("WSJT-X: porta %1").arg(port));

    // Secondo listener su UDP 2333: Decodium invia ADIF plain-text su questa porta
    // (stessa porta usata da logom4 per ricevere QSO in tempo reale)
    if (port != 2333) {
        m_wsjtx2333 = new WsjtxListener(this);
        connect(m_wsjtx2333, &WsjtxListener::qsoLogged, this, &MainWindow::onWsjtxQso);
        m_wsjtx2333->start(2333);
    }
}

void MainWindow::onCatStateChanged(const RigState& state)
{
    if (state.freq > 0)
        m_lblFreq->setText(tr("Freq: %1 MHz").arg(state.freq / 1e6, 0, 'f', 3));
    if (!state.mode.isEmpty())
        m_lblMode->setText(tr("Modo: %1").arg(state.mode));

    m_qsoEntry->setRigState(state);
}

void MainWindow::onCatError(const QString& msg)
{
    m_lblCat->setText(tr("CAT: error"));
    statusBar()->showMessage(tr("CAT: %1").arg(msg), 5000);
}

void MainWindow::onWsjtxQso(const Qso& qso)
{
    Qso copy = qso;
    int dups = 0;
    Database::instance().importQsos({copy}, &dups);
    m_logView->refresh();
    updateStatusBar();
    if (dups == 0)
        statusBar()->showMessage(tr("Decodium/WSJT-X: loggato %1").arg(qso.callsign), 4000);
    else
        statusBar()->showMessage(tr("Decodium/WSJT-X: %1 già presente").arg(qso.callsign), 3000);
}

// ---------------------------------------------------------------------------
// Slots – File menu
// ---------------------------------------------------------------------------
void MainWindow::onNewQso()
{
    m_qsoEntry->reset();
    m_qsoEntry->show();
    m_qsoEntry->raise();
    m_qsoEntry->activateWindow();
}

void MainWindow::onImportAdif()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Importa ADIF"),
                   QString(), tr("File ADIF (*.adi *.adif);;Tutti (*.*)"));
    if (file.isEmpty()) return;

    QList<Qso> qsos = AdifHandler::importAdif(file);
    if (qsos.isEmpty()) {
        QMessageBox::warning(this, tr("Importa ADIF"),
            tr("Nessun QSO trovato nel file.\n\n"
               "Verificare che il file sia in formato ADIF valido (.adi o .adif)\n"
               "e contenga i campi CALL, QSO_DATE, BAND (o FREQ) e MODE."));
        return;
    }
    int dups = 0;
    Database::instance().importQsos(qsos, &dups);
    m_logView->refresh();
    updateStatusBar();
    int imported = qsos.size() - dups;
    statusBar()->showMessage(
        tr("Importati %1 QSO (%2 duplicati saltati)").arg(imported).arg(dups), 6000);
    if (imported > 0)
        QMessageBox::information(this, tr("Importa ADIF"),
            tr("Importazione completata.\n%1 QSO importati, %2 duplicati saltati.")
            .arg(imported).arg(dups));
}

void MainWindow::onExportAdif()
{
    QString file = QFileDialog::getSaveFileName(this, tr("Esporta ADIF"),
                   QString(), tr("File ADIF (*.adi)"));
    if (file.isEmpty()) return;

    QSettings cfg;
    QString myCall = cfg.value("station/callsign", "N0CALL").toString();
    QList<Qso> qsos = Database::instance().getAllQsos();
    int n = AdifHandler::exportAdif(file, qsos, myCall);
    statusBar()->showMessage(tr("Esportati %1 QSO in ADIF").arg(n), 4000);
}

void MainWindow::onExportCabrillo()
{
    QString file = QFileDialog::getSaveFileName(this, tr("Esporta Cabrillo"),
                   QString(), tr("Cabrillo (*.cbr *.log)"));
    if (file.isEmpty()) return;

    QSettings cfg;
    QHash<QString,QString> header;
    header["callsign"] = cfg.value("station/callsign","N0CALL").toString();
    header["contest"]  = cfg.value("contest/id","").toString();
    header["exchange"] = cfg.value("contest/exchange","").toString();

    QList<Qso> qsos = Database::instance().getAllQsos();
    int n = Cabrillo::exportCabrillo(file, qsos, header);
    statusBar()->showMessage(tr("Esportati %1 QSO in Cabrillo").arg(n), 4000);
}

void MainWindow::onNewLog()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Nuovo Log"), tr("Nome log:"),
                                         QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    Database::instance().addLog(name, QString(), QString(), QString());
    updateStatusBar();
}

void MainWindow::onManageLogs()
{
    LogManagerDialog dlg(this);
    dlg.exec();
    updateStatusBar();
    m_logView->refresh();
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        loadSettings();
        updateWindowTitle();
        QString theme = QSettings().value("ui/theme","Default").toString();
        onChangeTheme(theme);
    }
}

void MainWindow::onExit() { close(); }

// ---------------------------------------------------------------------------
// Slots – Log menu
// ---------------------------------------------------------------------------
void MainWindow::onEditQso()
{
    Qso q = m_logView->selectedQso();
    if (!q.isValid()) return;
    m_qsoEntry->loadQso(q);
    m_qsoEntry->show();
    m_qsoEntry->raise();
}

void MainWindow::onDeleteQso()
{
    Qso q = m_logView->selectedQso();
    if (!q.isValid()) return;
    if (QMessageBox::question(this, tr("Elimina"),
            tr("Eliminare il QSO con %1?").arg(q.callsign),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    Database::instance().deleteQso(q.id);
    m_logView->refresh();
    updateStatusBar();
}

void MainWindow::onDuplicateQso()
{
    Qso q = m_logView->selectedQso();
    if (!q.isValid()) return;
    q.id = 0;
    m_qsoEntry->loadQso(q);
    m_qsoEntry->show();
    m_qsoEntry->raise();
}

void MainWindow::onSearch()
{
    m_logView->openSearchBar();
}

// ---------------------------------------------------------------------------
// Slots – Awards menu
// ---------------------------------------------------------------------------
void MainWindow::onStats()
{
    StatsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onAwards()
{
    AwardsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onDxHunter()
{
    DxHunterDialog dlg(this);
    dlg.exec();
}

void MainWindow::onWorldMap()
{
    WorldMapWidget* w = new WorldMapWidget(nullptr);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    w->setWindowTitle(tr("World Map – Worked Entities"));
    w->resize(1000, 500);
    w->show();
}

void MainWindow::onDashboard()
{
    DashboardDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Slots – Online menu
// ---------------------------------------------------------------------------
void MainWindow::showOnline(int tab)
{
    if (!m_onlineDialog) {
        auto* dlg = new OnlineDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose, false);
        connect(dlg, &OnlineDialog::qsosUpdated, this, [this] {
            m_logView->refresh();
            m_mapWidget->refresh();
            updateStatusBar();
        });
        m_onlineDialog = dlg;
    }
    auto* dlg = static_cast<OnlineDialog*>(m_onlineDialog);
    dlg->switchTab(static_cast<OnlineDialog::Tab>(tab));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::onUploadLotw()    { showOnline(static_cast<int>(OnlineDialog::Tab::LoTW_Upload));   }
void MainWindow::onDownloadLotw()  { showOnline(static_cast<int>(OnlineDialog::Tab::LoTW_Download)); }
void MainWindow::onUploadEqsl()    { showOnline(static_cast<int>(OnlineDialog::Tab::eQSL));          }
void MainWindow::onDownloadEqsl()  { showOnline(static_cast<int>(OnlineDialog::Tab::eQSL));          }
void MainWindow::onUploadClublog() { showOnline(static_cast<int>(OnlineDialog::Tab::ClubLog));       }
void MainWindow::onOnlineDialog()  { showOnline(static_cast<int>(OnlineDialog::Tab::LoTW_Upload));   }

// ---------------------------------------------------------------------------
// Slots – Tools menu
// ---------------------------------------------------------------------------
void MainWindow::onIntegrity()
{
    IntegrityDialog dlg(this);
    dlg.exec();
    m_logView->refresh();
}

void MainWindow::onQslDialog()
{
    QslDialog dlg(this);
    dlg.exec();
    m_logView->refresh();
}

void MainWindow::onDuplicates()
{
    DuplicatesDialog dlg(this);
    dlg.exec();
    m_logView->refresh();
}

void MainWindow::onBackfillNames()
{
    QStringList missing = Database::instance().callsignsMissingNameQth();
    if (missing.isEmpty()) {
        QMessageBox::information(this, tr("Recupera Nome/QTH"),
                                 tr("Tutti i QSO hanno già nome e QTH."));
        return;
    }

    int ret = QMessageBox::question(this, tr("Recupera Nome/QTH"),
        tr("Trovati %1 nominativi con nome/QTH mancante.\n"
           "Verrà interrogato il callbook per ciascuno e aggiornati tutti i QSO corrispondenti.\n\n"
           "Procedere?").arg(missing.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QProgressDialog prog(tr("Ricerca nominativi…"), tr("Annulla"), 0, missing.size(), this);
    prog.setWindowModality(Qt::NonModal);
    prog.setMinimumDuration(0);
    prog.show();

    // Use HamQTH if configured, otherwise HamDB
    QSettings cfg;
    QString hamqthUser = cfg.value("hamqth/username").toString();
    HamQthService* hamqth = nullptr;
    HamDbService*  hamdb  = nullptr;
    if (!hamqthUser.isEmpty()) {
        hamqth = new HamQthService(this);
        hamqth->setCredentials(hamqthUser, cfg.value("hamqth/password").toString());
    } else {
        hamdb = new HamDbService(this);
    }

    int updated = 0;
    for (int i = 0; i < missing.size(); ++i) {
        if (prog.wasCanceled()) break;
        prog.setValue(i);
        prog.setLabelText(tr("Ricerca %1…").arg(missing[i]));
        QApplication::processEvents();

        const QString& cs = missing[i];
        QHash<QString,QString> data;

        // Check DB cache first (instant)
        data = Database::instance().getCallbookCache(cs);

        if (data.isEmpty()) {
            if (hamqth)
                data = hamqth->lookupSync(cs);
            else if (hamdb)
                data = hamdb->lookupSync(cs);
        }

        // Skip update if lookup returned error or no useful data
        if (!data.isEmpty() && !data.contains("_error")) {
            QString name = data.value("name");
            if (name.isEmpty()) name = data.value("fname");
            QString qth = data.value("addr1");
            if (!data.value("addr2").isEmpty()) qth += ", " + data.value("addr2");
            QString grid = data.value("grid");
            if (!name.isEmpty() || !qth.isEmpty() || !grid.isEmpty())
                updated += Database::instance().applyCallbookToQsos(cs, name, qth, grid);
        }
    }
    prog.setValue(missing.size());
    if (hamqth) hamqth->deleteLater();
    if (hamdb)  hamdb->deleteLater();

    m_logView->refresh();
    QMessageBox::information(this, tr("Recupero Completato"),
        tr("Aggiornati %1 QSO con nome/QTH.").arg(updated));
}

void MainWindow::onChangeTheme(const QString& name)
{
    m_currentTheme = name;
    ThemeManager::apply(qApp, name);
}

// ---------------------------------------------------------------------------
// Database callbacks
// ---------------------------------------------------------------------------
void MainWindow::onQsoSaved(int)
{
    m_logView->refresh();
    m_mapWidget->refresh();
    updateStatusBar();
}

void MainWindow::onQsoDeleted(int)
{
    m_logView->refresh();
    m_mapWidget->refresh();
    updateStatusBar();
}

void MainWindow::onLogChanged(int logId)
{
    Database::instance().setCurrentLog(logId);
    m_logView->refresh();
    updateStatusBar();
}

void MainWindow::applyTheme(const QString& name)
{
    ThemeManager::apply(qApp, name);
}
