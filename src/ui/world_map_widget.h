#pragma once
#include <QWidget>
#include <QHash>
#include <QPolygonF>

class QPainter;
class QTimer;

class WorldMapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WorldMapWidget(QWidget* parent = nullptr);
    void refresh();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    QPointF   latLonToXY(double lat, double lon) const;
    void      loadWorked();
    void      drawCoastlines(QPainter& p);
    void      drawTerminator(QPainter& p);
    void      drawEntities(QPainter& p);
    void      drawStation(QPainter& p);
    void      drawLegend(QPainter& p);
    QPolygonF terminatorPolygon() const;

    QHash<QString,bool> m_worked;
    QHash<QString,bool> m_confirmed;
    bool   m_dirty = true;
    QTimer* m_termTimer = nullptr;
};
