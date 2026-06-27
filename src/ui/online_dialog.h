#pragma once
#include <QDialog>
class QTabWidget;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QLineEdit;
class QLabel;
class QListWidget;
class QCloseEvent;
class OnlineService;

class OnlineDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Tab { LoTW_Upload=0, LoTW_Download, eQSL=2, ClubLog=3, HamQTH=4, eQSL_Gallery=5 };

    explicit OnlineDialog(QWidget* parent = nullptr, Tab tab = Tab::LoTW_Upload);

    void switchTab(Tab t);

signals:
    void qsosUpdated();   // emitted after any batch DB update (log view should refresh)

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onLotwUpload();
    void onLotwDownload();
    void onEqslUpload();
    void onEqslInbox();
    void onClublogUpload();
    void onHamqthTest();
    void onHamqthSave();
    void addEqslCard(const QString& path);

private:
    void build();
    void log(const QString& msg);
    void setBusy(bool busy, int progressMax = 0);

    QTabWidget*    m_tabs       = nullptr;
    QTextEdit*     m_log        = nullptr;
    QProgressBar*  m_progress   = nullptr;
    QPushButton*   m_btnCancel  = nullptr;
    Tab            m_initTab;
    bool           m_busy       = false;
    QPushButton*   m_btnClose   = nullptr;
    OnlineService* m_activeSvc  = nullptr;  // tracked to allow cancel

    // LoTW / TQSL
    QLineEdit*   m_tqslPath    = nullptr;
    QLineEdit*   m_tqslStation = nullptr;
    // eQSL credentials
    QLineEdit*   m_eqslUser    = nullptr;
    QLineEdit*   m_eqslPass    = nullptr;
    QLabel*      m_eqslStatus  = nullptr;
    // HamQTH tab
    QLineEdit*   m_hqUser      = nullptr;
    QLineEdit*   m_hqPass      = nullptr;
    QLabel*      m_hqStatus    = nullptr;
    // eQSL gallery
    QListWidget* m_eqslGallery = nullptr;
};
