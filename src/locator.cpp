#include "locator.h"
#include <cmath>

namespace Locator {

std::optional<QPair<double,double>> gridToLatLon(const QString& grid)
{
    QString g = grid.toUpper().trimmed();
    if (g.size() < 4) return std::nullopt;
    try {
        double lon = (g[0].unicode() - 'A') * 20.0 - 180.0 + g[2].digitValue() * 2.0;
        double lat = (g[1].unicode() - 'A') * 10.0 - 90.0  + g[3].digitValue() * 1.0;
        if (g.size() >= 6) {
            lon += (g[4].unicode() - 'A') * 5.0 / 60.0;
            lat += (g[5].unicode() - 'A') * 2.5 / 60.0;
            return QPair<double,double>(lat + 1.25/60.0, lon + 2.5/60.0);
        }
        return QPair<double,double>(lat + 0.5, lon + 1.0);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<QPair<int,int>> distanceBearing(const QString& grid1, const QString& grid2)
{
    auto p1 = gridToLatLon(grid1);
    auto p2 = gridToLatLon(grid2);
    if (!p1 || !p2) return std::nullopt;

    const double R = 6371.0;
    double lat1 = p1->first  * M_PI / 180.0;
    double lon1 = p1->second * M_PI / 180.0;
    double lat2 = p2->first  * M_PI / 180.0;
    double lon2 = p2->second * M_PI / 180.0;

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = std::sin(dlat/2)*std::sin(dlat/2) +
               std::cos(lat1)*std::cos(lat2)*std::sin(dlon/2)*std::sin(dlon/2);
    double km = 2 * R * std::asin(std::sqrt(a));

    double y = std::sin(dlon) * std::cos(lat2);
    double x = std::cos(lat1)*std::sin(lat2) - std::sin(lat1)*std::cos(lat2)*std::cos(dlon);
    double bearing = std::fmod(std::atan2(y, x) * 180.0 / M_PI + 360.0, 360.0);

    return QPair<int,int>(static_cast<int>(std::round(km)),
                          static_cast<int>(std::round(bearing)));
}

} // namespace Locator
