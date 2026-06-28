#include "contest_dialog.h"
#include "../database.h"
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSettings>
#include <QDateTime>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>

ContestDialog::ContestDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Modalità Contest"));
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    resize(800, 600);

    auto* vl = new QVBoxLayout(this);

    // Contest info
    auto* topGrp  = new QGroupBox(tr("Contest"), this);
    auto* topForm = new QFormLayout(topGrp);
    m_contestName = new QLineEdit(this);
    m_contestName->setPlaceholderText(tr("es. CQ WW DX SSB"));
    m_exSent = new QLineEdit(this);
    m_exSent->setPlaceholderText(tr("es. 599 14"));
    topForm->addRow(tr("Nome contest:"),     m_contestName);
    topForm->addRow(tr("Exchange inviato:"), m_exSent);
    vl->addWidget(topGrp);

    // Inserimento QSO
    auto* entryGrp  = new QGroupBox(tr("Inserimento QSO"), this);
    auto* entryForm = new QFormLayout(entryGrp);

    auto* bandMode = new QHBoxLayout;
    m_band = new QComboBox(this);
    m_band->addItems({"160M","80M","60M","40M","30M","20M","17M","15M","12M","10M","6M","2M"});
    m_band->setCurrentText("20M");
    m_mode = new QComboBox(this);
    m_mode->addItems({"SSB","CW","FT8","FT4","RTTY","PSK31"});
    bandMode->addWidget(m_band);
    bandMode->addWidget(m_mode);
    entryForm->addRow(tr("Banda/Modo:"), bandMode);

    m_callsign = new QLineEdit(this);
    m_callsign->setPlaceholderText(tr("Nominativo"));
    m_callsign->setFont(QFont("Courier New", 14, QFont::Bold));
    m_rst = new QLineEdit("599", this);
    m_rst->setMaximumWidth(60);
    m_exRcvd = new QLineEdit(this);
    m_exRcvd->setPlaceholderText(tr("Exchange ricevuto, es. 001 JN71"));
    m_serial  = new QSpinBox(this);
    m_serial->setRange(1, 9999);
    m_serial->setValue(1);

    entryForm->addRow(tr("Nominativo:"),    m_callsign);
    entryForm->addRow(tr("RST:"),           m_rst);
    entryForm->addRow(tr("Exchange rx:"),   m_exRcvd);
    entryForm->addRow(tr("Seriale inviato:"), m_serial);

    // Dupe indicator
    m_dupeLabel = new QLabel(this);
    m_dupeLabel->setFont(QFont("Arial", 11, QFont::Bold));
    entryForm->addRow(tr("Stato:"), m_dupeLabel);

    vl->addWidget(entryGrp);

    // Pulsante Log (e shortcut Enter)
    auto* btnLog = new QPushButton(tr("Registra QSO  [Invio]"), this);
    btnLog->setDefault(true);
    btnLog->setFixedHeight(36);
    QFont bf = btnLog->font(); bf.setBold(true); btnLog->setFont(bf);
    vl->addWidget(btnLog);

    // Score
    m_scoreLabel = new QLabel(tr("QSO: 0  Punti: 0"), this);
    QFont sf = m_scoreLabel->font(); sf.setBold(true); sf.setPointSize(11);
    m_scoreLabel->setFont(sf);
    vl->addWidget(m_scoreLabel);

    // Log mini
    m_log = new QTableWidget(0, 6, this);
    m_log->setHorizontalHeaderLabels({tr("#"), tr("Ora"), tr("Nominativo"),
                                      tr("Banda"), tr("Modo"), tr("Exchange")});
    m_log->horizontalHeader()->setStretchLastSection(true);
    m_log->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_log->setAlternatingRowColors(true);
    m_log->verticalHeader()->hide();
    vl->addWidget(m_log, 1);

    connect(btnLog,      &QPushButton::clicked,           this, &ContestDialog::onLog);
    connect(m_callsign,  &QLineEdit::textChanged,         this, &ContestDialog::onCallsignChanged);
    connect(m_callsign,  &QLineEdit::returnPressed,       this, &ContestDialog::onLog);

    // Carica exchange inviato da settings
    QSettings cfg;
    m_exSent->setText(cfg.value("contest/exSent").toString());
    m_contestName->setText(cfg.value("contest/name").toString());
    m_serial->setValue(cfg.value("contest/serial", 1).toInt());
}

bool ContestDialog::isDupe(const QString& callsign) const
{
    for (const Qso& q : m_qsos)
        if (q.callsign.toUpper() == callsign.toUpper() &&
            q.band == m_band->currentText())
            return true;
    return false;
}

void ContestDialog::onCallsignChanged(const QString& cs)
{
    if (cs.trimmed().isEmpty()) {
        m_dupeLabel->setText(QString());
        return;
    }
    if (isDupe(cs.trimmed())) {
        m_dupeLabel->setText(tr("⚠ DUPLICATO sulla banda %1").arg(m_band->currentText()));
        m_dupeLabel->setStyleSheet("color: red;");
    } else {
        m_dupeLabel->setText(tr("✓ Nuovo"));
        m_dupeLabel->setStyleSheet("color: green;");
    }
}

void ContestDialog::onLog()
{
    QString cs = m_callsign->text().trimmed().toUpper();
    if (cs.isEmpty()) return;

    if (isDupe(cs)) {
        if (QMessageBox::question(this, tr("Duplicato"),
                tr("%1 già loggato su %2. Registrare comunque?")
                .arg(cs).arg(m_band->currentText()),
                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    QDateTime now = QDateTime::currentDateTimeUtc();
    Qso q;
    q.callsign  = cs;
    q.date      = now.toString("yyyy-MM-dd");
    q.timeOn    = now.toString("HHmm");
    q.timeOff   = q.timeOn;
    q.band      = m_band->currentText();
    q.mode      = m_mode->currentText();
    q.rstSent   = m_rst->text().trimmed().isEmpty() ? "599" : m_rst->text().trimmed();
    q.rstRcvd   = m_rst->text().trimmed().isEmpty() ? "599" : m_rst->text().trimmed();
    q.comment   = QString("Contest: %1 | Ex sent: %2 %3 | Ex rcvd: %4")
                      .arg(m_contestName->text())
                      .arg(m_exSent->text())
                      .arg(m_serial->value())
                      .arg(m_exRcvd->text());

    Database::instance().saveQso(q);
    m_qsos.append(q);
    m_serial->setValue(m_serial->value() + 1);

    // Aggiorna mini log
    int row = 0;
    m_log->insertRow(0);
    m_log->setItem(0, 0, new QTableWidgetItem(QString::number(m_qsos.size())));
    m_log->setItem(0, 1, new QTableWidgetItem(q.timeOn));
    m_log->setItem(0, 2, new QTableWidgetItem(q.callsign));
    m_log->setItem(0, 3, new QTableWidgetItem(q.band));
    m_log->setItem(0, 4, new QTableWidgetItem(q.mode));
    m_log->setItem(0, 5, new QTableWidgetItem(m_exRcvd->text()));

    // Pulisci per prossimo QSO
    m_callsign->clear();
    m_exRcvd->clear();
    m_callsign->setFocus();

    updateScore();

    // Salva stato
    QSettings cfg;
    cfg.setValue("contest/name",    m_contestName->text());
    cfg.setValue("contest/exSent",  m_exSent->text());
    cfg.setValue("contest/serial",  m_serial->value());
}

void ContestDialog::updateScore()
{
    // Punteggio semplice: 1 punto per QSO, bonus per DXCC diversi
    QSet<QString> countries;
    for (const Qso& q : m_qsos)
        if (!q.country.isEmpty()) countries.insert(q.country);

    m_score = m_qsos.size() * (countries.isEmpty() ? 1 : countries.size());
    m_scoreLabel->setText(tr("QSO: %1   Moltiplicatori: %2   Punti: %3")
                              .arg(m_qsos.size()).arg(countries.size()).arg(m_score));
}
