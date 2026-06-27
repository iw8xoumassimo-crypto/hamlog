#include "duplicates_dialog.h"
#include "../database.h"
#include <QTableWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QColor>
#include <QBrush>

DuplicatesDialog::DuplicatesDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Gestione Duplicati QSO"));
    setMinimumSize(760, 480);
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);

    auto* vl = new QVBoxLayout(this);

    // Info label
    auto* info = new QLabel(
        tr("I duplicati sono QSO con lo stesso nominativo, data, banda e modo.\n"
           "\"Selezione auto\" spunta tutte le copie da eliminare, mantenendo solo il primo."), this);
    info->setWordWrap(true);
    vl->addWidget(info);

    // Toolbar buttons
    auto* hl = new QHBoxLayout;
    auto* btnRefresh = new QPushButton(tr("⟳  Aggiorna"), this);
    auto* btnAuto    = new QPushButton(tr("✔  Selezione auto duplicati"), this);
    auto* btnDelete  = new QPushButton(tr("✖  Elimina selezionati"), this);
    btnDelete->setStyleSheet("QPushButton { color: #cc0000; font-weight: bold; }");
    m_status = new QLabel(this);
    hl->addWidget(btnRefresh);
    hl->addWidget(btnAuto);
    hl->addWidget(btnDelete);
    hl->addStretch();
    hl->addWidget(m_status);
    vl->addLayout(hl);

    // Table: checkbox | ID | Callsign | Date | Time | Band | Mode | Name
    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels({
        tr("Elim"), tr("ID"), tr("Nominativo"), tr("Data"), tr("Ora"),
        tr("Banda"), tr("Modo"), tr("Nome")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 40);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(false);
    vl->addWidget(m_table);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    connect(btnRefresh, &QPushButton::clicked, this, &DuplicatesDialog::onRefresh);
    connect(btnAuto,    &QPushButton::clicked, this, &DuplicatesDialog::onAutoSelect);
    connect(btnDelete,  &QPushButton::clicked, this, &DuplicatesDialog::onDeleteSelected);

    onRefresh();
}

void DuplicatesDialog::onRefresh()
{
    auto dups = Database::instance().findDuplicateQsos();
    m_table->setRowCount(0);

    // Group duplicates by (callsign+date+band+mode) for color coding
    // dups contains: id, callsign, date, time_on, band, mode, dup_id
    // Each row is a QSO that has at least one duplicate
    // We use alternating pastel colors per group
    static const QColor groupColors[] = {
        QColor(0xff,0xf0,0xe0),   // peach
        QColor(0xe0,0xf0,0xff),   // light blue
        QColor(0xe0,0xff,0xe8),   // mint
        QColor(0xff,0xe0,0xff),   // lavender — note: intentionally repeats after 3
    };
    static const int NUM_COLORS = 3;

    int groupIdx  = -1;
    QString lastKey;

    for (const auto& d : dups) {
        QString key = d["callsign"].toString() + "|" +
                      d["date"].toString()     + "|" +
                      d["band"].toString()     + "|" +
                      d["mode"].toString();
        if (key != lastKey) { ++groupIdx; lastKey = key; }

        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Column 0: checkbox
        auto* chk = new QCheckBox(this);
        m_table->setCellWidget(row, 0, chk);

        // Store QSO id so we can delete it
        auto makeItem = [&](const QString& txt) {
            auto* item = new QTableWidgetItem(txt);
            item->setData(Qt::UserRole, d["id"].toInt());
            return item;
        };
        m_table->setItem(row, 1, makeItem(d["id"].toString()));
        m_table->setItem(row, 2, makeItem(d["callsign"].toString()));
        m_table->setItem(row, 3, makeItem(d["date"].toString()));
        m_table->setItem(row, 4, makeItem(d["time_on"].toString()));
        m_table->setItem(row, 5, makeItem(d["band"].toString()));
        m_table->setItem(row, 6, makeItem(d["mode"].toString()));
        m_table->setItem(row, 7, makeItem(d["name"].toString()));

        // Color the whole row by group
        QColor bg = groupColors[groupIdx % NUM_COLORS];
        for (int c = 1; c < 8; ++c)
            m_table->item(row, c)->setBackground(QBrush(bg));
    }

    int total = dups.size();
    m_status->setText(total > 0
        ? tr("<b>%1</b> duplicato/i trovato/i").arg(total)
        : tr("<b style='color:green'>Nessun duplicato</b>"));
}

void DuplicatesDialog::onAutoSelect()
{
    // For each group of rows sharing the same (call+date+band+mode),
    // tick all EXCEPT the row with the lowest ID (= the first/original).
    // We scan the table top-to-bottom; the first row of a group (lowest ID)
    // is left unchecked, all subsequent ones in the group are checked.

    QString lastKey;
    bool firstInGroup = true;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QString call = m_table->item(row, 2)->text();
        QString date = m_table->item(row, 3)->text();
        QString band = m_table->item(row, 5)->text();
        QString mode = m_table->item(row, 6)->text();
        QString key  = call + "|" + date + "|" + band + "|" + mode;

        if (key != lastKey) { lastKey = key; firstInGroup = true; }

        auto* chk = qobject_cast<QCheckBox*>(m_table->cellWidget(row, 0));
        if (chk) chk->setChecked(!firstInGroup);   // keep the first, mark the rest

        firstInGroup = false;
    }
}

void DuplicatesDialog::onDeleteSelected()
{
    QList<int> ids;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* chk = qobject_cast<QCheckBox*>(m_table->cellWidget(row, 0));
        if (chk && chk->isChecked())
            ids.append(m_table->item(row, 1)->data(Qt::UserRole).toInt());
    }

    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Elimina"), tr("Nessuna riga selezionata."));
        return;
    }

    int ret = QMessageBox::question(this, tr("Elimina duplicati"),
        tr("Eliminare %1 QSO selezionati?").arg(ids.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    int deleted = 0;
    for (int id : ids)
        if (Database::instance().deleteQso(id)) ++deleted;

    m_status->setText(tr("Eliminati %1 QSO").arg(deleted));
    onRefresh();
}
