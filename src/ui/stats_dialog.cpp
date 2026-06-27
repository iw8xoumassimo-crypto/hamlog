#include "stats_dialog.h"
#include "../database.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPainter>
#include <QPaintEvent>
#include <QSplitter>
#include <QFont>
#include <QtMath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Inline bar-chart widget (QPainter-based, no external deps)
// ---------------------------------------------------------------------------
class BarChartWidget : public QWidget
{
public:
    explicit BarChartWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(180);
    }

    void setData(const QHash<QString,int>& data,
                 const QColor& barColor = QColor(0x26,0x8b,0xd2))
    {
        m_items.clear();
        m_color = barColor;
        for (auto it = data.cbegin(); it != data.cend(); ++it)
            m_items.append({it.key(), it.value()});
        std::sort(m_items.begin(), m_items.end(),
                  [](const Item& a, const Item& b){ return a.count > b.count; });
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_items.isEmpty()) return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int leftMargin  = 64;
        const int rightMargin = 50;
        const int topMargin   = 12;
        const int bottomMargin= 8;
        const int barH        = 20;
        const int barGap      = 6;
        const int n           = m_items.size();

        int maxCount = m_items.isEmpty() ? 1 : m_items[0].count;

        // Background
        p.fillRect(rect(), palette().color(QPalette::Window));

        int y = topMargin;
        for (const Item& item : m_items) {
            double fraction = (maxCount > 0) ? double(item.count) / maxCount : 0.0;
            int barW = int(fraction * (width() - leftMargin - rightMargin));

            // Bar background track
            QRectF track(leftMargin, y, width() - leftMargin - rightMargin, barH);
            p.setPen(Qt::NoPen);
            p.setBrush(palette().color(QPalette::AlternateBase));
            p.drawRoundedRect(track, 3, 3);

            // Filled bar with gradient
            if (barW > 0) {
                QLinearGradient grad(leftMargin, y, leftMargin + barW, y);
                grad.setColorAt(0.0, m_color.lighter(130));
                grad.setColorAt(1.0, m_color);
                p.setBrush(grad);
                p.drawRoundedRect(QRectF(leftMargin, y, barW, barH), 3, 3);
            }

            // Label (left)
            p.setPen(palette().color(QPalette::Text));
            QFont f = font();
            f.setBold(true);
            f.setPointSizeF(qMax(7.5, f.pointSizeF() - 0.5));
            p.setFont(f);
            p.drawText(QRectF(0, y, leftMargin - 6, barH),
                       Qt::AlignRight | Qt::AlignVCenter, item.label);

            // Count (right)
            f.setBold(false);
            p.setFont(f);
            p.setPen(palette().color(QPalette::PlaceholderText));
            p.drawText(QRectF(leftMargin + barW + 6, y, rightMargin - 4, barH),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(item.count));

            y += barH + barGap;
        }

        Q_UNUSED(bottomMargin)
        Q_UNUSED(n)
    }

    QSize sizeHint() const override
    {
        int h = 12 + 8 + m_items.size() * 26;
        return QSize(300, qMax(h, 120));
    }

private:
    struct Item { QString label; int count; };
    QList<Item> m_items;
    QColor      m_color = QColor(0x26, 0x8b, 0xd2);
};

// ---------------------------------------------------------------------------
// StatsDialog
// ---------------------------------------------------------------------------
StatsDialog::StatsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Statistiche"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(650, 520);
    build();
    populate();
}

void StatsDialog::build()
{
    auto* vl = new QVBoxLayout(this);
    vl->addStretch();
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

void StatsDialog::populate()
{
    Stats s = Database::instance().getStats();
    auto* vl = qobject_cast<QVBoxLayout*>(layout());

    // Summary group
    auto* grp = new QGroupBox(tr("Riepilogo"), this);
    auto* gl  = new QGridLayout(grp);

    auto addStat = [&](int row, int col, const QString& lbl, const QString& val){
        gl->addWidget(new QLabel(lbl, grp), row, col*2);
        auto* v = new QLabel(val, grp);
        v->setStyleSheet("font-weight:bold");
        gl->addWidget(v, row, col*2+1);
    };

    addStat(0,0, tr("QSO totali:"),        QString::number(s.totalQsos));
    addStat(0,1, tr("Nominativi unici:"),  QString::number(s.uniqueCalls));
    addStat(1,0, tr("Paesi:"),             QString::number(s.countries));
    addStat(1,1, tr("Zone CQ:"),           QString::number(s.cqZones));
    addStat(2,0, tr("Zone ITU:"),          QString::number(s.ituZones));
    addStat(2,1, tr("LoTW confermati:"),   QString::number(s.lotwConfirmed));
    addStat(3,0, tr("eQSL confermati:"),   QString::number(s.eqslConfirmed));

    vl->insertWidget(0, grp);

    // Splitter: charts on left, tables on right
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // --- Charts panel ---
    auto* chartsW  = new QWidget(splitter);
    auto* chartsVl = new QVBoxLayout(chartsW);
    chartsVl->setContentsMargins(0,0,0,0);

    auto* bandGrp  = new QGroupBox(tr("Per Banda"), chartsW);
    auto* bandVl   = new QVBoxLayout(bandGrp);
    auto* bandChart= new BarChartWidget(bandGrp);
    bandChart->setData(s.byBand, QColor(0x26, 0x8b, 0xd2));
    bandVl->addWidget(bandChart);
    chartsVl->addWidget(bandGrp);

    auto* modeGrp  = new QGroupBox(tr("Per Modo"), chartsW);
    auto* modeVl   = new QVBoxLayout(modeGrp);
    auto* modeChart= new BarChartWidget(modeGrp);
    modeChart->setData(s.byMode, QColor(0x2a, 0xa1, 0x98));
    modeVl->addWidget(modeChart);
    chartsVl->addWidget(modeGrp);

    chartsVl->addStretch();
    splitter->addWidget(chartsW);

    // --- Tables panel ---
    auto* tabs = new QTabWidget(splitter);

    auto makePairTable = [](const QHash<QString,int>& data, QWidget* parent) -> QTableWidget* {
        auto* t = new QTableWidget(data.size(), 2, parent);
        t->setHorizontalHeaderLabels({QObject::tr("Nome"), QObject::tr("Conteggio")});
        t->horizontalHeader()->setStretchLastSection(true);
        t->verticalHeader()->hide();
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        int row = 0;
        QList<QPair<int,QString>> sorted;
        for (auto it = data.cbegin(); it != data.cend(); ++it)
            sorted.append({it.value(), it.key()});
        std::sort(sorted.begin(), sorted.end(), std::greater<>());
        for (const auto& [cnt, name] : sorted) {
            t->setItem(row,0, new QTableWidgetItem(name));
            t->setItem(row,1, new QTableWidgetItem(QString::number(cnt)));
            ++row;
        }
        return t;
    };

    tabs->addTab(makePairTable(s.byBand,      tabs), tr("Per Banda"));
    tabs->addTab(makePairTable(s.byMode,      tabs), tr("Per Modo"));
    tabs->addTab(makePairTable(s.byContinent, tabs), tr("Per Continente"));

    splitter->addWidget(tabs);
    splitter->setSizes({340, 260});

    vl->insertWidget(1, splitter);
}
