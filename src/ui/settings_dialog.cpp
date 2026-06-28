#include "settings_dialog.h"
#include "../services.h"
#include <algorithm>
#include <windows.h>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QSettings>
#include <QApplication>
#include <QCoreApplication>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Impostazioni"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumWidth(480);
    build();
    load();
}

void SettingsDialog::build()
{
    auto* vl = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    auto makeTab = [&](const QString& name) -> QFormLayout* {
        auto* w = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setRowWrapPolicy(QFormLayout::WrapLongRows);
        tabs->addTab(w, name);
        return fl;
    };

    // Station
    {
        auto* fl = makeTab(tr("Stazione"));
        m_callsign  = new QLineEdit(this);
        m_operator  = new QLineEdit(this);
        m_myName    = new QLineEdit(this);
        m_qth       = new QLineEdit(this);
        m_locator   = new QLineEdit(this);
        m_myCountry = new QLineEdit(this);
        m_myCq      = new QSpinBox(this); m_myCq->setRange(1,40);
        m_myItu     = new QSpinBox(this); m_myItu->setRange(1,90);
        m_myPower   = new QLineEdit(this); m_myPower->setMaximumWidth(80);
        m_myAntenna = new QLineEdit(this);
        fl->addRow(tr("Nominativo:"),  m_callsign);
        fl->addRow(tr("Operatore:"),   m_operator);
        fl->addRow(tr("Nome:"),        m_myName);
        fl->addRow(tr("QTH:"),         m_qth);
        fl->addRow(tr("Locator:"),     m_locator);
        fl->addRow(tr("Nazione:"),     m_myCountry);
        fl->addRow(tr("Zona CQ:"),     m_myCq);
        fl->addRow(tr("Zona ITU:"),    m_myItu);
        fl->addRow(tr("Potenza (W):"), m_myPower);
        fl->addRow(tr("Antenna:"),     m_myAntenna);
    }

    // HamQTH (second tab — most commonly needed for callbook)
    {
        auto* fl = makeTab(tr("HamQTH"));
        auto* info = new QLabel(tr(
            "<b>HamQTH.com</b> — callbook gratuito per radioamatori.<br>"
            "Inserisci le credenziali per abilitare la ricerca automatica<br>"
            "di nome, QTH e paese durante l'inserimento QSO."), this);
        info->setWordWrap(true);
        fl->addRow(info);
        m_hamqthUser = new QLineEdit(this);
        m_hamqthUser->setPlaceholderText(tr("es. IW8XOU"));
        m_hamqthPass = new QLineEdit(this);
        m_hamqthPass->setEchoMode(QLineEdit::Password);
        fl->addRow(tr("Nominativo HamQTH:"), m_hamqthUser);
        fl->addRow(tr("Password:"),           m_hamqthPass);

        // Test button + status
        auto* hl2 = new QHBoxLayout;
        auto* btnTest   = new QPushButton(tr("Prova connessione"), this);
        m_hamqthStatus  = new QLabel(this);
        m_hamqthStatus->setWordWrap(true);
        hl2->addWidget(btnTest);
        hl2->addWidget(m_hamqthStatus, 1);
        fl->addRow(hl2);

        connect(btnTest, &QPushButton::clicked, this, [this]{
            QString u = m_hamqthUser->text().trimmed();
            QString p = m_hamqthPass->text();
            if (u.isEmpty() || p.isEmpty()) {
                m_hamqthStatus->setStyleSheet("color:#c0392b; font-weight:bold;");
                m_hamqthStatus->setText(tr("Inserisci nominativo e password."));
                return;
            }
            m_hamqthStatus->setStyleSheet("color:#7f8c8d;");
            m_hamqthStatus->setText(tr("Connessione in corso…"));
            qApp->processEvents();
            HamQthService svc;
            svc.setCredentials(u, p);
            auto result = svc.lookupSync(u);
            if (result.contains("_error")) {
                m_hamqthStatus->setStyleSheet("color:#c0392b; font-weight:bold;");
                m_hamqthStatus->setText(tr("Errore: %1").arg(result["_error"]));
            } else if (result.isEmpty()) {
                m_hamqthStatus->setStyleSheet("color:#e67e22; font-weight:bold;");
                m_hamqthStatus->setText(tr("Login OK ma nessun dato trovato per %1.").arg(u));
            } else {
                QString name = result.value("name", result.value("fname"));
                m_hamqthStatus->setStyleSheet("color:#27ae60; font-weight:bold;");
                m_hamqthStatus->setText(tr("✓ Connessione riuscita! Trovato: %1").arg(name));
            }
        });
    }

    // Log options
    {
        auto* fl = makeTab(tr("Log"));
        m_autoUtc     = new QCheckBox(tr("Ora UTC automatica al salvataggio"), this);
        m_autosaveFt8 = new QCheckBox(tr("Salva automaticamente QSO FT8/FT4 da Decodium"), this);
        m_dupeBand    = new QCheckBox(tr("Considera duplicato per banda"), this);
        m_dupeMode    = new QCheckBox(tr("Considera duplicato per modo"), this);
        fl->addRow(m_autoUtc);
        fl->addRow(m_autosaveFt8);
        fl->addRow(new QLabel(tr("Controllo duplicati:"), this));
        fl->addRow(m_dupeBand);
        fl->addRow(m_dupeMode);
    }

    // Database
    {
        auto* fl = makeTab(tr("Database"));
        m_dbPath = new QLineEdit(this);
        auto* hl = new QHBoxLayout;
        hl->addWidget(m_dbPath);
        auto* btn = new QPushButton(tr("…"), this);
        btn->setMaximumWidth(30);
        hl->addWidget(btn);
        fl->addRow(tr("Percorso database:"), hl);
        connect(btn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDb);
    }

    // CAT
    {
        auto* fl = makeTab(tr("CAT"));
        m_catEnabled = new QCheckBox(tr("Abilita CAT (frequenza automatica nel QSO)"), this);
        m_catBackend = new QComboBox(this);   // mantenuto per compatibilità, nascosto
        m_catBackend->addItems({"rigctld"});
        m_catBackend->hide();
        m_catHost = new QLineEdit("127.0.0.1", this);
        m_catPort = new QSpinBox(this);
        m_catPort->setRange(1, 65535);
        m_catPort->setValue(4532);
        fl->addRow(m_catEnabled);
        fl->addRow(tr("Host rigctld:"), m_catHost);
        fl->addRow(tr("Porta TCP:"),    m_catPort);

        // --- rigctld auto-start ---
        fl->addRow(new QLabel(tr("<b>Avvio automatico rigctld</b> (condivisione CAT con Decodium e altre app):"), this));
        m_rigctldAutoStart = new QCheckBox(tr("Avvia rigctld automaticamente all'apertura di HamLog"), this);
        fl->addRow(m_rigctldAutoStart);

        auto* pathRow = new QHBoxLayout;
        m_rigctldPath = new QLineEdit(this);
        m_rigctldPath->setPlaceholderText(tr("Percorso rigctld.exe"));
        pathRow->addWidget(m_rigctldPath);
        auto* btnRig = new QPushButton(tr("…"), this); btnRig->setMaximumWidth(30);
        pathRow->addWidget(btnRig);
        fl->addRow(tr("Eseguibile rigctld:"), pathRow);
        connect(btnRig, &QPushButton::clicked, this, &SettingsDialog::onBrowseRigctld);

        auto* comRow = new QHBoxLayout;
        m_rigctldComPort = new QComboBox(this);
        m_rigctldComPort->setEditable(true);
        m_rigctldComPort->setInsertPolicy(QComboBox::NoInsert);
        m_rigctldComPort->lineEdit()->setPlaceholderText(tr("es. COM5"));
        comRow->addWidget(m_rigctldComPort, 1);
        auto* btnRefreshCom = new QPushButton(tr("↺"), this);
        btnRefreshCom->setMaximumWidth(30);
        btnRefreshCom->setToolTip(tr("Aggiorna lista porte COM disponibili"));
        comRow->addWidget(btnRefreshCom);
        fl->addRow(tr("Porta COM radio:"), comRow);
        connect(btnRefreshCom, &QPushButton::clicked, this, &SettingsDialog::refreshComPorts);
        refreshComPorts();

        m_rigctldModel = new QSpinBox(this);
        m_rigctldModel->setRange(1, 9999);
        m_rigctldModel->setValue(1035);
        m_rigctldModel->setToolTip(tr("FT-991A = 1035  |  FT-891 = 1036  |  FT-818 = 1038\n"
                                      "IC-7300 = 3073  |  IC-9700 = 3081\n"
                                      "Vedi: https://hamlib.github.io/riglist.html"));
        fl->addRow(tr("Modello Hamlib (es. 1035=FT-991A):"), m_rigctldModel);

        m_rigctldBaud = new QComboBox(this);
        m_rigctldBaud->addItems({"4800","9600","19200","38400","57600","115200"});
        m_rigctldBaud->setCurrentText("38400");
        fl->addRow(tr("Velocità seriale (baud):"), m_rigctldBaud);

        // abilita/disabilita campi in base al checkbox autostart
        auto updateRigFields = [this](bool en) {
            m_rigctldPath->setEnabled(en);
            m_rigctldComPort->setEnabled(en);
            m_rigctldModel->setEnabled(en);
            m_rigctldBaud->setEnabled(en);
        };
        connect(m_rigctldAutoStart, &QCheckBox::toggled, updateRigFields);
        updateRigFields(false);
    }

    // LoTW
    {
        auto* fl = makeTab(tr("LoTW"));
        m_lotwCall = new QLineEdit(this);
        m_lotwPass = new QLineEdit(this); m_lotwPass->setEchoMode(QLineEdit::Password);
        m_tqslPath = new QLineEdit(this);
        auto* hl = new QHBoxLayout;
        hl->addWidget(m_tqslPath);
        auto* btn = new QPushButton(tr("…"), this); btn->setMaximumWidth(30);
        hl->addWidget(btn);
        fl->addRow(tr("Utente:"),       m_lotwCall);
        fl->addRow(tr("Password:"),    m_lotwPass);
        fl->addRow(tr("Percorso tqsl:"), hl);
        connect(btn, &QPushButton::clicked, this, &SettingsDialog::onBrowseTqsl);
    }

    // eQSL
    {
        auto* fl = makeTab(tr("eQSL"));
        m_eqslUser = new QLineEdit(this);
        m_eqslPass = new QLineEdit(this); m_eqslPass->setEchoMode(QLineEdit::Password);
        fl->addRow(tr("Utente:"),   m_eqslUser);
        fl->addRow(tr("Password:"), m_eqslPass);
    }

    // ClubLog
    {
        auto* fl = makeTab(tr("ClubLog"));
        m_clubEmail = new QLineEdit(this);
        m_clubPass  = new QLineEdit(this); m_clubPass->setEchoMode(QLineEdit::Password);
        m_clubCall  = new QLineEdit(this);
        fl->addRow(tr("Email:"),      m_clubEmail);
        fl->addRow(tr("Password:"),   m_clubPass);
        fl->addRow(tr("Nominativo:"), m_clubCall);
    }

    // UI
    {
        auto* fl = makeTab(tr("Interfaccia"));
        m_theme = new QComboBox(this);
        m_theme->addItems({"Default","Dark","Night","Sepia"});
        fl->addRow(tr("Tema:"), m_theme);
    }

    // WSJT-X
    {
        auto* fl = makeTab(tr("WSJT-X"));
        m_wsjtxEn   = new QCheckBox(tr("Abilita ascolto WSJT-X"), this);
        m_wsjtxPort = new QSpinBox(this);
        m_wsjtxPort->setRange(1024, 65535);
        m_wsjtxPort->setValue(2333);
        fl->addRow(m_wsjtxEn);
        fl->addRow(tr("Porta UDP:"), m_wsjtxPort);
    }

    // Decodium
    {
        auto* fl = makeTab(tr("Decodium"));
        m_decodiumPath = new QLineEdit(this);
        auto* hl = new QHBoxLayout;
        hl->addWidget(m_decodiumPath);
        auto* btn = new QPushButton(tr("…"), this); btn->setMaximumWidth(30);
        hl->addWidget(btn);
        fl->addRow(tr("File ADIF da monitorare:"), hl);
        connect(btn, &QPushButton::clicked, this, [this]{
            QString f = QFileDialog::getOpenFileName(this, tr("File ADIF"),
                        QString(), tr("ADIF (*.adi *.adif)"));
            if (!f.isEmpty()) m_decodiumPath->setText(f);
        });
    }

    // Sync automatica
    {
        auto* fl = makeTab(tr("Sync Auto"));
        m_syncEnabled  = new QCheckBox(tr("Abilita sincronizzazione automatica"), this);
        m_syncInterval = new QSpinBox(this);
        m_syncInterval->setRange(5, 1440);
        m_syncInterval->setValue(60);
        m_syncInterval->setSuffix(tr(" minuti"));
        m_syncLotw = new QCheckBox(tr("LoTW — scarica QSL ricevute"), this);
        m_syncEqsl = new QCheckBox(tr("eQSL — scarica QSL ricevute"), this);
        fl->addRow(m_syncEnabled);
        fl->addRow(tr("Ogni:"), m_syncInterval);
        fl->addRow(new QLabel(tr("Servizi da sincronizzare:"), this));
        fl->addRow(m_syncLotw);
        fl->addRow(m_syncEqsl);
        connect(m_syncEnabled, &QCheckBox::toggled, m_syncInterval, &QSpinBox::setEnabled);
        connect(m_syncEnabled, &QCheckBox::toggled, m_syncLotw,     &QCheckBox::setEnabled);
        connect(m_syncEnabled, &QCheckBox::toggled, m_syncEqsl,     &QCheckBox::setEnabled);
        m_syncInterval->setEnabled(false);
        m_syncLotw->setEnabled(false);
        m_syncEqsl->setEnabled(false);
    }

    vl->addWidget(tabs);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

void SettingsDialog::load()
{
    QSettings cfg;
    m_callsign->setText(cfg.value("station/callsign").toString());
    m_operator->setText(cfg.value("station/operator").toString());
    m_myName->setText(cfg.value("station/name").toString());
    m_qth->setText(cfg.value("station/qth").toString());
    m_locator->setText(cfg.value("station/locator").toString());
    m_myCountry->setText(cfg.value("station/country").toString());
    m_myCq->setValue(cfg.value("station/cq_zone",14).toInt());
    m_myItu->setValue(cfg.value("station/itu_zone",28).toInt());
    m_myPower->setText(cfg.value("station/power").toString());
    m_myAntenna->setText(cfg.value("station/antenna").toString());

    m_autoUtc->setChecked(cfg.value("log/auto_utc",true).toBool());
    m_autosaveFt8->setChecked(cfg.value("log/autosave_ft8",true).toBool());
    m_dupeBand->setChecked(cfg.value("log/dupe_band",true).toBool());
    m_dupeMode->setChecked(cfg.value("log/dupe_mode",false).toBool());

    m_dbPath->setText(cfg.value("database/path").toString());

    m_catEnabled->setChecked(cfg.value("cat/enabled",false).toBool());
    m_catBackend->setCurrentText(cfg.value("cat/backend","rigctld").toString());
    m_catHost->setText(cfg.value("cat/host","127.0.0.1").toString());
    m_catPort->setValue(cfg.value("cat/port",4532).toInt());

    {
        bool autoStart = cfg.value("cat/rigctld_autostart", false).toBool();
        m_rigctldAutoStart->setChecked(autoStart);
        m_rigctldPath->setText(cfg.value("cat/rigctld_path",
            QCoreApplication::applicationDirPath() + "/rigctld.exe").toString());
        {
            QString saved = cfg.value("cat/rigctld_comport", "COM5").toString();
            int idx = m_rigctldComPort->findText(saved, Qt::MatchFixedString);
            if (idx >= 0) m_rigctldComPort->setCurrentIndex(idx);
            else           m_rigctldComPort->setCurrentText(saved);
        }
        m_rigctldModel->setValue(cfg.value("cat/rigctld_model", 1035).toInt());
        m_rigctldBaud->setCurrentText(cfg.value("cat/rigctld_baud","38400").toString());
        m_rigctldPath->setEnabled(autoStart);
        m_rigctldComPort->setEnabled(autoStart);
        m_rigctldModel->setEnabled(autoStart);
        m_rigctldBaud->setEnabled(autoStart);
    }

    m_lotwCall->setText(cfg.value("lotw/username").toString());
    m_lotwPass->setText(cfg.value("lotw/password").toString());
    m_tqslPath->setText(cfg.value("lotw/tqsl_path","tqsl").toString());

    m_eqslUser->setText(cfg.value("eqsl/username").toString());
    m_eqslPass->setText(cfg.value("eqsl/password").toString());

    m_clubEmail->setText(cfg.value("clublog/email").toString());
    m_clubPass->setText(cfg.value("clublog/password").toString());
    m_clubCall->setText(cfg.value("clublog/callsign").toString());

    m_hamqthUser->setText(cfg.value("hamqth/username").toString());
    m_hamqthPass->setText(cfg.value("hamqth/password").toString());

    m_theme->setCurrentText(cfg.value("ui/theme","Default").toString());

    m_wsjtxEn->setChecked(cfg.value("wsjtx/enabled",true).toBool());
    m_wsjtxPort->setValue(cfg.value("wsjtx/port",2333).toInt());

    m_decodiumPath->setText(cfg.value("adifwatcher/path").toString());

    bool syncEn = cfg.value("sync/enabled", false).toBool();
    m_syncEnabled->setChecked(syncEn);
    m_syncInterval->setValue(cfg.value("sync/intervalMinutes", 60).toInt());
    m_syncLotw->setChecked(cfg.value("sync/lotw", true).toBool());
    m_syncEqsl->setChecked(cfg.value("sync/eqsl", true).toBool());
    m_syncInterval->setEnabled(syncEn);
    m_syncLotw->setEnabled(syncEn);
    m_syncEqsl->setEnabled(syncEn);
}

void SettingsDialog::save()
{
    QSettings cfg;
    cfg.setValue("station/callsign",  m_callsign->text().trimmed().toUpper());
    cfg.setValue("station/operator",  m_operator->text().trimmed().toUpper());
    cfg.setValue("station/name",      m_myName->text().trimmed());
    cfg.setValue("station/qth",       m_qth->text().trimmed());
    cfg.setValue("station/locator",   m_locator->text().trimmed().toUpper());
    cfg.setValue("station/country",   m_myCountry->text().trimmed());
    cfg.setValue("station/cq_zone",   m_myCq->value());
    cfg.setValue("station/itu_zone",  m_myItu->value());
    cfg.setValue("station/power",     m_myPower->text().trimmed());
    cfg.setValue("station/antenna",   m_myAntenna->text().trimmed());

    cfg.setValue("log/auto_utc",      m_autoUtc->isChecked());
    cfg.setValue("log/autosave_ft8",  m_autosaveFt8->isChecked());
    cfg.setValue("log/dupe_band",     m_dupeBand->isChecked());
    cfg.setValue("log/dupe_mode",     m_dupeMode->isChecked());

    { QString p = m_dbPath->text().trimmed(); if (!p.isEmpty()) cfg.setValue("database/path", p); }

    cfg.setValue("cat/enabled",              m_catEnabled->isChecked());
    cfg.setValue("cat/backend",              m_catBackend->currentText());
    cfg.setValue("cat/host",                 m_catHost->text().trimmed());
    cfg.setValue("cat/port",                 m_catPort->value());
    cfg.setValue("cat/rigctld_autostart",    m_rigctldAutoStart->isChecked());
    cfg.setValue("cat/rigctld_path",         m_rigctldPath->text().trimmed());
    cfg.setValue("cat/rigctld_comport",      m_rigctldComPort->currentText().trimmed());
    cfg.setValue("cat/rigctld_model",        m_rigctldModel->value());
    cfg.setValue("cat/rigctld_baud",         m_rigctldBaud->currentText());

    cfg.setValue("lotw/username",     m_lotwCall->text().trimmed());
    cfg.setValue("lotw/password",     m_lotwPass->text());
    cfg.setValue("lotw/tqsl_path",    m_tqslPath->text().trimmed());

    cfg.setValue("eqsl/username",     m_eqslUser->text().trimmed());
    cfg.setValue("eqsl/password",     m_eqslPass->text());

    cfg.setValue("clublog/email",     m_clubEmail->text().trimmed());
    cfg.setValue("clublog/password",  m_clubPass->text());
    cfg.setValue("clublog/callsign",  m_clubCall->text().trimmed().toUpper());

    cfg.setValue("hamqth/username",   m_hamqthUser->text().trimmed());
    cfg.setValue("hamqth/password",   m_hamqthPass->text());

    cfg.setValue("ui/theme",          m_theme->currentText());

    cfg.setValue("wsjtx/enabled",     m_wsjtxEn->isChecked());
    cfg.setValue("wsjtx/port",        m_wsjtxPort->value());

    cfg.setValue("adifwatcher/path",  m_decodiumPath->text().trimmed());

    cfg.setValue("sync/enabled",         m_syncEnabled->isChecked());
    cfg.setValue("sync/intervalMinutes", m_syncInterval->value());
    cfg.setValue("sync/lotw",            m_syncLotw->isChecked());
    cfg.setValue("sync/eqsl",            m_syncEqsl->isChecked());
}

void SettingsDialog::onAccept()
{
    save();
    accept();
}

void SettingsDialog::onBrowseDb()
{
    QString f = QFileDialog::getSaveFileName(this, tr("File database"),
                m_dbPath->text(), tr("SQLite (*.db)"));
    if (!f.isEmpty()) m_dbPath->setText(f);
}

void SettingsDialog::onBrowseTqsl()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Eseguibile tqsl"),
                m_tqslPath->text());
    if (!f.isEmpty()) m_tqslPath->setText(f);
}

void SettingsDialog::onBrowseRigctld()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Eseguibile rigctld"),
                m_rigctldPath->text(), tr("Eseguibili (*.exe);;Tutti (*)"));
    if (!f.isEmpty()) m_rigctldPath->setText(f);
}

void SettingsDialog::refreshComPorts()
{
    QString current = m_rigctldComPort->currentText();
    m_rigctldComPort->clear();

    // Legge le porte COM dal registro di Windows via WinAPI
    // (QSettings non funziona: i nomi chiave contengono '\' che viene interpretato come separatore)
    QStringList ports;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD idx = 0;
        wchar_t valName[256], valData[256];
        DWORD nameLen, dataLen, type;
        while (true) {
            nameLen = 256; dataLen = sizeof(valData);
            LONG res = RegEnumValueW(hKey, idx++, valName, &nameLen,
                                     nullptr, &type, (LPBYTE)valData, &dataLen);
            if (res != ERROR_SUCCESS) break;
            if (type == REG_SZ)
                ports << QString::fromWCharArray(valData);
        }
        RegCloseKey(hKey);
    }

    // Fallback: se il registro non restituisce nulla, proponi COM1-COM20
    if (ports.isEmpty())
        for (int i = 1; i <= 20; ++i)
            ports << QString("COM%1").arg(i);

    // Ordina numericamente (COM1, COM2, … COM10, COM11…)
    std::sort(ports.begin(), ports.end(), [](const QString& a, const QString& b){
        return a.mid(3).toInt() < b.mid(3).toInt();
    });

    m_rigctldComPort->addItems(ports);

    if (!current.isEmpty()) {
        int idx = m_rigctldComPort->findText(current, Qt::MatchFixedString);
        if (idx >= 0) m_rigctldComPort->setCurrentIndex(idx);
        else           m_rigctldComPort->setCurrentText(current);
    }
}
