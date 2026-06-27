#include "online_dialog.h"
#include "../database.h"
#include "../services.h"
#include "../adif.h"
#include <QTabWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QTemporaryFile>
#include <QDateTime>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QListWidget>
#include <QPixmap>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

OnlineDialog::OnlineDialog(QWidget* parent, Tab tab)
    : QDialog(parent), m_initTab(tab)
{
    setWindowTitle(tr("Servizi Online"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(560, 420);
    build();
    m_tabs->setCurrentIndex(static_cast<int>(tab));
}

void OnlineDialog::closeEvent(QCloseEvent* e)
{
    if (m_busy) {
        auto btn = QMessageBox::question(this, tr("Download in corso"),
            tr("Un'operazione è in corso. Annullare e chiudere?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) { e->ignore(); return; }
        if (m_activeSvc) m_activeSvc->cancelPending();
    }
    QDialog::closeEvent(e);
}

void OnlineDialog::build()
{
    auto* vl = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // LoTW Upload
    {
        auto* w  = new QWidget;
        auto* wl = new QVBoxLayout(w);

        // TQSL configuration
        auto* grp = new QGroupBox(tr("Configurazione TQSL"), w);
        auto* fl  = new QFormLayout(grp);

        // TQSL executable path
        auto* pathRow  = new QHBoxLayout;
        m_tqslPath     = new QLineEdit(grp);
        m_tqslPath->setPlaceholderText(tr("es. C:\\Program Files\\TrustedQSL\\tqsl.exe"));
        auto* btnBrowse = new QPushButton(tr("Sfoglia…"), grp);
        btnBrowse->setFixedWidth(90);
        pathRow->addWidget(m_tqslPath, 1);
        pathRow->addWidget(btnBrowse);
        fl->addRow(tr("Percorso TQSL:"), pathRow);

        auto* btnFind  = new QPushButton(tr("Cerca automaticamente"), grp);
        fl->addRow("", btnFind);

        // Station location name used in TQSL
        m_tqslStation = new QLineEdit(grp);
        m_tqslStation->setPlaceholderText(tr("Nome stazione configurata in TQSL, es. IW8XOU"));
        fl->addRow(tr("Stazione TQSL:"), m_tqslStation);

        wl->addWidget(grp);

        auto* btnsRow   = new QHBoxLayout;
        auto* btnUpload = new QPushButton(tr("Carica ADIF su LoTW via TQSL"), w);
        auto* btnList   = new QPushButton(tr("Elenca stazioni TQSL"), w);
        btnList->setToolTip(tr("Mostra i nomi delle stazioni configurate in TQSL\n"
                               "(usali nel campo 'Stazione TQSL' qui sopra)"));
        btnsRow->addWidget(btnUpload, 2);
        btnsRow->addWidget(btnList,   1);
        wl->addLayout(btnsRow);
        wl->addStretch();

        // Load saved settings (auto-fix if path points to a .tq6 certificate instead of exe)
        {
            QSettings cfg;
            QString savedPath = cfg.value("lotw/tqsl_path").toString();
            // If saved path is a certificate (.tq6) or empty, search for the real exe
            bool needSearch = savedPath.isEmpty()
                           || !savedPath.toLower().endsWith(".exe")
                           || !QFile::exists(savedPath);
            if (needSearch) {
                const QStringList candidates = {
                    "C:/Program Files/TrustedQSL/tqsl.exe",
                    "C:/Program Files (x86)/TrustedQSL/tqsl.exe",
                    "C:/Program Files/TQSL/tqsl.exe",
                    "C:/Program Files (x86)/TQSL/tqsl.exe",
                };
                for (const QString& p : candidates) {
                    if (QFile::exists(p)) {
                        savedPath = QDir::toNativeSeparators(p);
                        cfg.setValue("lotw/tqsl_path", savedPath);
                        break;
                    }
                }
            }
            m_tqslPath->setText(savedPath);
            QString savedStation = cfg.value("lotw/tqsl_station",
                                             cfg.value("station/callsign")).toString();
            m_tqslStation->setText(savedStation);
        }

        connect(btnBrowse, &QPushButton::clicked, this, [this] {
            QString path = QFileDialog::getOpenFileName(this, tr("Seleziona TQSL"),
                "C:/Program Files", tr("Eseguibili (*.exe);;Tutti i file (*)"));
            if (!path.isEmpty()) {
                m_tqslPath->setText(QDir::toNativeSeparators(path));
                QSettings().setValue("lotw/tqsl_path", m_tqslPath->text());
            }
        });

        connect(btnFind, &QPushButton::clicked, this, [this] {
            const QStringList candidates = {
                "C:/Program Files/TrustedQSL/tqsl.exe",
                "C:/Program Files (x86)/TrustedQSL/tqsl.exe",
                "C:/Program Files/TQSL/tqsl.exe",
                "C:/Program Files (x86)/TQSL/tqsl.exe",
                "C:/Users/" + qEnvironmentVariable("USERNAME") + "/AppData/Local/Programs/TrustedQSL/tqsl.exe",
            };
            for (const QString& p : candidates) {
                if (QFile::exists(p)) {
                    m_tqslPath->setText(QDir::toNativeSeparators(p));
                    QSettings().setValue("lotw/tqsl_path", m_tqslPath->text());
                    log(tr("✓ TQSL trovato: %1").arg(p));
                    return;
                }
            }
            log(tr("✗ TQSL non trovato nelle posizioni standard. Scaricalo da "
                   "https://lotw.arrl.org/lotw-help/installation/ poi usa 'Sfoglia'."));
        });

        connect(btnUpload, &QPushButton::clicked, this, &OnlineDialog::onLotwUpload);

        // Elenca stazioni disponibili in TQSL (utile per capire il nome esatto)
        connect(btnList, &QPushButton::clicked, this, [this] {
            QString tqsl = m_tqslPath ? m_tqslPath->text().trimmed() : QString();
            if (tqsl.isEmpty() || !QFile::exists(tqsl)) {
                log(tr("⚠ Configura prima il percorso TQSL."));
                return;
            }
            log(tr("Ricerca stazioni TQSL…"));
            // Run tqsl with a dummy station name — the error output lists available ones
            QStringList args{"-l", "___INVALID___", "-u", "-q",
                             QDir::temp().absoluteFilePath("dummy_tqsl.adi")};
            QFile dummy(args.last());
            if (dummy.open(QIODevice::WriteOnly))
                dummy.write("START-OF-LOG: 3.0\nEND-OF-LOG:\n"), dummy.close();
            QProcess proc;
            proc.setProcessChannelMode(QProcess::MergedChannels);
            proc.start(tqsl, args);
            proc.waitForFinished(10000);
            QString out = QString::fromLocal8Bit(proc.readAll()).trimmed();
            dummy.remove();
            if (out.isEmpty())
                log(tr("TQSL non ha prodotto output. Prova ad aprirlo manualmente "
                       "per verificare le stazioni configurate."));
            else
                log(tr("Output TQSL:\n") + out);
        });

        m_tabs->addTab(w, tr("LoTW Upload"));
    }
    // LoTW Download
    {
        auto* w  = new QWidget;
        auto* wl = new QVBoxLayout(w);
        auto* btn = new QPushButton(tr("Scarica QSL da LoTW"), w);
        connect(btn, &QPushButton::clicked, this, &OnlineDialog::onLotwDownload);
        wl->addWidget(btn);
        wl->addStretch();
        m_tabs->addTab(w, tr("LoTW Download"));
    }
    // eQSL (credenziali + upload + inbox in un unico tab)
    {
        auto* w  = new QWidget;
        auto* wl = new QVBoxLayout(w);

        // -- Credenziali --
        auto* credGrp = new QGroupBox(tr("Credenziali eQSL.cc"), w);
        auto* fl      = new QFormLayout(credGrp);
        m_eqslUser = new QLineEdit(credGrp);
        m_eqslUser->setPlaceholderText(tr("es. IW8XOU"));
        m_eqslPass = new QLineEdit(credGrp);
        m_eqslPass->setEchoMode(QLineEdit::Password);
        fl->addRow(tr("Nominativo:"), m_eqslUser);
        fl->addRow(tr("Password:"),   m_eqslPass);

        auto* credBtns = new QHBoxLayout;
        auto* btnSave  = new QPushButton(tr("Salva credenziali"), credGrp);
        m_eqslStatus   = new QLabel(credGrp);
        m_eqslStatus->setWordWrap(true);
        credBtns->addWidget(btnSave);
        credBtns->addWidget(m_eqslStatus, 1);
        fl->addRow(credBtns);
        wl->addWidget(credGrp);

        // -- Azioni --
        auto* actGrp  = new QGroupBox(tr("Operazioni"), w);
        auto* actVl   = new QVBoxLayout(actGrp);
        auto* btnUp   = new QPushButton(tr("Carica log su eQSL (Upload ADIF)"), actGrp);
        auto* btnIn   = new QPushButton(tr("Scarica conferme + cartoline (Inbox)"), actGrp);
        btnIn->setDefault(true);
        actVl->addWidget(btnUp);
        actVl->addWidget(btnIn);
        wl->addWidget(actGrp);
        wl->addStretch();

        // Carica credenziali salvate
        {
            QSettings cfg;
            m_eqslUser->setText(cfg.value("eqsl/username").toString());
            m_eqslPass->setText(cfg.value("eqsl/password").toString());
        }

        connect(btnSave, &QPushButton::clicked, this, [this] {
            QSettings cfg;
            cfg.setValue("eqsl/username", m_eqslUser->text().trimmed());
            cfg.setValue("eqsl/password", m_eqslPass->text());
            m_eqslStatus->setStyleSheet("color:#27ae60;font-weight:bold;");
            m_eqslStatus->setText(tr("✓ Credenziali salvate."));
        });
        connect(btnUp,  &QPushButton::clicked, this, &OnlineDialog::onEqslUpload);
        connect(btnIn,  &QPushButton::clicked, this, &OnlineDialog::onEqslInbox);

        m_tabs->addTab(w, tr("eQSL"));
    }
    // ClubLog
    {
        auto* w  = new QWidget;
        auto* wl = new QVBoxLayout(w);
        auto* btn = new QPushButton(tr("Carica su ClubLog"), w);
        connect(btn, &QPushButton::clicked, this, &OnlineDialog::onClublogUpload);
        wl->addWidget(btn);
        wl->addStretch();
        m_tabs->addTab(w, tr("ClubLog"));
    }

    // HamQTH
    {
        auto* w  = new QWidget;
        auto* wl = new QVBoxLayout(w);

        auto* info = new QLabel(tr(
            "<b>HamQTH.com</b> — callbook gratuito per radioamatori.<br>"
            "Le credenziali vengono usate per la ricerca automatica di nome,<br>"
            "QTH e paese durante l'inserimento dei QSO."), w);
        info->setWordWrap(true);
        wl->addWidget(info);

        auto* grp = new QGroupBox(tr("Credenziali HamQTH"), w);
        auto* fl  = new QFormLayout(grp);
        m_hqUser = new QLineEdit(grp);
        m_hqUser->setPlaceholderText(tr("es. IW8XOU"));
        m_hqPass = new QLineEdit(grp);
        m_hqPass->setEchoMode(QLineEdit::Password);
        fl->addRow(tr("Nominativo:"), m_hqUser);
        fl->addRow(tr("Password:"),   m_hqPass);
        wl->addWidget(grp);

        auto* hl2   = new QHBoxLayout;
        auto* btnTest = new QPushButton(tr("Prova connessione"), w);
        auto* btnSave = new QPushButton(tr("Salva"), w);
        btnSave->setDefault(true);
        m_hqStatus = new QLabel(w);
        m_hqStatus->setWordWrap(true);
        hl2->addWidget(btnTest);
        hl2->addWidget(btnSave);
        hl2->addWidget(m_hqStatus, 1);
        wl->addLayout(hl2);
        wl->addStretch();

        // Load saved credentials
        {
            QSettings cfg;
            m_hqUser->setText(cfg.value("hamqth/username").toString());
            m_hqPass->setText(cfg.value("hamqth/password").toString());
        }

        connect(btnTest, &QPushButton::clicked, this, &OnlineDialog::onHamqthTest);
        connect(btnSave, &QPushButton::clicked, this, &OnlineDialog::onHamqthSave);

        m_tabs->addTab(w, tr("HamQTH"));
    }

    // eQSL Gallery
    {
        auto* w  = new QWidget;
        auto* wl = new QVBoxLayout(w);

        auto* info = new QLabel(
            tr("<b>Cartoline eQSL scaricate.</b> "
               "Doppio click su una cartolina per aprirla a schermo intero."), w);
        info->setWordWrap(true);
        wl->addWidget(info);

        m_eqslGallery = new QListWidget(w);
        m_eqslGallery->setViewMode(QListWidget::IconMode);
        m_eqslGallery->setIconSize(QSize(180, 120));
        m_eqslGallery->setResizeMode(QListWidget::Adjust);
        m_eqslGallery->setSpacing(10);
        m_eqslGallery->setUniformItemSizes(true);
        m_eqslGallery->setMovement(QListWidget::Static);
        m_eqslGallery->setWordWrap(true);
        wl->addWidget(m_eqslGallery, 1);

        // Load cards already on disk (real images only)
        QString cardDir = QStandardPaths::writableLocation(
                              QStandardPaths::AppDataLocation) + "/eqsl_cards";
        const auto realCards = QDir(cardDir).entryInfoList(
                                   {"*.jpg","*.png"}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : realCards)
            addEqslCard(fi.filePath());

        // Count confirmed QSOs without custom card images
        int nNoImage = QDir(cardDir).entryList({"*.noimage"}, QDir::Files).size();

        QString statusText;
        if (realCards.isEmpty() && nNoImage == 0) {
            statusText = tr("Nessuna cartolina eQSL. Scarica prima le conferme dalla scheda eQSL.");
        } else if (realCards.isEmpty()) {
            statusText = tr("%1 QSO confermati eQSL ✓ — le stazioni usano cartoline auto-generate,\n"
                            "visibili solo sul sito web eQSL.cc (non scaricabili via API).\n"
                            "Le conferme sono registrate nel log (colonna eQSL ✓).").arg(nNoImage);
        } else {
            statusText = tr("%1 cartoline con immagine personalizzata, %2 QSO con cartolina auto-generata.")
                             .arg(realCards.size()).arg(nNoImage);
        }
        auto* noCard = new QLabel(statusText, w);
        noCard->setAlignment(Qt::AlignCenter);
        noCard->setWordWrap(true);
        wl->addWidget(noCard);

        connect(m_eqslGallery, &QListWidget::itemDoubleClicked,
                this, [](QListWidgetItem* item) {
                    QString path = item->data(Qt::UserRole).toString();
                    if (!path.isEmpty())
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                });

        m_tabs->addTab(w, tr("Cartoline eQSL"));
    }

    vl->addWidget(m_tabs);

    // Progress row: bar + cancel button
    auto* progressRow = new QHBoxLayout;
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->hide();
    m_btnCancel = new QPushButton(tr("Annulla"), this);
    m_btnCancel->hide();
    m_btnCancel->setFixedWidth(90);
    connect(m_btnCancel, &QPushButton::clicked, this, [this] {
        if (m_activeSvc) m_activeSvc->cancelPending();
        m_btnCancel->setEnabled(false);
        m_btnCancel->setText(tr("Annullando…"));
    });
    progressRow->addWidget(m_progress, 1);
    progressRow->addWidget(m_btnCancel);
    vl->addLayout(progressRow);

    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(140);
    vl->addWidget(m_log);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_btnClose = bb->button(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

void OnlineDialog::switchTab(Tab t)
{
    m_tabs->setCurrentIndex(static_cast<int>(t));
}

void OnlineDialog::log(const QString& msg)
{
    m_log->append(QDateTime::currentDateTime().toString("[HH:mm:ss] ") + msg);
}

void OnlineDialog::setBusy(bool busy, int progressMax)
{
    m_busy = busy;
    // Tabs stay ENABLED — user can navigate while downloading
    if (busy) {
        m_progress->setRange(0, progressMax);   // 0 = indeterminate spin; >0 = percentage
        m_progress->setValue(0);
        m_progress->show();
        m_btnCancel->setEnabled(true);
        m_btnCancel->setText(tr("Annulla"));
        m_btnCancel->show();
    } else {
        m_progress->hide();
        m_btnCancel->hide();
        m_activeSvc = nullptr;
    }
}

// ---------------------------------------------------------------------------
// LoTW Upload
// ---------------------------------------------------------------------------
void OnlineDialog::onLotwUpload()
{
    if (m_busy) return;
    QSettings cfg;

    // Prefer live UI fields, fall back to settings
    QString tqsl = (m_tqslPath && !m_tqslPath->text().trimmed().isEmpty())
                   ? m_tqslPath->text().trimmed()
                   : cfg.value("lotw/tqsl_path").toString().trimmed();
    QString station = (m_tqslStation && !m_tqslStation->text().trimmed().isEmpty())
                      ? m_tqslStation->text().trimmed()
                      : cfg.value("lotw/tqsl_station",
                                  cfg.value("station/callsign", "N0CALL")).toString().trimmed();

    // Validate TQSL path — must be a .exe, not a .tq6 certificate
    if (tqsl.isEmpty()) {
        log(tr("⚠ Percorso TQSL non configurato. Usa 'Cerca automaticamente' o 'Sfoglia'."));
        return;
    }
    if (!tqsl.toLower().endsWith(".exe")) {
        log(tr("⚠ Il percorso TQSL punta a '%1' che non è un eseguibile (.exe).\n"
               "   Deve puntare a tqsl.exe, non a un certificato .tq6.")
            .arg(QFileInfo(tqsl).fileName()));
        if (m_tqslPath) m_tqslPath->setStyleSheet("border: 2px solid #c0392b;");
        return;
    }
    if (!QFile::exists(tqsl)) {
        log(tr("⚠ TQSL non trovato in: %1").arg(tqsl));
        return;
    }
    if (m_tqslPath) m_tqslPath->setStyleSheet("");

    // Save validated paths
    cfg.setValue("lotw/tqsl_path",    tqsl);
    cfg.setValue("lotw/tqsl_station", station);

    // Use a persistent file (not temp) — TQSL opens it independently via startDetached
    // and reads it after this function returns.
    QString adifPath = QDir::temp().absoluteFilePath("HamLog_lotw.adi");
    QList<Qso> qsos = Database::instance().getAllQsos();
    int n = AdifHandler::exportAdif(adifPath, qsos, station);
    if (n < 0) { log(tr("Impossibile creare il file ADIF in %1").arg(adifPath)); return; }
    log(tr("ADIF esportato: %1 QSO → %2").arg(n).arg(QDir::toNativeSeparators(adifPath)));

    setBusy(true, 0);
    auto* svc = new LotwService(this);
    m_activeSvc = svc;
    connect(svc, &LotwService::progress, this, &OnlineDialog::log);
    connect(svc, &LotwService::uploadFinished, this,
            [this, svc](bool ok, const QString& msg) {
                setBusy(false);
                log((ok ? tr("✓ ") : tr("✗ ")) + msg);
                svc->deleteLater();
            });
    svc->upload(adifPath, station, tqsl);
}

// ---------------------------------------------------------------------------
// LoTW Download
// ---------------------------------------------------------------------------
void OnlineDialog::onLotwDownload()
{
    if (m_busy) return;
    QSettings cfg;
    QString user = cfg.value("lotw/username").toString();
    QString pass = cfg.value("lotw/password").toString();
    if (user.isEmpty()) { log(tr("Nome utente LoTW non configurato")); return; }

    setBusy(true, 0);
    auto* svc = new LotwService(this);
    m_activeSvc = svc;
    svc->resetCancel();
    connect(svc, &LotwService::progress, this, &OnlineDialog::log);
    connect(svc, &LotwService::qslsReceived, this, [this](const QList<Qso>& qsls) {
        log(tr("LoTW: %1 QSL ricevute dal server").arg(qsls.size()));
        auto normMode = [](const QString& m) -> QString {
            QString u = m.toUpper();
            return (u=="USB"||u=="LSB") ? "SSB" : u;
        };
        // Show first QSL for diagnosis
        if (!qsls.isEmpty()) {
            const Qso& q0 = qsls.first();
            log(tr("  Primo: %1  data=%2  banda=%3  modo=%4")
                .arg(q0.callsign, q0.date, q0.band, q0.mode));
        }
        int updated = 0;
        for (const Qso& q : qsls) {
            for (const Qso& existing : Database::instance().searchQsos(q.callsign)) {
                if (existing.date == q.date &&
                    existing.band.toLower() == q.band.toLower() &&
                    normMode(existing.mode) == normMode(q.mode))
                {
                    Database::instance().markLotwRcvd({existing.id}, "Y");
                    ++updated;
                    break;
                }
            }
        }
        log(tr("Aggiornati %1 / %2 QSO con conferme LoTW").arg(updated).arg(qsls.size()));
        emit qsosUpdated();
    });
    connect(svc, &LotwService::finished, this,
            [this, svc](bool /*ok*/, const QString& msg) {
                setBusy(false);
                log(msg);
                svc->deleteLater();
            });
    svc->fetchQsls(user, pass);
}

// ---------------------------------------------------------------------------
// eQSL Upload
// ---------------------------------------------------------------------------
void OnlineDialog::onEqslUpload()
{
    if (m_busy) return;
    QSettings cfg;
    // Prefer live fields (user might not have clicked "Salva")
    QString user   = (m_eqslUser && !m_eqslUser->text().trimmed().isEmpty())
                     ? m_eqslUser->text().trimmed()
                     : cfg.value("eqsl/username").toString().trimmed();
    QString pass   = (m_eqslPass && !m_eqslPass->text().isEmpty())
                     ? m_eqslPass->text()
                     : cfg.value("eqsl/password").toString();
    QString myCall = cfg.value("station/callsign", "N0CALL").toString();
    if (user.isEmpty()) {
        log(tr("⚠ Inserisci le credenziali eQSL nel tab eQSL prima di caricare."));
        return;
    }

    QList<Qso> qsos = Database::instance().getAllQsos();
    QTemporaryFile tmp;
    if (!tmp.open()) { log(tr("Impossibile creare il file temporaneo")); return; }
    AdifHandler::exportAdif(tmp.fileName(), qsos, myCall);
    tmp.seek(0);
    QString adif = QString::fromUtf8(tmp.readAll());

    setBusy(true, 0);
    auto* svc = new EqslService(this);
    m_activeSvc = svc;
    connect(svc, &EqslService::progress, this, &OnlineDialog::log);
    connect(svc, &EqslService::uploadFinished, this,
            [this, svc](bool ok, int n, const QString& msg) {
                setBusy(false);
                log(tr("eQSL: %1 (%2 record) — %3")
                    .arg(ok ? tr("OK") : tr("Errore")).arg(n).arg(msg));
                svc->deleteLater();
            });
    svc->uploadAdif(user, pass, adif);
}

// ---------------------------------------------------------------------------
// eQSL Inbox — confirms QSOs AND downloads card images
// ---------------------------------------------------------------------------
void OnlineDialog::onEqslInbox()
{
    if (m_busy) return;
    QSettings cfg;
    QString user = (m_eqslUser && !m_eqslUser->text().trimmed().isEmpty())
                   ? m_eqslUser->text().trimmed()
                   : cfg.value("eqsl/username").toString().trimmed();
    QString pass = (m_eqslPass && !m_eqslPass->text().isEmpty())
                   ? m_eqslPass->text()
                   : cfg.value("eqsl/password").toString();
    if (user.isEmpty()) {
        log(tr("⚠ Inserisci il nominativo e la password nel tab eQSL."));
        return;
    }

    setBusy(true, 0);   // indeterminate while downloading ADIF
    auto* svc = new EqslService(this);
    m_activeSvc = svc;
    svc->resetCancel();
    connect(svc, &EqslService::progress, this, &OnlineDialog::log);

    // Download incrementale: solo QSL ricevute dopo l'ultimo download
    QString lastDate = cfg.value("eqsl/lastInboxDate").toString();
    if (lastDate.isEmpty()) {
        log(tr("eQSL: primo download — recupero tutto lo storico…"));
    } else {
        log(tr("eQSL: download incrementale da %1…").arg(lastDate));
    }

    // Step 1: download inbox ADIF
    QList<Qso> inboxQsos;
    bool inboxOk = false;

    connect(svc, &EqslService::inboxReceived, this,
            [this, &inboxQsos, &inboxOk](const QList<Qso>& qsls) {
                inboxQsos = qsls;
                inboxOk   = true;
                log(tr("eQSL Inbox: %1 QSO ricevuti").arg(qsls.size()));
            });
    connect(svc, &EqslService::finished, this,
            [this](bool ok, const QString& msg) {
                if (!ok) log(tr("✗ %1").arg(msg));
            });

    svc->fetchInbox(user, pass, lastDate);

    if (!inboxOk) {
        setBusy(false);
        svc->deleteLater();
        return;
    }
    if (inboxQsos.isEmpty()) {
        log(tr("Inbox eQSL vuoto — nessun QSO da scaricare."));
        setBusy(false);
        svc->deleteLater();
        return;
    }

    // Step 2: match against local log with flexible comparison and mark confirmed
    auto normMode = [](const QString& m) -> QString {
        QString u = m.toUpper();
        return (u=="USB"||u=="LSB") ? "SSB" : u;
    };

    // Detailed diagnostics: show why first few inbox QSOs do NOT match local log
    {
        int diagShown = 0;
        log(tr("--- Diagnostica matching (prime mancate corrispondenze) ---"));
        for (const Qso& q : inboxQsos) {
            if (diagShown >= 5) break;
            auto cands = Database::instance().searchQsos(q.callsign);
            if (cands.isEmpty()) {
                log(tr("  NOMATCH %1: nessun QSO locale con questo call")
                    .arg(q.callsign));
                ++diagShown;
                continue;
            }
            bool found = false;
            for (const Qso& ex : cands) {
                if (ex.date == q.date &&
                    ex.band.toLower() == q.band.toLower() &&
                    normMode(ex.mode) == normMode(q.mode))
                { found = true; break; }
            }
            if (!found) {
                const Qso& best = cands.first();
                log(tr("  NOMATCH %1: eQSL[data=%2 banda=%3 modo=%4] "
                       "vs locale[data=%5 banda=%6 modo=%7]")
                    .arg(q.callsign, q.date, q.band, q.mode,
                         best.date, best.band, best.mode));
                ++diagShown;
            }
        }
        log(tr("--- Fine diagnostica ---"));
    }

    QList<Qso> matched;
    for (const Qso& q : inboxQsos) {
        QString qBand = q.band.toLower();
        QString qMode = normMode(q.mode);
        for (const Qso& ex : Database::instance().searchQsos(q.callsign)) {
            if (ex.date == q.date &&
                ex.band.toLower() == qBand &&
                normMode(ex.mode)  == qMode)
            {
                Database::instance().markEqslRcvd({ex.id}, "Y");
                matched.append(q);
                break;
            }
        }
    }
    log(tr("Aggiornati %1 / %2 QSO nel log come confermati eQSL.")
        .arg(matched.size()).arg(inboxQsos.size()));
    if (!matched.isEmpty()) emit qsosUpdated();

    // Salva la data odierna — il prossimo download partirà da qui
    cfg.setValue("eqsl/lastInboxDate",
                 QDate::currentDate().toString("yyyy-MM-dd"));

    // Step 3: scarica cartoline solo per i QSO matchati nel log locale
    if (matched.isEmpty()) {
        log(tr("Nessun QSO locale corrispondente — nessuna cartolina da scaricare."));
        setBusy(false);
        svc->deleteLater();
        return;
    }

    QString imgDir = QStandardPaths::writableLocation(
                         QStandardPaths::AppDataLocation) + "/eqsl_cards";
    log(tr("Download cartoline per %1 QSO in: %2").arg(matched.size()).arg(imgDir));

    // Switch progress bar to determinate percentage mode
    m_progress->setRange(0, 100);
    m_progress->setValue(0);

    connect(svc, &EqslService::progressPct, this, [this](int pct) {
        m_progress->setValue(pct);
    });
    connect(svc, &EqslService::imagesComplete, this,
            [this](int dl, int total) {
                log(tr("Cartoline scaricate: %1 / %2").arg(dl).arg(total));
                m_progress->setValue(100);
            });
    connect(svc, &EqslService::imageDownloaded, this,
            [this](const QString&, const QString&, const QString&, const QString& path) {
                addEqslCard(path);
            });

    QString myCall = QSettings().value("station/callsign", user).toString().toUpper();
    svc->fetchImages(user, pass, matched, imgDir, myCall);

    setBusy(false);
    svc->deleteLater();

    // Switch to gallery tab and reload if cards are now on disk
    if (m_eqslGallery && m_eqslGallery->count() == 0) {
        const auto entries = QDir(imgDir).entryInfoList(
                                 {"*.jpg","*.png"}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : entries)
            addEqslCard(fi.filePath());
    }
    if (m_eqslGallery && m_eqslGallery->count() > 0)
        m_tabs->setCurrentIndex(static_cast<int>(Tab::eQSL_Gallery));
}

// ---------------------------------------------------------------------------
// ClubLog Upload
// ---------------------------------------------------------------------------
void OnlineDialog::onClublogUpload()
{
    if (m_busy) return;
    QSettings cfg;
    QString email = cfg.value("clublog/email").toString();
    QString pass  = cfg.value("clublog/password").toString();
    QString call  = cfg.value("clublog/callsign").toString();
    if (email.isEmpty()) { log(tr("Email ClubLog non configurata")); return; }

    QList<Qso> qsos = Database::instance().getAllQsos();
    QTemporaryFile tmp;
    if (!tmp.open()) { log(tr("Impossibile creare il file temporaneo")); return; }
    AdifHandler::exportAdif(tmp.fileName(), qsos, call);
    tmp.seek(0);
    QString adif = QString::fromUtf8(tmp.readAll());

    setBusy(true, 0);
    auto* svc = new ClublogService(this);
    m_activeSvc = svc;
    connect(svc, &ClublogService::progress, this, &OnlineDialog::log);
    connect(svc, &ClublogService::uploadFinished, this,
            [this, svc](bool ok, const QString& msg) {
                setBusy(false);
                log(tr("ClubLog: %1 — %2").arg(ok ? tr("OK") : tr("Errore")).arg(msg));
                svc->deleteLater();
            });
    svc->uploadAdif(email, pass, call, adif);
}

// ---------------------------------------------------------------------------
// HamQTH — test & save
// ---------------------------------------------------------------------------
void OnlineDialog::addEqslCard(const QString& path)
{
    QPixmap pix(path);
    if (pix.isNull() || !m_eqslGallery) return;
    QIcon icon(pix.scaled(180, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QString label = QFileInfo(path).baseName().replace('_', ' ');
    auto* item = new QListWidgetItem(icon, label, m_eqslGallery);
    item->setData(Qt::UserRole, path);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(196, 148));
}

void OnlineDialog::onHamqthTest()
{
    QString u = m_hqUser->text().trimmed();
    QString p = m_hqPass->text();
    if (u.isEmpty() || p.isEmpty()) {
        m_hqStatus->setStyleSheet("color:#c0392b; font-weight:bold;");
        m_hqStatus->setText(tr("Inserisci nominativo e password."));
        return;
    }
    m_hqStatus->setStyleSheet("color:#7f8c8d;");
    m_hqStatus->setText(tr("Connessione in corso…"));
    qApp->processEvents();

    HamQthService svc;
    svc.setCredentials(u, p);
    auto result = svc.lookupSync(u);

    if (result.contains("_error")) {
        m_hqStatus->setStyleSheet("color:#c0392b; font-weight:bold;");
        m_hqStatus->setText(tr("Errore: %1").arg(result["_error"]));
        log(tr("HamQTH: login fallito — %1").arg(result["_error"]));
    } else if (result.isEmpty()) {
        m_hqStatus->setStyleSheet("color:#e67e22; font-weight:bold;");
        m_hqStatus->setText(tr("Login OK — nessun dato per %1").arg(u));
        log(tr("HamQTH: autenticazione OK, nominativo %1 non trovato").arg(u));
    } else {
        QString name = result.value("name", result.value("fname", u));
        m_hqStatus->setStyleSheet("color:#27ae60; font-weight:bold;");
        m_hqStatus->setText(tr("✓ Connessione riuscita! (%1)").arg(name));
        log(tr("HamQTH: connesso come %1 (%2)").arg(u, name));
    }
}

void OnlineDialog::onHamqthSave()
{
    QSettings cfg;
    cfg.setValue("hamqth/username", m_hqUser->text().trimmed());
    cfg.setValue("hamqth/password", m_hqPass->text());
    m_hqStatus->setStyleSheet("color:#27ae60; font-weight:bold;");
    m_hqStatus->setText(tr("✓ Credenziali salvate."));
    log(tr("HamQTH: credenziali salvate per %1").arg(m_hqUser->text().trimmed()));
}
