#pragma once
#include <QString>
#include <QList>
#include "types.h"

namespace AdifHandler {
    QList<Qso> importAdif(const QString& filename);
    int exportAdif(const QString& filename, const QList<Qso>& qsos,
                   const QString& myCall = QString(),
                   const QString& program = "HamLog");
}
