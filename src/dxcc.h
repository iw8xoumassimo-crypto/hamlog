#pragma once
#include <QString>
#include <QStringList>
#include "types.h"

namespace Dxcc {
    void init();
    DxccInfo lookup(const QString& callsign);
    QStringList allEntities();
    int entityCount();
}
