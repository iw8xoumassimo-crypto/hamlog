#include "integrity_dialog.h"
#include "../database.h"
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QMessageBox>

IntegrityDialog::IntegrityDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Controllo Integrità Database"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(720, 500);

    auto* vl = new QVBoxLayout(this);

    // Summary group
    auto* sumGrp = new QGroupBox(tr("Riepilogo"), this);
    auto* sumVl  = new QVBoxLayout(sumGrp);
    m_summary = new QLabel(tr("Premi «Riepilogo» per analizzare il database."), sumGrp);
    m_summary->setWordWrap(true);
    sumVl->addWidget(m_summary);
    vl->addWidget(sumGrp);

    // Action buttons
    auto* actGrp = new QGroupBox(tr("Operazioni"), this);
    auto* gl     = new QGridLayout(actGrp);

    auto* btnSummary = new QPushButton(tr("Riepilogo DB"), actGrp);
    auto* btnFind    = new QPushButton(tr("Trova duplicati"), actGrp);
    auto* btnRemove  = new QPushButton(tr("Rimuovi duplicati"), actGrp);
    auto* btnEnrich  = new QPushButton(tr("Integra DXCC mancanti"), actGrp);
    auto* btnMissing = new QPushButton(tr("Campi obbligatori mancanti"), actGrp);

    btnSummary->setToolTip(tr("Mostra statistiche generali del database"));
    btnFind   ->setToolTip(tr("Elenca QSO duplicati (stesso nominativo/data/banda/modo)"));
    btnRemove ->setToolTip(tr("Elimina i duplicati mantenendo il primo"));
    btnEnrich ->setToolTip(tr("Aggiorna paese/continente/zona per i QSO privi di dati DXCC"));
    btnMissing->setToolTip(tr("Trova QSO con nominativo, data, banda o modo mancante"));

    gl->addWidget(btnSummary, 0, 0);
    gl->addWidget(btnFind,    0, 1);
    gl->addWidget(btnRemove,  0, 2);
    gl->addWidget(btnEnrich,  1, 0);
    gl->addWidget(btnMissing, 1, 1);
    vl->addWidget(actGrp);

    // Status label
    m_status = new QLabel(this);
    m_status->setStyleSheet("font-weight:bold; padding:2px;");
    vl->addWidget(m_status);

    // Results table
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({tr("ID"), tr("Nominativo"), tr("Data"),
                                        tr("Ora"), tr("Banda"), tr("Modo")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    vl->addWidget(m_table);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    connect(btnSummary, &QPushButton::clicked, this, &IntegrityDialog::onSummary);
    connect(btnFind,    &QPushButton::clicked, this, &IntegrityDialog::onFindDuplicates);
    connect(btnRemove,  &QPushButton::clicked, this, &IntegrityDialog::onRemoveDuplicates);
    connect(btnEnrich,  &QPushButton::clicked, this, &IntegrityDialog::onEnrichDxcc);
    connect(btnMissing, &QPushButton::clicked, this, &IntegrityDialog::onCheckMissingFields);

    // Show summary on open
    onSummary();
}

void IntegrityDialog::onSummary()
{
    int total   = Database::instance().countQsos();
    auto stats  = Database::instance().getStats();
    int noName  = 0;
    int noGrid  = 0;
    int noDxcc  = 0;

    auto qsos = Database::instance().getAllQsos();
    for (const Qso& q : qsos) {
        if (q.name.isEmpty())      ++noName;
        if (q.gridsquare.isEmpty())++noGrid;
        if (q.country.isEmpty())   ++noDxcc;
    }

    m_summary->setText(
        tr("<b>Totale QSO:</b> %1 &nbsp;&nbsp; "
           "<b>Nominativi unici:</b> %2 &nbsp;&nbsp; "
           "<b>Paesi:</b> %3 &nbsp;&nbsp; "
           "<b>Zone CQ:</b> %4<br>"
           "<b>Senza nome:</b> %5 &nbsp;&nbsp; "
           "<b>Senza grid:</b> %6 &nbsp;&nbsp; "
           "<b>Senza dati DXCC:</b> %7")
        .arg(total).arg(stats.uniqueCalls).arg(stats.countries).arg(stats.cqZones)
        .arg(noName).arg(noGrid).arg(noDxcc)
    );
    m_table->setRowCount(0);
    m_status->clear();
}

void IntegrityDialog::onFindDuplicates()
{
    auto dups = Database::instance().findDuplicates();
    m_table->setRowCount(0);
    for (const auto& d : dups) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row,0, new QTableWidgetItem(d["id"].toString()));
        m_table->setItem(row,1, new QTableWidgetItem(d["callsign"].toString()));
        m_table->setItem(row,2, new QTableWidgetItem(d["date"].toString()));
        m_table->setItem(row,3, new QTableWidgetItem(d["time_on"].toString()));
        m_table->setItem(row,4, new QTableWidgetItem(d["band"].toString()));
        m_table->setItem(row,5, new QTableWidgetItem(d["mode"].toString()));
    }
    m_status->setText(tr("Duplicati trovati: %1").arg(dups.size()));
}

void IntegrityDialog::onRemoveDuplicates()
{
    if (QMessageBox::question(this, tr("Rimuovi duplicati"),
            tr("Rimuovere tutti i QSO duplicati (mantenendo il primo per ogni gruppo)?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    int n = Database::instance().removeDuplicates();
    m_status->setText(tr("Rimossi %1 duplicato/i").arg(n));
    onFindDuplicates();
    onSummary();
}

void IntegrityDialog::onEnrichDxcc()
{
    int n = Database::instance().enrichMissingDxcc();
    m_status->setText(tr("Aggiornati dati DXCC per %1 QSO").arg(n));
    onSummary();
}

void IntegrityDialog::onCheckMissingFields()
{
    auto qsos = Database::instance().getAllQsos();
    m_table->setRowCount(0);
    int found = 0;
    for (const Qso& q : qsos) {
        bool missing = q.callsign.isEmpty() || q.date.isEmpty()
                    || q.band.isEmpty()     || q.mode.isEmpty();
        if (!missing) continue;
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row,0, new QTableWidgetItem(QString::number(q.id)));
        m_table->setItem(row,1, new QTableWidgetItem(q.callsign.isEmpty()
                                                     ? tr("‼ mancante") : q.callsign));
        m_table->setItem(row,2, new QTableWidgetItem(q.date));
        m_table->setItem(row,3, new QTableWidgetItem(q.timeOn));
        m_table->setItem(row,4, new QTableWidgetItem(q.band.isEmpty()
                                                     ? tr("‼ mancante") : q.band));
        m_table->setItem(row,5, new QTableWidgetItem(q.mode.isEmpty()
                                                     ? tr("‼ mancante") : q.mode));
        ++found;
    }
    m_status->setText(tr("QSO con campi mancanti: %1").arg(found));
}
