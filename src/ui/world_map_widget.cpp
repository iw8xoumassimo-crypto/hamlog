#include "world_map_widget.h"
#include "world_outline_data.h"
#include "../database.h"
#include "../dxcc.h"
#include "../locator.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QSettings>
#include <QTimer>
#include <QDateTime>
#include <QLinearGradient>
#include <QtMath>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
WorldMapWidget::WorldMapWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(300, 140);
    setMouseTracking(true);

    // Refresh terminator every 5 minutes
    m_termTimer = new QTimer(this);
    m_termTimer->setInterval(5 * 60 * 1000);
    connect(m_termTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_termTimer->start();
}

void WorldMapWidget::refresh()
{
    m_dirty = true;
    loadWorked();
    update();
}

void WorldMapWidget::loadWorked()
{
    m_worked    = Database::instance().workedEntities();
    m_confirmed = Database::instance().confirmedEntities();
    m_dirty = false;
}

QPointF WorldMapWidget::latLonToXY(double lat, double lon) const
{
    double x = (lon + 180.0) / 360.0 * width();
    double y = (90.0 - lat)  / 180.0 * height();
    return QPointF(x, y);
}

// ---------------------------------------------------------------------------
// Solar terminator polygon (night-side fill)
// ---------------------------------------------------------------------------
QPolygonF WorldMapWidget::terminatorPolygon() const
{
    QDateTime utc = QDateTime::currentDateTimeUtc();
    int doy  = utc.date().dayOfYear();
    double hour = utc.time().hour() + utc.time().minute() / 60.0
                                    + utc.time().second() / 3600.0;

    // Solar declination (simplified), degrees
    double decl = -23.45 * qCos(qDegreesToRadians(360.0 / 365.0 * (doy + 10)));

    // Subsolar longitude — sun is at zenith here at local noon
    double subLon = -(hour - 12.0) * 15.0;
    while (subLon >  180.0) subLon -= 360.0;
    while (subLon < -180.0) subLon += 360.0;

    double declRad = qDegreesToRadians(decl);
    double tanDecl = qTan(declRad);

    QPolygonF poly;

    // Build terminator curve lon = -180 … 180
    for (double lon = -180.0; lon <= 181.0; lon += 1.0) {
        double H   = qDegreesToRadians(lon - subLon);
        double lat;
        if (qAbs(tanDecl) < 1e-8) {
            // Equinox: terminator is a meridian — no meaningful curve
            lat = (qCos(H) >= 0.0) ? 90.0 : -90.0;
        } else {
            lat = qRadiansToDegrees(qAtan2(-qCos(H), tanDecl));
        }
        lat = qBound(-90.0, lat, 90.0);
        poly << latLonToXY(lat, qBound(-180.0, lon, 180.0));
    }

    // Close polygon via the night-side pole
    // decl > 0 → NH summer → south pole in night
    // decl < 0 → SH summer → north pole in night
    // decl = 0 → equinox  → half and half (handled above)
    if (decl >= 0.0) {
        poly << latLonToXY(-90.0,  181.0);
        poly << latLonToXY(-90.0, -181.0);
    } else {
        poly << latLonToXY( 90.0,  181.0);
        poly << latLonToXY( 90.0, -181.0);
    }

    return poly;
}

// ---------------------------------------------------------------------------
// Paint helpers
// ---------------------------------------------------------------------------
void WorldMapWidget::drawTerminator(QPainter& p)
{
    QPolygonF poly = terminatorPolygon();

    // Night-side dark overlay
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 10, 30, 110));
    p.drawPolygon(poly);

    // Terminator line (gray line) — warm orange glow
    if (poly.size() > 2) {
        QPolygonF curve;
        for (int i = 0; i < poly.size() - 2; ++i)
            curve << poly[i];

        // Outer glow
        p.setPen(QPen(QColor(0xff, 0xa0, 0x00, 60), 5.0));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(curve);

        // Core line
        p.setPen(QPen(QColor(0xff, 0xcc, 0x44, 200), 1.5));
        p.drawPolyline(curve);
    }
}

void WorldMapWidget::drawEntities(QPainter& p)
{
    auto entities = Dxcc::allEntities();
    for (const QString& entity : entities) {
        DxccInfo info = Dxcc::lookup(entity);
        // Skip entities with no coordinates
        if (qAbs(info.lat) < 0.001 && qAbs(info.lon) < 0.001) continue;

        bool worked    = m_worked.contains(entity);
        bool confirmed = m_confirmed.contains(entity);
        QPointF pt = latLonToXY(info.lat, info.lon);

        if (!worked) {
            // Unworked: tiny semi-transparent gray dot
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xc0, 0xc0, 0xd0, 70));
            p.drawEllipse(pt, 2.0, 2.0);
        } else if (confirmed) {
            // Confirmed: bright green with glow halo
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x00, 0xff, 0x70, 50));
            p.drawEllipse(pt, 9.0, 9.0);

            p.setPen(QPen(Qt::white, 0.8));
            p.setBrush(QColor(0x00, 0xe0, 0x50));
            p.drawEllipse(pt, 5.0, 5.0);
        } else {
            // Worked not confirmed: yellow/amber with halo
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xff, 0xcc, 0x00, 50));
            p.drawEllipse(pt, 8.0, 8.0);

            p.setPen(QPen(Qt::white, 0.8));
            p.setBrush(QColor(0xff, 0xcc, 0x00));
            p.drawEllipse(pt, 4.0, 4.0);
        }
    }
}

void WorldMapWidget::drawStation(QPainter& p)
{
    QSettings cfg;
    QString myGrid = cfg.value("station/locator").toString();
    if (myGrid.size() < 4) return;

    auto pos = Locator::gridToLatLon(myGrid);
    if (!pos) return;

    QPointF pt = latLonToXY(pos->first, pos->second);

    // Outer glow ring
    p.setPen(QPen(QColor(0xff, 0x50, 0x00, 80), 4.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(pt, 10.0, 10.0);

    // Fill circle
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(0xff, 0x50, 0x00));
    p.drawEllipse(pt, 6.0, 6.0);

    // Cross arms
    p.setPen(QPen(Qt::white, 1.5));
    p.drawLine(pt + QPointF(-11, 0), pt + QPointF(11, 0));
    p.drawLine(pt + QPointF(0, -11), pt + QPointF(0, 11));
}

void WorldMapWidget::drawLegend(QPainter& p)
{
    const int lx = 8;
    const int ly = height() - 68;
    const int lw = 272;
    const int lh = 42;

    // Background panel
    p.setPen(QPen(QColor(255, 255, 255, 40), 0.5));
    p.setBrush(QColor(0, 5, 20, 140));
    p.drawRoundedRect(lx, ly, lw, lh, 5, 5);

    QFont small = font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.5));
    p.setFont(small);

    auto drawItem = [&](int x, QColor fill, QColor glow, const QString& label) {
        // Glow
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(x + 2, ly + 10, 14, 14);
        // Dot
        p.setPen(QPen(Qt::white, 0.8));
        p.setBrush(fill);
        p.drawEllipse(x + 4, ly + 12, 10, 10);
        // Label
        p.setPen(Qt::white);
        p.drawText(x + 18, ly + 22, label);
    };

    drawItem(lx + 4,  QColor(0x00,0xe0,0x50), QColor(0x00,0xff,0x70,60), tr("Confermato"));
    drawItem(lx + 94, QColor(0xff,0xcc,0x00), QColor(0xff,0xcc,0x00,60), tr("Lavorato"));
    drawItem(lx + 172,QColor(0xc0,0xc0,0xd0,120), QColor(0,0,0,0),       tr("Non lavorato"));

    // Gray-line indicator
    p.setPen(QColor(0xff, 0xcc, 0x44, 200));
    p.drawText(lx + 4, ly + lh - 5,
               tr("Linea grigia: ora %1 UTC").arg(
                   QDateTime::currentDateTimeUtc().toString("HH:mm")));
}

// ---------------------------------------------------------------------------
// Continent outlines
// ---------------------------------------------------------------------------
void WorldMapWidget::drawCoastlines(QPainter& p)
{
    p.setPen(QPen(QColor(0x55, 0x75, 0x55, 200), 0.6));
    p.setBrush(QColor(0x33, 0x55, 0x2a, 230));

    for (int ci = 0; kContinents[ci]; ++ci) {
        const float* data = kContinents[ci];
        QPolygonF poly;
        for (int i = 0; data[i] < 92.0f; i += 2)
            poly << latLonToXY(data[i], data[i+1]);
        if (poly.size() >= 3)
            p.drawPolygon(poly);
    }
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------
void WorldMapWidget::paintEvent(QPaintEvent*)
{
    if (m_dirty) loadWorked();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 1. Ocean gradient background
    QLinearGradient ocean(0, 0, 0, height());
    ocean.setColorAt(0.00, QColor(0x10, 0x28, 0x4a));
    ocean.setColorAt(0.35, QColor(0x1a, 0x4e, 0x80));
    ocean.setColorAt(0.65, QColor(0x1a, 0x4e, 0x80));
    ocean.setColorAt(1.00, QColor(0x10, 0x28, 0x4a));
    p.fillRect(rect(), ocean);

    // 2. Graticule — subtle grid
    p.setPen(QPen(QColor(255, 255, 255, 18), 0.4));
    for (int lon = -180; lon <= 180; lon += 30) {
        p.drawLine(latLonToXY(90, lon), latLonToXY(-90, lon));
    }
    for (int lat = -90; lat <= 90; lat += 30) {
        p.drawLine(latLonToXY(lat, -180), latLonToXY(lat, 180));
    }
    // Equator slightly brighter
    p.setPen(QPen(QColor(255, 255, 255, 40), 0.6));
    p.drawLine(latLonToXY(0, -180), latLonToXY(0, 180));

    // 3. Continent fills
    drawCoastlines(p);

    // 4. Day/night overlay + gray line
    drawTerminator(p);

    // 5. Entity dots
    drawEntities(p);

    // 6. My station
    drawStation(p);

    // 7. Legend
    drawLegend(p);
}

void WorldMapWidget::resizeEvent(QResizeEvent*) { update(); }

// ---------------------------------------------------------------------------
// Mouse click — show entity tooltip
// ---------------------------------------------------------------------------
void WorldMapWidget::mousePressEvent(QMouseEvent* ev)
{
    auto entities = Dxcc::allEntities();
    double bestDist = 1e9;
    QString bestEntity;

    for (const QString& entity : entities) {
        DxccInfo info = Dxcc::lookup(entity);
        if (qAbs(info.lat) < 0.001 && qAbs(info.lon) < 0.001) continue;
        QPointF pt = latLonToXY(info.lat, info.lon);
        double dx = pt.x() - ev->pos().x();
        double dy = pt.y() - ev->pos().y();
        double dist = dx*dx + dy*dy;
        if (dist < bestDist) { bestDist = dist; bestEntity = entity; }
    }

    if (bestDist < 300) {
        bool w = m_worked.contains(bestEntity);
        bool c = m_confirmed.contains(bestEntity);
        DxccInfo info = Dxcc::lookup(bestEntity);
        QString tip = QString("<b>%1</b><br>CQ %2 · ITU %3 · %4<br>%5")
            .arg(bestEntity)
            .arg(info.cqZone).arg(info.ituZone).arg(info.continent)
            .arg(c ? tr("✓ Confermato") : w ? tr("○ Lavorato") : tr("— Non lavorato"));
        QToolTip::showText(ev->globalPosition().toPoint(), tip);
    }
}
