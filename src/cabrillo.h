#pragma once
#include <QString>
#include <QHash>
#include <QList>
#include "types.h"

namespace Cabrillo {
    int exportCabrillo(const QString& filename,
                       const QList<Qso>& qsos,
                       const QHash<QString,QString>& header);
}
