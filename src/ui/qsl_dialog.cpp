#include "qsl_dialog.h"
#include "../database.h"
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QColor>
#include <QBrush>
#include <QDesktopServices>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrl>
#include <QDir>
#include <QSettings>
#include <QDateTime>

QslDialog::QslDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Gestione QSL"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(700, 480);
    build();
    populate();
}

void QslDialog::build()
{
    auto* vl = new QVBoxLayout(this);

    auto* hl = new QHBoxLayout;
    m_filter = new QComboBox(this);
    m_filter->addItem(tr("In attesa (inviata, non ricevuta)"), "pending");
    m_filter->addItem(tr("Tutti senza QSL"),                   "none");
    m_filter->addItem(tr("Tutti"),                             "all");

    auto* btnSent  = new QPushButton(tr("Segna QSL Inviata"),  this);
    auto* btnRcvd  = new QPushButton(tr("Segna QSL Ricevuta"), this);
    auto* btnPrint = new QPushButton(tr("Stampa lista…"), this);
    btnPrint->setToolTip(tr("Esporta la lista corrente in HTML e apre il browser per la stampa"));

    hl->addWidget(new QLabel(tr("Mostra:")));
    hl->addWidget(m_filter);
    hl->addStretch();
    hl->addWidget(btnSent);
    hl->addWidget(btnRcvd);
    hl->addWidget(btnPrint);
    vl->addLayout(hl);

    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels({tr("Nominativo"), tr("Data"), tr("Banda"), tr("Modo"),
                                        tr("QSL↑"), tr("QSL↓"), tr("LoTW↓"), tr("eQSL↓")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    vl->addWidget(m_table);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    connect(m_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QslDialog::onFilter);
    connect(btnSent,  &QPushButton::clicked, this, &QslDialog::onMarkSent);
    connect(btnRcvd,  &QPushButton::clicked, this, &QslDialog::onMarkRcvd);
    connect(btnPrint, &QPushButton::clicked, this, &QslDialog::onPrint);
}

void QslDialog::populate()
{
    QString mode = m_filter->currentData().toString();
    QsoFilter f;
    QList<Qso> qsos = Database::instance().getAllQsos(f);

    m_table->setRowCount(0);
    for (const Qso& q : qsos) {
        bool hasSent = q.qslSent == "Y";
        bool hasRcvd = q.qslRcvd == "Y" || q.lotwRcvd == "Y" || q.eqslRcvd == "Y";

        if (mode == "pending" && !(hasSent && !hasRcvd)) continue;
        if (mode == "none"    && (hasSent || hasRcvd))   continue;

        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row,0, new QTableWidgetItem(q.callsign));
        m_table->setItem(row,1, new QTableWidgetItem(q.date));
        m_table->setItem(row,2, new QTableWidgetItem(q.band));
        m_table->setItem(row,3, new QTableWidgetItem(q.mode));
        m_table->setItem(row,4, new QTableWidgetItem(q.qslSent));
        m_table->setItem(row,5, new QTableWidgetItem(q.qslRcvd));
        m_table->setItem(row,6, new QTableWidgetItem(q.lotwRcvd));
        m_table->setItem(row,7, new QTableWidgetItem(q.eqslRcvd));

        // Store QSO id in item
        m_table->item(row,0)->setData(Qt::UserRole, q.id);

        QColor bg = hasRcvd ? QColor(0xd0,0xff,0xd0) :
                    hasSent ? QColor(0xff,0xff,0xd0) : QColor(0xff,0xf0,0xf0);
        for (int c=0; c<8; ++c)
            m_table->item(row,c)->setBackground(QBrush(bg));
    }
}

void QslDialog::onMarkSent()
{
    QList<int> ids;
    for (auto* item : m_table->selectedItems())
        if (item->column() == 0) ids.append(item->data(Qt::UserRole).toInt());
    if (!ids.isEmpty()) {
        Database::instance().markQslSent(ids, "Y");
        populate();
    }
}

void QslDialog::onMarkRcvd()
{
    QList<int> ids;
    for (auto* item : m_table->selectedItems())
        if (item->column() == 0) ids.append(item->data(Qt::UserRole).toInt());
    if (!ids.isEmpty()) {
        Database::instance().markQslRcvd(ids, "Y");
        populate();
    }
}

void QslDialog::onFilter() { populate(); }

void QslDialog::onPrint()
{
    QSettings cfg;
    QString myCall = cfg.value("station/callsign","N0CALL").toString();
    QString date   = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    QString title  = tr("Lista QSL – %1 – %2").arg(myCall, date);

    // Build HTML table
    QString html = QString(R"(<!DOCTYPE html><html><head>
<meta charset="utf-8">
<title>%1</title>
<style>
  body { font-family: Arial,sans-serif; font-size:11pt; }
  h2   { color:#1e3a5f; }
  table{ border-collapse:collapse; width:100%%; }
  th   { background:#2980b9; color:white; padding:5px 8px; }
  td   { border:1px solid #ddd; padding:4px 8px; }
  tr:nth-child(even){ background:#f5f5f5; }
  .sent{ background:#ffffc0; }
  .rcvd{ background:#c0ffc0; }
  .none{ background:#ffe0e0; }
</style></head><body>
<h2>%1</h2>
<table>
<tr><th>Nominativo</th><th>Data</th><th>Banda</th><th>Modo</th>
    <th>QSL↑</th><th>QSL↓</th><th>LoTW↓</th><th>eQSL↓</th></tr>
)").arg(title.toHtmlEscaped());

    for (int row = 0; row < m_table->rowCount(); ++row) {
        bool sent = (m_table->item(row,4) && m_table->item(row,4)->text() == "Y");
        bool rcvd = (m_table->item(row,5) && m_table->item(row,5)->text() == "Y")
                 || (m_table->item(row,6) && m_table->item(row,6)->text() == "Y")
                 || (m_table->item(row,7) && m_table->item(row,7)->text() == "Y");
        QString cls = rcvd ? "rcvd" : sent ? "sent" : "none";
        html += QString("<tr class=\"%1\">").arg(cls);
        for (int col = 0; col < 8; ++col) {
            QString txt = m_table->item(row,col) ? m_table->item(row,col)->text() : "";
            html += "<td>" + txt.toHtmlEscaped() + "</td>";
        }
        html += "</tr>\n";
    }
    html += QString("</table><p style=\"color:#888\">%1 voci — generato da HamLog</p></body></html>")
            .arg(m_table->rowCount());

    // Write to temp file and open browser
    QTemporaryFile tmp(QDir::tempPath() + "/hamlog_qsl_XXXXXX.html");
    tmp.setAutoRemove(false);
    if (tmp.open()) {
        QTextStream s(&tmp);
        s << html;
        tmp.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(tmp.fileName()));
    }
}
