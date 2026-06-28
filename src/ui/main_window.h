#pragma once
#include <QMainWindow>
#include <QList>
#include "types.h"

class QAction;
class QLabel;
class QSplitter;
class QDockWidget;
class QStackedWidget;
class QComboBox;
class QToolButton;
class QTimer;
class QDialog;
class QProcess;

class LogView;
class QsoEntry;
class ClusterView;
class CallbookPanel;
class ContestPanel;
class CatService;
class WsjtxListener;
class AdifWatcher;
class AdifTcpServer;
class WorldMapWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* ev) override;

private slots:
    // File menu
    void onNewQso();
    void onImportAdif();
    void onExportAdif();
    void onExportCabrillo();
    void onNewLog();
    void onManageLogs();
    void onSettings();
    void onExit();

    // Log menu
    void onEditQso();
    void onDeleteQso();
    void onDuplicateQso();
    void onSearch();

    // Awards menu
    void onStats();
    void onAwards();
    void onDxHunter();
    void onWorldMap();

    // Online menu
    void onUploadLotw();
    void onDownloadLotw();
    void onUploadEqsl();
    void onDownloadEqsl();
    void onUploadClublog();
    void onOnlineDialog();

    void showOnline(int tab);   // takes OnlineDialog::Tab as int to avoid include in header

    // Tools menu
    void onIntegrity();
    void onQslDialog();
    void onDashboard();
    void onDuplicates();
    void onBackfillNames();
    void onChangeTheme(const QString& name);

    // CAT
    void onCatStateChanged(const RigState& state);
    void onCatError(const QString& msg);

    // WSJT-X
    void onWsjtxQso(const Qso& qso);

    // Database
    void onQsoSaved(int id);
    void onQsoDeleted(int id);
    void onLogChanged(int logId);

    void updateStatusBar();
    void updateWindowTitle();
    void applySyncTimer();
    void onAutoSync();
    void applyRigctld();

private:
    void buildMenu();
    void buildToolBar();
    void buildStatusBar();
    void buildCentralWidget();
    void buildDocks();
    void connectSignals();
    void restoreLayout();
    void saveLayout();
    void loadSettings();
    void saveSettings();
    void applyTheme(const QString& name);
    void startCat();
    void startWsjtx();

    // Actions
    QAction* m_actNewQso      = nullptr;
    QAction* m_actImport      = nullptr;
    QAction* m_actExport      = nullptr;
    QAction* m_actSettings    = nullptr;
    QAction* m_actEditQso     = nullptr;
    QAction* m_actDeleteQso   = nullptr;
    QAction* m_actSearch      = nullptr;
    QAction* m_actStats       = nullptr;
    QAction* m_actAwards      = nullptr;
    QAction* m_actDxHunter    = nullptr;
    QAction* m_actUploadLotw  = nullptr;
    QAction* m_actUploadEqsl  = nullptr;
    QAction* m_actUploadClub  = nullptr;
    QAction* m_actIntegrity   = nullptr;
    QAction* m_actQslDialog   = nullptr;
    QAction* m_actDashboard   = nullptr;

    // Central layout
    QSplitter*       m_splitter     = nullptr;
    LogView*         m_logView      = nullptr;
    ClusterView*     m_clusterView  = nullptr;
    WorldMapWidget*  m_mapWidget    = nullptr;

    // Docks
    QDockWidget*     m_callbookDock = nullptr;
    QDockWidget*     m_contestDock  = nullptr;
    CallbookPanel*   m_callbook     = nullptr;
    ContestPanel*    m_contest      = nullptr;

    // QSO entry (floating window)
    QsoEntry*        m_qsoEntry     = nullptr;

    // Online services dialog — persistent, non-modal (forward declare in cpp)
    QDialog*         m_onlineDialog = nullptr;

    // Status bar widgets
    QLabel* m_lblCount   = nullptr;
    QLabel* m_lblLog     = nullptr;
    QLabel* m_lblFreq    = nullptr;
    QLabel* m_lblMode    = nullptr;
    QLabel* m_lblCat     = nullptr;
    QLabel* m_lblWsjtx   = nullptr;
    QLabel* m_lblClock   = nullptr;

    // Services
    CatService*     m_cat         = nullptr;
    WsjtxListener*  m_wsjtx       = nullptr;
    WsjtxListener*  m_wsjtx2333   = nullptr;  // secondo listener UDP 2333 (ADIF Decodium)
    AdifWatcher*    m_adifWatcher = nullptr;
    AdifTcpServer*  m_adifTcp     = nullptr;

    QTimer*   m_statusTimer    = nullptr;
    QTimer*   m_clockTimer     = nullptr;
    QTimer*   m_syncTimer      = nullptr;
    QProcess* m_rigctldProcess = nullptr;

    QString m_currentTheme = "Default";
};
