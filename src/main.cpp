#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include "database.h"
#include "dxcc.h"
#include "theme_manager.h"
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("HamLog");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("IW8XOU");
    app.setOrganizationDomain("hamlog.local");

    // First-run defaults (written only if the key doesn't exist yet)
    QSettings cfg;
    auto setDefault = [&](const QString& key, const QVariant& val) {
        if (!cfg.contains(key) || cfg.value(key).toString().isEmpty())
            cfg.setValue(key, val);
    };
    setDefault("station/callsign", "N0CALL");
    setDefault("station/operator", "N0CALL");
    setDefault("lotw/username",    "");
    setDefault("lotw/tqsl_path",   "C:/Program Files/TQSL/tqsl.exe");
    setDefault("eqsl/username",    "");
    setDefault("clublog/email",    "");
    setDefault("clublog/callsign", "");
    setDefault("hamqth/username",  "");
    setDefault("wsjtx/enabled",    true);

    // Apply saved theme
    QString theme = cfg.value("ui/theme", "Default").toString();
    ThemeManager::apply(&app, theme);

    // Init DXCC table
    Dxcc::init();

    // Open database — ensure path is never empty (empty path = in-memory DB = data lost on close)
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    setDefault("database/path", dataDir + "/hamlog.db");
    QString dbPath = cfg.value("database/path").toString();
    if (dbPath.trimmed().isEmpty())
        dbPath = dataDir + "/hamlog.db";

    Database& db = Database::instance();
    if (!db.open(dbPath)) {
        qCritical() << "Cannot open database:" << dbPath;
        return 1;
    }

    MainWindow w;
    w.show();

    int ret = app.exec();
    db.close();
    return ret;
}
