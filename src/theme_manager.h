#pragma once
#include <QString>
#include <QStringList>

class QApplication;

namespace ThemeManager {
    enum Theme { Default = 0, Dark, Night, Sepia };

    void        apply(QApplication* app, Theme theme);
    void        apply(QApplication* app, const QString& themeName);
    Theme       current();
    QString     currentName();
    QStringList availableThemes();
    Theme       fromName(const QString& name);
}
