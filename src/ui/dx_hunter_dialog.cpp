#include "dx_hunter_dialog.h"
#include "../database.h"
#include "../dxcc.h"
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QColor>
#include <QBrush>

DxHunterDialog::DxHunterDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("DX Hunter – Entità Mancanti"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(500, 450);
    build();
    populate();
}

void DxHunterDialog::build()
{
    auto* vl = new QVBoxLayout(this);

    auto* hl = new QHBoxLayout;
    m_band = new QComboBox(this);
    m_band->addItem(tr("Tutte"), QString());
    for (const QString& b : {"160m","80m","40m","20m","15m","10m","6m","2m"})
        m_band->addItem(b, b);
    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Tutti"), QString());
    for (const QString& m : {"CW","SSB","FT8","FT4","RTTY"})
        m_mode->addItem(m, m);
    hl->addWidget(new QLabel(tr("Banda:")));
    hl->addWidget(m_band);
    hl->addWidget(new QLabel(tr("Modo:")));
    hl->addWidget(m_mode);
    hl->addStretch();
    vl->addLayout(hl);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("Entità"), tr("Continente"), tr("Zona CQ"), tr("Stato")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vl->addWidget(m_table);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    connect(m_band, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DxHunterDialog::onFilter);
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DxHunterDialog::onFilter);
}

void DxHunterDialog::populate()
{
    QString band = m_band->currentData().toString();
    QString mode = m_mode->currentData().toString();

    auto worked = Database::instance().workedEntities(band, mode);
    auto all    = Dxcc::allEntities();

    m_table->setRowCount(0);
    for (const QString& entity : all) {
        if (worked.contains(entity)) continue;  // already worked
        DxccInfo info = Dxcc::lookup(entity);
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(entity));
        m_table->setItem(row, 1, new QTableWidgetItem(info.continent));
        m_table->setItem(row, 2, new QTableWidgetItem(info.cqZone ? QString::number(info.cqZone) : ""));
        m_table->setItem(row, 3, new QTableWidgetItem(tr("Mancante")));

        for (int c = 0; c < 4; ++c)
            m_table->item(row,c)->setBackground(QBrush(QColor(0xff,0xee,0xee)));
    }
}

void DxHunterDialog::onFilter() { populate(); }
