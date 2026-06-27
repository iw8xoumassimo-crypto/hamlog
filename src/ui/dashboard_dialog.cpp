#include "dashboard_dialog.h"
#include "../database.h"
#include <QLabel>
#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Band-activity "donut segment" strip — shows each band as a proportional arc
// ---------------------------------------------------------------------------
class BandActivityWidget : public QWidget
{
public:
    explicit BandActivityWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(110);
    }

    void setData(const QHash<QString,int>& data)
    {
        m_items.clear();
        int total = 0;
        for (auto v : data) total += v;
        if (total == 0) return;

        // Standard band colors
        static const QHash<QString,QColor> bandColors = {
            {"160m", QColor(0xa0,0x40,0xff)},
            {"80m",  QColor(0x60,0x60,0xff)},
            {"60m",  QColor(0x40,0x80,0xff)},
            {"40m",  QColor(0x20,0xb0,0xff)},
            {"30m",  QColor(0x00,0xc8,0xc8)},
            {"20m",  QColor(0x00,0xc0,0x40)},
            {"17m",  QColor(0x80,0xd0,0x00)},
            {"15m",  QColor(0xd0,0xc0,0x00)},
            {"12m",  QColor(0xff,0xa0,0x00)},
            {"10m",  QColor(0xff,0x60,0x00)},
            {"6m",   QColor(0xff,0x20,0x20)},
            {"2m",   QColor(0xff,0x40,0x80)},
        };

        QList<QPair<int,QString>> sorted;
        for (auto it = data.cbegin(); it != data.cend(); ++it)
            sorted.append({it.value(), it.key()});
        std::sort(sorted.begin(), sorted.end(), std::greater<>());

        m_total = total;
        for (const auto& [cnt, band] : sorted) {
            QColor c = bandColors.value(band, QColor(0x80,0x80,0x80));
            m_items.append({band, cnt, c});
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_items.isEmpty()) return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), palette().window());

        const int cx  = width() / 2;
        const int cy  = height() / 2 + 4;
        const int r   = qMin(cx, cy) - 16;
        const int gap = 3; // degrees between segments

        int startAngle = 90 * 16;  // Qt: 0=3 o'clock, + = CCW; start at 12 o'clock

        for (const Item& item : m_items) {
            double fraction = double(item.count) / m_total;
            int span = int(fraction * 360 * 16) - gap * 16;
            if (span <= 0) { startAngle -= gap * 16; continue; }

            // Outer arc (thick)
            p.setPen(Qt::NoPen);
            p.setBrush(item.color);
            QRectF outerRect(cx - r, cy - r, 2*r, 2*r);
            p.drawPie(outerRect, startAngle, span);

            // Inner cutout for donut effect
            p.setBrush(palette().window());
            int ir = int(r * 0.60);
            p.drawEllipse(cx - ir, cy - ir, 2*ir, 2*ir);

            startAngle += span + gap * 16;
        }

        // Center label
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.3);
        f.setBold(true);
        p.setFont(f);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(cx - 30, cy - 10, 60, 20,
                   Qt::AlignCenter, QString::number(m_total));

        f.setPointSizeF(font().pointSizeF() * 0.85);
        f.setBold(false);
        p.setFont(f);
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(cx - 30, cy + 6, 60, 14,
                   Qt::AlignCenter, QObject::tr("QSO"));

        // Band labels around the donut
        double angle = 90.0;
        for (const Item& item : m_items) {
            double fraction = double(item.count) / m_total;
            double midAngle = angle - (fraction * 360.0) / 2.0;
            double rad = qDegreesToRadians(midAngle);
            double lx = cx + (r + 10) * qCos(rad);
            double ly = cy - (r + 10) * qSin(rad);

            p.setPen(item.color);
            QFont lf = font();
            lf.setPointSizeF(qMax(6.5, lf.pointSizeF() - 1.0));
            p.setFont(lf);
            p.drawText(int(lx) - 18, int(ly) - 7, 36, 14,
                       Qt::AlignCenter, item.label);

            angle -= fraction * 360.0;
        }
    }

private:
    struct Item { QString label; int count; QColor color; };
    QList<Item> m_items;
    int         m_total = 1;
};

// ---------------------------------------------------------------------------
// DashboardDialog
// ---------------------------------------------------------------------------
DashboardDialog::DashboardDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Dashboard"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setMinimumSize(580, 400);
    build();
    populate();
}

void DashboardDialog::build()
{
    auto* vl = new QVBoxLayout(this);
    vl->addStretch();
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

void DashboardDialog::populate()
{
    auto* vl = qobject_cast<QVBoxLayout*>(layout());
    Stats s = Database::instance().getStats();

    // --- Stat cards row ---
    auto addCard = [&](const QString& title, const QString& value, const QString& color) {
        auto* grp = new QGroupBox(title, this);
        auto* gl  = new QGridLayout(grp);
        auto* lbl = new QLabel(value, grp);
        lbl->setStyleSheet(QString("font-size:26px; font-weight:bold; color:%1;").arg(color));
        lbl->setAlignment(Qt::AlignCenter);
        gl->addWidget(lbl, 0, 0);
        return grp;
    };

    auto* cardsW = new QWidget(this);
    auto* gl     = new QGridLayout(cardsW);
    gl->setSpacing(6);
    gl->addWidget(addCard(tr("QSO Totali"),       QString::number(s.totalQsos),    "#2060c0"), 0, 0);
    gl->addWidget(addCard(tr("Paesi"),            QString::number(s.countries),    "#208020"), 0, 1);
    gl->addWidget(addCard(tr("Zone CQ"),          QString::number(s.cqZones),      "#c06020"), 0, 2);
    gl->addWidget(addCard(tr("LoTW Confermati"),  QString::number(s.lotwConfirmed),"#20a0a0"), 1, 0);
    gl->addWidget(addCard(tr("eQSL Confermati"),  QString::number(s.eqslConfirmed),"#a020a0"), 1, 1);
    gl->addWidget(addCard(tr("Nominativi Unici"), QString::number(s.uniqueCalls),  "#a0a000"), 1, 2);

    vl->insertWidget(0, cardsW);

    // --- Today label ---
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QsoFilter f;
    f.dateFrom = today;
    f.dateTo   = today;
    int todayCount = Database::instance().getAllQsos(f).size();

    auto* todayLbl = new QLabel(tr("Oggi: %1 QSO").arg(todayCount), this);
    todayLbl->setStyleSheet("font-size:13px; color:#606060; margin-left:4px;");
    vl->insertWidget(1, todayLbl);

    // --- Band activity donut ---
    if (!s.byBand.isEmpty()) {
        auto* bandGrp  = new QGroupBox(tr("Attività per Banda"), this);
        auto* bandVl   = new QVBoxLayout(bandGrp);
        auto* bandChart= new BandActivityWidget(bandGrp);
        bandChart->setData(s.byBand);
        bandVl->addWidget(bandChart);
        vl->insertWidget(2, bandGrp);
    }
}
