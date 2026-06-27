#include "awards_dialog.h"
#include "../database.h"
#include "../dxcc.h"
#include "types.h"
#include <QComboBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QColor>
#include <QBrush>

AwardsDialog::AwardsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Diplomi e Statistiche"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(680, 540);
    build();
    onFilterChanged();
}

void AwardsDialog::build()
{
    auto* vl = new QVBoxLayout(this);

    // Filter bar (shared across tabs)
    auto* hl = new QHBoxLayout;
    m_band = new QComboBox(this);
    m_band->addItem(tr("Tutte le bande"), QString());
    for (const QString& b : {"160m","80m","40m","20m","15m","10m","6m","2m"})
        m_band->addItem(b, b);
    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Tutti i modi"), QString());
    for (const QString& m : {"CW","SSB","FT8","FT4","RTTY"})
        m_mode->addItem(m, m);
    m_showAll = new QCheckBox(tr("Mostra non lavorate"), this);

    hl->addWidget(new QLabel(tr("Banda:")));
    hl->addWidget(m_band);
    hl->addWidget(new QLabel(tr("Modo:")));
    hl->addWidget(m_mode);
    hl->addWidget(m_showAll);
    hl->addStretch();
    vl->addLayout(hl);

    // Tabs
    m_tabs = new QTabWidget(this);

    // --- Tab 1: DXCC ---
    auto* dxccW  = new QWidget(m_tabs);
    auto* dxccVl = new QVBoxLayout(dxccW);
    m_lblDxcc    = new QLabel(dxccW);
    m_lblDxcc->setStyleSheet("font-weight:bold; padding:2px;");
    dxccVl->addWidget(m_lblDxcc);
    m_dxccTable = new QTableWidget(0, 5, dxccW);
    m_dxccTable->setHorizontalHeaderLabels(
        {tr("Entità"), tr("CQ"), tr("ITU"), tr("Lavorata"), tr("Confermata")});
    m_dxccTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_dxccTable->verticalHeader()->hide();
    m_dxccTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dxccTable->setAlternatingRowColors(true);
    dxccVl->addWidget(m_dxccTable);
    m_tabs->addTab(dxccW, tr("DXCC"));

    // --- Tab 2: CQ Zones (WAZ) ---
    auto* cqW  = new QWidget(m_tabs);
    auto* cqVl = new QVBoxLayout(cqW);
    m_lblCq    = new QLabel(cqW);
    m_lblCq->setStyleSheet("font-weight:bold; padding:2px;");
    cqVl->addWidget(m_lblCq);
    m_cqTable = new QTableWidget(0, 3, cqW);
    m_cqTable->setHorizontalHeaderLabels(
        {tr("Zona CQ"), tr("Lavorata"), tr("Confermata")});
    m_cqTable->horizontalHeader()->setStretchLastSection(true);
    m_cqTable->verticalHeader()->hide();
    m_cqTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cqTable->setAlternatingRowColors(true);
    cqVl->addWidget(m_cqTable);
    m_tabs->addTab(cqW, tr("Zone CQ (WAZ)"));

    // --- Tab 3: ITU Zones ---
    auto* ituW  = new QWidget(m_tabs);
    auto* ituVl = new QVBoxLayout(ituW);
    m_lblItu    = new QLabel(ituW);
    m_lblItu->setStyleSheet("font-weight:bold; padding:2px;");
    ituVl->addWidget(m_lblItu);
    m_ituTable = new QTableWidget(0, 3, ituW);
    m_ituTable->setHorizontalHeaderLabels(
        {tr("Zona ITU"), tr("Lavorata"), tr("Confermata")});
    m_ituTable->horizontalHeader()->setStretchLastSection(true);
    m_ituTable->verticalHeader()->hide();
    m_ituTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ituTable->setAlternatingRowColors(true);
    ituVl->addWidget(m_ituTable);
    m_tabs->addTab(ituW, tr("Zone ITU"));

    vl->addWidget(m_tabs);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    connect(m_band,    QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AwardsDialog::onFilterChanged);
    connect(m_mode,    QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AwardsDialog::onFilterChanged);
    connect(m_showAll, &QCheckBox::toggled, this, &AwardsDialog::onFilterChanged);
}

void AwardsDialog::populateDxcc()
{
    QString band = m_band->currentData().toString();
    QString mode = m_mode->currentData().toString();
    bool showAll = m_showAll->isChecked();

    auto worked    = Database::instance().workedEntities(band, mode);
    auto confirmed = Database::instance().confirmedEntities(band, mode);
    auto entities  = Dxcc::allEntities();

    m_dxccTable->setRowCount(0);
    int workedCount = 0, confirmedCount = 0;

    for (const QString& entity : entities) {
        bool w = worked.contains(entity);
        bool c = confirmed.contains(entity);
        if (!w && !showAll) continue;

        int row = m_dxccTable->rowCount();
        m_dxccTable->insertRow(row);

        DxccInfo info = Dxcc::lookup(entity);
        m_dxccTable->setItem(row, 0, new QTableWidgetItem(entity));
        m_dxccTable->setItem(row, 1, new QTableWidgetItem(
            info.cqZone  ? QString::number(info.cqZone)  : ""));
        m_dxccTable->setItem(row, 2, new QTableWidgetItem(
            info.ituZone ? QString::number(info.ituZone) : ""));
        m_dxccTable->setItem(row, 3, new QTableWidgetItem(w ? tr("✓") : ""));
        m_dxccTable->setItem(row, 4, new QTableWidgetItem(c ? tr("✓") : ""));

        QColor bg;
        if (c)       bg = QColor(0xc8, 0xf8, 0xc8);  // confirmed: green
        else if (w)  bg = QColor(0xff, 0xff, 0xcc);  // worked: yellow
        else         bg = QColor(0xf8, 0xf0, 0xf0);  // not worked: light pink

        for (int col = 0; col < 5; ++col)
            if (m_dxccTable->item(row,col))
                m_dxccTable->item(row,col)->setBackground(QBrush(bg));

        if (w) ++workedCount;
        if (c) ++confirmedCount;
    }

    m_lblDxcc->setText(tr("DXCC — Lavorate: <b>%1</b>  Confermate: <b>%2</b> / %3 entità totali")
        .arg(workedCount).arg(confirmedCount).arg(Dxcc::entityCount()));
}

void AwardsDialog::populateCqZones()
{
    QString band = m_band->currentData().toString();
    QString mode = m_mode->currentData().toString();
    bool showAll = m_showAll->isChecked();

    QsoFilter f;
    f.band = band;
    f.mode = mode;
    auto qsos = Database::instance().getAllQsos(f);

    QHash<int,bool> workedZones;
    QHash<int,bool> confirmedZones;
    for (const Qso& q : qsos) {
        if (q.cqZone <= 0) continue;
        workedZones[q.cqZone] = true;
        if (q.lotwRcvd == "Y" || q.qslRcvd == "Y")
            confirmedZones[q.cqZone] = true;
    }

    m_cqTable->setRowCount(0);
    int worked = 0, confirmed = 0;

    for (int zone = 1; zone <= 40; ++zone) {
        bool w = workedZones.contains(zone);
        bool c = confirmedZones.contains(zone);
        if (!w && !showAll) continue;

        int row = m_cqTable->rowCount();
        m_cqTable->insertRow(row);
        m_cqTable->setItem(row, 0, new QTableWidgetItem(QString::number(zone)));
        m_cqTable->setItem(row, 1, new QTableWidgetItem(w ? tr("✓") : ""));
        m_cqTable->setItem(row, 2, new QTableWidgetItem(c ? tr("✓") : ""));

        QColor bg = c ? QColor(0xc8,0xf8,0xc8) : w ? QColor(0xff,0xff,0xcc) : QColor(0xf8,0xf0,0xf0);
        for (int col = 0; col < 3; ++col)
            if (m_cqTable->item(row,col))
                m_cqTable->item(row,col)->setBackground(QBrush(bg));

        if (w) ++worked;
        if (c) ++confirmed;
    }

    m_lblCq->setText(tr("WAZ — Zone CQ lavorate: <b>%1</b>  Confermate: <b>%2</b> / 40 zone")
        .arg(worked).arg(confirmed));
}

void AwardsDialog::populateItuZones()
{
    QString band = m_band->currentData().toString();
    QString mode = m_mode->currentData().toString();
    bool showAll = m_showAll->isChecked();

    QsoFilter f;
    f.band = band;
    f.mode = mode;
    auto qsos = Database::instance().getAllQsos(f);

    QHash<int,bool> workedZones;
    QHash<int,bool> confirmedZones;
    for (const Qso& q : qsos) {
        if (q.ituZone <= 0) continue;
        workedZones[q.ituZone] = true;
        if (q.lotwRcvd == "Y" || q.qslRcvd == "Y")
            confirmedZones[q.ituZone] = true;
    }

    m_ituTable->setRowCount(0);
    int worked = 0, confirmed = 0;

    for (int zone = 1; zone <= 90; ++zone) {
        bool w = workedZones.contains(zone);
        bool c = confirmedZones.contains(zone);
        if (!w && !showAll) continue;

        int row = m_ituTable->rowCount();
        m_ituTable->insertRow(row);
        m_ituTable->setItem(row, 0, new QTableWidgetItem(QString::number(zone)));
        m_ituTable->setItem(row, 1, new QTableWidgetItem(w ? tr("✓") : ""));
        m_ituTable->setItem(row, 2, new QTableWidgetItem(c ? tr("✓") : ""));

        QColor bg = c ? QColor(0xc8,0xf8,0xc8) : w ? QColor(0xff,0xff,0xcc) : QColor(0xf8,0xf0,0xf0);
        for (int col = 0; col < 3; ++col)
            if (m_ituTable->item(row,col))
                m_ituTable->item(row,col)->setBackground(QBrush(bg));

        if (w) ++worked;
        if (c) ++confirmed;
    }

    m_lblItu->setText(tr("Zone ITU lavorate: <b>%1</b>  Confermate: <b>%2</b> / 90 zone")
        .arg(worked).arg(confirmed));
}

void AwardsDialog::onFilterChanged()
{
    populateDxcc();
    populateCqZones();
    populateItuZones();
}
