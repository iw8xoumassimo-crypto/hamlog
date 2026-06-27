#include "log_view.h"
#include "../database.h"
#include <QTableView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QColor>
#include <QBrush>
#include <QFont>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QDebug>

// ---------------------------------------------------------------------------
// Column definitions
// ---------------------------------------------------------------------------
enum Col {
    C_DATE, C_TIME, C_CALLSIGN, C_BAND, C_FREQ, C_MODE,
    C_RST_S, C_RST_R, C_NAME, C_QTH, C_COUNTRY, C_CONT,
    C_CQ, C_GRID, C_LOTW, C_EQSL, C_QSL,
    C_COUNT
};

// Custom delegate: forces Qt::BackgroundRole colors to show even under Fusion+QSS
class LogRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        bool selected = o.state & QStyle::State_Selected;
        QBrush rowBrush = idx.data(Qt::BackgroundRole).value<QBrush>();
        if (!selected && rowBrush.style() != Qt::NoBrush) {
            o.palette.setBrush(QPalette::All, QPalette::Base,          rowBrush);
            o.palette.setBrush(QPalette::All, QPalette::AlternateBase, rowBrush);
            o.features &= ~QStyleOptionViewItem::Alternate;
        }
        QStyledItemDelegate::paint(p, o, idx);
    }
};

QStringList LogModel::columnNames()
{
    return {"Data","Ora","Nominativo","Banda","Freq","Modo",
            "RST↑","RST↓","Nome","QTH","Paese","Cont",
            "CQ","Loc.","LoTW","eQSL","QSL"};
}

// ---------------------------------------------------------------------------
// LogModel
// ---------------------------------------------------------------------------
LogModel::LogModel(QObject* parent) : QAbstractTableModel(parent) {}

int LogModel::rowCount(const QModelIndex&) const    { return m_qsos.size(); }
int LogModel::columnCount(const QModelIndex&) const { return C_COUNT; }

QVariant LogModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_qsos.size()) return {};
    const Qso& q = m_qsos[idx.row()];

    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
        case C_DATE:     return q.date;
        case C_TIME:     return q.timeOn;
        case C_CALLSIGN: return q.callsign;
        case C_BAND:     return q.band;
        case C_FREQ:     return q.freq > 0 ? QString::number(q.freq,'f',3) : QString();
        case C_MODE:     return q.mode + (q.submode.isEmpty() ? "" : "/" + q.submode);
        case C_RST_S:    return q.rstSent;
        case C_RST_R:    return q.rstRcvd;
        case C_NAME:     return q.name;
        case C_QTH:      return q.qth;
        case C_COUNTRY:  return q.country;
        case C_CONT:     return q.continent;
        case C_CQ:       return q.cqZone ? QString::number(q.cqZone) : QString();
        case C_GRID:     return q.gridsquare;
        case C_LOTW: {
            QString s;
            if (q.lotwSent == "Y") s += "✉";
            if (q.lotwRcvd == "Y") s += "✓";
            return s;
        }
        case C_EQSL: {
            QString s;
            if (q.eqslSent == "Y") s += "✉";
            if (q.eqslRcvd == "Y") s += "✓";
            return s;
        }
        case C_QSL: {
            QString s;
            if (q.qslSent  == "Y") s += "✉";
            if (q.qslRcvd  == "Y") s += "✓";
            return s;
        }
        }
    }

    if (role == Qt::BackgroundRole) {
        bool lotwRcvd = (q.lotwRcvd == "Y");
        bool qslRcvd  = (q.qslRcvd  == "Y");
        bool eqslRcvd = (q.eqslRcvd == "Y");
        bool anySent  = (q.lotwSent == "Y" || q.eqslSent == "Y" || q.qslSent == "Y");
        if (lotwRcvd && qslRcvd)  return QBrush(QColor(0x80, 0xe8, 0xb0)); // LoTW+QSL: blue-green
        if (qslRcvd)              return QBrush(QColor(0xcd, 0xff, 0xd8)); // QSL card: green
        if (lotwRcvd)             return QBrush(QColor(0xcc, 0xee, 0xff)); // LoTW only: cyan
        if (eqslRcvd)             return QBrush(QColor(0xff, 0xff, 0xcc)); // eQSL only: yellow
        if (anySent)              return QBrush(QColor(0xff, 0xf5, 0xd0)); // sent: cream
        return {};
    }

    if (role == Qt::ForegroundRole) {
        switch (idx.column()) {
        case C_LOTW:
            if (q.lotwRcvd == "Y") return QBrush(QColor(0x27,0xae,0x60));
            if (q.lotwSent == "Y") return QBrush(QColor(0x29,0x80,0xb9));
            break;
        case C_EQSL:
            if (q.eqslRcvd == "Y") return QBrush(QColor(0xc8,0x90,0x00));
            if (q.eqslSent == "Y") return QBrush(QColor(0x29,0x80,0xb9));
            break;
        case C_QSL:
            if (q.qslRcvd  == "Y") return QBrush(QColor(0x27,0xae,0x60));
            if (q.qslSent  == "Y") return QBrush(QColor(0x29,0x80,0xb9));
            break;
        }
        return {};
    }

    if (role == Qt::FontRole && idx.column() == C_CALLSIGN) {
        QFont f;
        f.setBold(true);
        return f;
    }

    if (role == Qt::TextAlignmentRole) {
        switch (idx.column()) {
        case C_RST_S: case C_RST_R:
        case C_CQ: case C_FREQ:
        case C_LOTW: case C_EQSL: case C_QSL:
            return Qt::AlignCenter;
        }
    }

    if (role == Qt::UserRole)
        return q.id;

    return {};
}

QVariant LogModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient == Qt::Horizontal && role == Qt::DisplayRole)
        return columnNames().value(section);
    return {};
}

void LogModel::refresh()
{
    beginResetModel();
    m_qsos = Database::instance().getAllQsos(m_filter);
    endResetModel();
}

Qso LogModel::qsoAt(int row) const
{
    if (row < 0 || row >= m_qsos.size()) return {};
    return m_qsos[row];
}

// ---------------------------------------------------------------------------
// LogView
// ---------------------------------------------------------------------------
LogView::LogView(QWidget* parent) : QWidget(parent)
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0,0,0,0);
    vl->setSpacing(2);

    buildToolbar();

    m_model = new LogModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(C_CALLSIGN);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSortingEnabled(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSortIndicator(0, Qt::DescendingOrder);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setWordWrap(false);
    m_table->setItemDelegate(new LogRowDelegate(this));

    // Column widths
    m_table->setColumnWidth(C_DATE,     90);
    m_table->setColumnWidth(C_TIME,     55);
    m_table->setColumnWidth(C_CALLSIGN, 100);
    m_table->setColumnWidth(C_BAND,     50);
    m_table->setColumnWidth(C_FREQ,     80);
    m_table->setColumnWidth(C_MODE,     60);
    m_table->setColumnWidth(C_RST_S,    45);
    m_table->setColumnWidth(C_RST_R,    45);
    m_table->setColumnWidth(C_NAME,     120);
    m_table->setColumnWidth(C_QTH,      90);
    m_table->setColumnWidth(C_COUNTRY,  130);
    m_table->setColumnWidth(C_CONT,     40);
    m_table->setColumnWidth(C_CQ,       35);
    m_table->setColumnWidth(C_GRID,     65);
    m_table->setColumnWidth(C_LOTW,     52);
    m_table->setColumnWidth(C_EQSL,     52);
    m_table->setColumnWidth(C_QSL,      48);

    vl->addWidget(m_table);

    // Context menu
    m_ctxMenu = new QMenu(this);
    m_ctxMenu->addAction(tr("Copia nominativo"), this, &LogView::onCopyCall);
    m_ctxMenu->addSeparator();
    m_ctxMenu->addAction(tr("Elimina selezionati"), this, &LogView::onDeleteSelected);

    connect(m_table, &QTableView::doubleClicked,   this, &LogView::onDoubleClick);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LogView::onSelectionChanged);
    connect(m_table, &QTableView::customContextMenuRequested,
            this, &LogView::onContextMenu);

    // Initial load
    refresh();
}

void LogView::buildToolbar()
{
    auto* hl = new QHBoxLayout;
    hl->setSpacing(4);

    QLabel* lbl = new QLabel(tr("Cerca:"), this);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Nominativo / paese…"));
    m_search->setClearButtonEnabled(true);

    m_bandFilter = new QComboBox(this);
    m_bandFilter->addItem(tr("Tutte le bande"), QString());
    for (const QString& b : {"160m","80m","60m","40m","30m","20m","17m",
                              "15m","12m","10m","6m","4m","2m","70cm"})
        m_bandFilter->addItem(b, b);

    m_modeFilter = new QComboBox(this);
    m_modeFilter->addItem(tr("Tutti i modi"), QString());
    for (const QString& m : {"CW","SSB","USB","LSB","AM","FM","FT8","FT4",
                              "JS8","RTTY","PSK31","SSTV"})
        m_modeFilter->addItem(m, m);

    m_countLabel = new QLabel(tr("0 QSO"), this);

    hl->addWidget(lbl);
    hl->addWidget(m_search, 2);
    hl->addWidget(new QLabel(tr("Banda:")));
    hl->addWidget(m_bandFilter);
    hl->addWidget(new QLabel(tr("Modo:")));
    hl->addWidget(m_modeFilter);
    hl->addStretch();
    hl->addWidget(m_countLabel);

    layout()->addItem(hl);   // will be added to the vbox in constructor

    connect(m_search,     &QLineEdit::textChanged,     this, &LogView::onFilterChanged);
    connect(m_bandFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogView::onFilterChanged);
    connect(m_modeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogView::onFilterChanged);
}

void LogView::refresh()
{
    // Keep current filter, just reload data and update count
    m_model->refresh();
    m_proxy->setFilterFixedString(m_search->text().trimmed());
    m_countLabel->setText(tr("%1 QSOs").arg(m_proxy->rowCount()));
}

void LogView::openSearchBar()
{
    m_search->setFocus();
    m_search->selectAll();
}

Qso LogView::selectedQso() const
{
    auto idxList = m_table->selectionModel()->selectedRows();
    if (idxList.isEmpty()) return {};
    return qsoFromProxy(idxList.first());
}

Qso LogView::qsoFromProxy(const QModelIndex& proxyIdx) const
{
    QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
    return m_model->qsoAt(srcIdx.row());
}

void LogView::onFilterChanged()
{
    // Band/mode filter reloads from DB
    QsoFilter f;
    f.band = m_bandFilter->currentData().toString();
    f.mode = m_modeFilter->currentData().toString();
    m_model->setFilter(f);
    m_model->refresh();

    // Text filter on callsign/country via proxy
    m_proxy->setFilterFixedString(m_search->text().trimmed());
    m_countLabel->setText(tr("%1 QSOs").arg(m_proxy->rowCount()));
}

void LogView::onDoubleClick(const QModelIndex& idx)
{
    emit qsoDoubleClicked(qsoFromProxy(idx));
}

void LogView::onSelectionChanged()
{
    Qso q = selectedQso();
    if (q.isValid()) emit selectionChanged(q.callsign);
}

void LogView::onContextMenu(const QPoint& pos)
{
    if (m_table->selectionModel()->hasSelection())
        m_ctxMenu->exec(m_table->viewport()->mapToGlobal(pos));
}

void LogView::onCopyCall()
{
    Qso q = selectedQso();
    if (q.isValid())
        QApplication::clipboard()->setText(q.callsign);
}

void LogView::onDeleteSelected()
{
    auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    if (QMessageBox::question(this, tr("Elimina"),
            tr("Eliminare %1 QSO selezionati?").arg(rows.size()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
    for (const QModelIndex& idx : rows) {
        Qso q = qsoFromProxy(idx);
        if (q.isValid()) Database::instance().deleteQso(q.id);
    }
    refresh();
}
