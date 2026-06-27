#pragma once
#include <QString>
#include <optional>
#include <QPair>

namespace Locator {
    std::optional<QPair<double,double>> gridToLatLon(const QString& grid);
    std::optional<QPair<int,int>> distanceBearing(const QString& grid1, const QString& grid2);
}
