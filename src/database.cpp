#include "database.h"
#include "dxcc.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QSignalBlocker>

static const char* CONNECTION_NAME = "hamlog_main";

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Database& Database::instance()
{
    static Database inst;
    return inst;
}

Database::Database(QObject* parent) : QObject(parent) {}

Database::~Database() { close(); }

// ---------------------------------------------------------------------------
// Open / Close
// ---------------------------------------------------------------------------
bool Database::open(const QString& path)
{
    if (QSqlDatabase::contains(CONNECTION_NAME))
        QSqlDatabase::removeDatabase(CONNECTION_NAME);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", CONNECTION_NAME);
    db.setDatabaseName(path);

    if (!db.open()) {
        qWarning() << "Database::open:" << db.lastError().text();
        return false;
    }

    // SQLite pragmas
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA synchronous=NORMAL");
    q.exec("PRAGMA foreign_keys=ON");
    q.exec("PRAGMA cache_size=-32000");

    m_path = path;
    createSchema();
    invalidateCache();

    // Seleziona il log corrente (primo disponibile) se non ancora impostato
    if (m_currentLog <= 0) {
        QSqlQuery lq(QSqlDatabase::database(CONNECTION_NAME));
        lq.exec("SELECT id FROM logs ORDER BY id LIMIT 1");
        if (lq.next()) m_currentLog = lq.value(0).toInt();
    }

    emit databaseOpened(path);
    return true;
}

void Database::close()
{
    if (QSqlDatabase::contains(CONNECTION_NAME)) {
        QSqlDatabase::database(CONNECTION_NAME).close();
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }
    m_path.clear();
    invalidateCache();
    emit databaseClosed();
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(CONNECTION_NAME) &&
           QSqlDatabase::database(CONNECTION_NAME, false).isOpen();
}

QString Database::path() const { return m_path; }

QSqlDatabase Database::db() const
{
    return QSqlDatabase::database(CONNECTION_NAME);
}

void Database::invalidateCache()
{
    m_cacheValid  = false;
    m_cache.clear();
    m_countCache  = -1;
}

// ---------------------------------------------------------------------------
// Schema creation
// ---------------------------------------------------------------------------
void Database::createSchema()
{
    QSqlDatabase d = db();
    QSqlQuery q(d);

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS logs (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            name      TEXT NOT NULL DEFAULT 'Default',
            callsign  TEXT,
            qth       TEXT,
            locator   TEXT,
            created   TEXT DEFAULT (datetime('now'))
        )
    )");

    // Insert default log if empty
    q.exec("INSERT OR IGNORE INTO logs (id,name) VALUES (1,'Default')");

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS qso (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            log_id        INTEGER REFERENCES logs(id) DEFAULT 1,
            callsign      TEXT NOT NULL,
            date          TEXT NOT NULL,
            time_on       TEXT,
            time_off      TEXT,
            band          TEXT,
            freq          REAL,
            mode          TEXT,
            submode       TEXT,
            rst_sent      TEXT DEFAULT '59',
            rst_rcvd      TEXT DEFAULT '59',
            name          TEXT,
            qth           TEXT,
            country       TEXT,
            dxcc_id       INTEGER,
            cq_zone       INTEGER,
            itu_zone      INTEGER,
            continent     TEXT,
            lat           REAL,
            lon           REAL,
            state         TEXT,
            gridsquare    TEXT,
            distance      INTEGER,
            bearing       INTEGER,
            power         REAL,
            comment       TEXT,
            notes         TEXT,
            iota          TEXT,
            sota_ref      TEXT,
            pota_ref      TEXT,
            contest_id    TEXT,
            srx           TEXT,
            stx           TEXT,
            manager       TEXT,
            operator_call TEXT,
            station_call  TEXT,
            qsl_sent      TEXT DEFAULT 'N',
            qsl_rcvd      TEXT DEFAULT 'N',
            lotw_sent     TEXT DEFAULT 'N',
            lotw_rcvd     TEXT DEFAULT 'N',
            eqsl_sent     TEXT DEFAULT 'N',
            eqsl_rcvd     TEXT DEFAULT 'N',
            clublog_sent  INTEGER DEFAULT 0,
            created_at    TEXT DEFAULT (datetime('now'))
        )
    )");

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS callbook_cache (
            callsign  TEXT PRIMARY KEY,
            data      TEXT,
            fetched   TEXT DEFAULT (datetime('now'))
        )
    )");

    q.exec("CREATE INDEX IF NOT EXISTS idx_qso_call ON qso(callsign)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_qso_date ON qso(date)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_qso_band ON qso(band)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_qso_mode ON qso(mode)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_qso_log  ON qso(log_id)");
}

// ---------------------------------------------------------------------------
// Helper: map QSqlQuery row -> Qso
// ---------------------------------------------------------------------------
static Qso rowToQso(const QSqlQuery& q)
{
    Qso r;
    r.id           = q.value("id").toInt();
    r.logId        = q.value("log_id").toInt();
    r.callsign     = q.value("callsign").toString();
    r.date         = q.value("date").toString();
    r.timeOn       = q.value("time_on").toString();
    r.timeOff      = q.value("time_off").toString();
    r.band         = q.value("band").toString();
    r.freq         = q.value("freq").toDouble();
    r.mode         = q.value("mode").toString();
    r.submode      = q.value("submode").toString();
    r.rstSent      = q.value("rst_sent").toString();
    r.rstRcvd      = q.value("rst_rcvd").toString();
    r.name         = q.value("name").toString();
    r.qth          = q.value("qth").toString();
    r.country      = q.value("country").toString();
    r.dxccId       = q.value("dxcc_id").toInt();
    r.cqZone       = q.value("cq_zone").toInt();
    r.ituZone      = q.value("itu_zone").toInt();
    r.continent    = q.value("continent").toString();
    r.lat          = q.value("lat").toDouble();
    r.lon          = q.value("lon").toDouble();
    r.state        = q.value("state").toString();
    r.gridsquare   = q.value("gridsquare").toString();
    r.distance     = q.value("distance").toInt();
    r.bearing      = q.value("bearing").toInt();
    r.power        = q.value("power").toDouble();
    r.comment      = q.value("comment").toString();
    r.notes        = q.value("notes").toString();
    r.iota         = q.value("iota").toString();
    r.sotaRef      = q.value("sota_ref").toString();
    r.potaRef      = q.value("pota_ref").toString();
    r.contestId    = q.value("contest_id").toString();
    r.srx          = q.value("srx").toString();
    r.stx          = q.value("stx").toString();
    r.manager      = q.value("manager").toString();
    r.operatorCall = q.value("operator_call").toString();
    r.stationCall  = q.value("station_call").toString();
    r.qslSent      = q.value("qsl_sent").toString();
    r.qslRcvd      = q.value("qsl_rcvd").toString();
    r.lotwSent     = q.value("lotw_sent").toString();
    r.lotwRcvd     = q.value("lotw_rcvd").toString();
    r.eqslSent     = q.value("eqsl_sent").toString();
    r.eqslRcvd     = q.value("eqsl_rcvd").toString();
    r.clublogSent  = q.value("clublog_sent").toBool();
    return r;
}

// ---------------------------------------------------------------------------
// QSO CRUD
// ---------------------------------------------------------------------------
bool Database::saveQso(Qso& qso)
{
    QSqlQuery q(db());
    q.prepare(R"(
        INSERT INTO qso
        (log_id,callsign,date,time_on,time_off,band,freq,mode,submode,
         rst_sent,rst_rcvd,name,qth,country,dxcc_id,cq_zone,itu_zone,
         continent,lat,lon,state,gridsquare,distance,bearing,power,
         comment,notes,iota,sota_ref,pota_ref,contest_id,srx,stx,
         manager,operator_call,station_call,
         qsl_sent,qsl_rcvd,lotw_sent,lotw_rcvd,eqsl_sent,eqsl_rcvd,clublog_sent)
        VALUES
        (:log_id,:callsign,:date,:time_on,:time_off,:band,:freq,:mode,:submode,
         :rst_sent,:rst_rcvd,:name,:qth,:country,:dxcc_id,:cq_zone,:itu_zone,
         :continent,:lat,:lon,:state,:gridsquare,:distance,:bearing,:power,
         :comment,:notes,:iota,:sota_ref,:pota_ref,:contest_id,:srx,:stx,
         :manager,:operator_call,:station_call,
         :qsl_sent,:qsl_rcvd,:lotw_sent,:lotw_rcvd,:eqsl_sent,:eqsl_rcvd,:clublog_sent)
    )");

    q.bindValue(":log_id",        qso.logId > 0 ? qso.logId : m_currentLog);
    q.bindValue(":callsign",      qso.callsign.toUpper());
    q.bindValue(":date",          qso.date);
    q.bindValue(":time_on",       qso.timeOn);
    q.bindValue(":time_off",      qso.timeOff.isEmpty() ? QVariant() : qso.timeOff);
    q.bindValue(":band",          qso.band);
    q.bindValue(":freq",          qso.freq > 0 ? qso.freq : QVariant());
    q.bindValue(":mode",          qso.mode);
    q.bindValue(":submode",       qso.submode.isEmpty() ? QVariant() : qso.submode);
    q.bindValue(":rst_sent",      qso.rstSent.isEmpty() ? "59" : qso.rstSent);
    q.bindValue(":rst_rcvd",      qso.rstRcvd.isEmpty() ? "59" : qso.rstRcvd);
    q.bindValue(":name",          qso.name);
    q.bindValue(":qth",           qso.qth);
    q.bindValue(":country",       qso.country);
    q.bindValue(":dxcc_id",       qso.dxccId ? qso.dxccId : QVariant());
    q.bindValue(":cq_zone",       qso.cqZone  ? qso.cqZone  : QVariant());
    q.bindValue(":itu_zone",      qso.ituZone ? qso.ituZone : QVariant());
    q.bindValue(":continent",     qso.continent);
    q.bindValue(":lat",           qso.lat  != 0.0 ? qso.lat  : QVariant());
    q.bindValue(":lon",           qso.lon  != 0.0 ? qso.lon  : QVariant());
    q.bindValue(":state",         qso.state);
    q.bindValue(":gridsquare",    qso.gridsquare);
    q.bindValue(":distance",      qso.distance ? qso.distance : QVariant());
    q.bindValue(":bearing",       qso.bearing  ? qso.bearing  : QVariant());
    q.bindValue(":power",         qso.power > 0 ? qso.power : QVariant());
    q.bindValue(":comment",       qso.comment);
    q.bindValue(":notes",         qso.notes);
    q.bindValue(":iota",          qso.iota);
    q.bindValue(":sota_ref",      qso.sotaRef);
    q.bindValue(":pota_ref",      qso.potaRef);
    q.bindValue(":contest_id",    qso.contestId);
    q.bindValue(":srx",           qso.srx);
    q.bindValue(":stx",           qso.stx);
    q.bindValue(":manager",       qso.manager);
    q.bindValue(":operator_call", qso.operatorCall);
    q.bindValue(":station_call",  qso.stationCall);
    q.bindValue(":qsl_sent",      qso.qslSent.isEmpty()  ? "N" : qso.qslSent);
    q.bindValue(":qsl_rcvd",      qso.qslRcvd.isEmpty()  ? "N" : qso.qslRcvd);
    q.bindValue(":lotw_sent",     qso.lotwSent.isEmpty() ? "N" : qso.lotwSent);
    q.bindValue(":lotw_rcvd",     qso.lotwRcvd.isEmpty() ? "N" : qso.lotwRcvd);
    q.bindValue(":eqsl_sent",     qso.eqslSent.isEmpty() ? "N" : qso.eqslSent);
    q.bindValue(":eqsl_rcvd",     qso.eqslRcvd.isEmpty() ? "N" : qso.eqslRcvd);
    q.bindValue(":clublog_sent",  qso.clublogSent ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "saveQso:" << q.lastError().text();
        return false;
    }
    qso.id = q.lastInsertId().toInt();
    invalidateCache();
    emit qsoSaved(qso.id);
    return true;
}


bool Database::updateQso(const Qso& qso)
{
    QSqlQuery q(db());
    q.prepare(R"(
        UPDATE qso SET
            callsign=:callsign, date=:date, time_on=:time_on, time_off=:time_off,
            band=:band, freq=:freq, mode=:mode, submode=:submode,
            rst_sent=:rst_sent, rst_rcvd=:rst_rcvd, name=:name, qth=:qth,
            country=:country, dxcc_id=:dxcc_id, cq_zone=:cq_zone, itu_zone=:itu_zone,
            continent=:continent, lat=:lat, lon=:lon, state=:state,
            gridsquare=:gridsquare, distance=:distance, bearing=:bearing, power=:power,
            comment=:comment, notes=:notes, iota=:iota, sota_ref=:sota_ref,
            pota_ref=:pota_ref, contest_id=:contest_id, srx=:srx, stx=:stx,
            manager=:manager, operator_call=:operator_call, station_call=:station_call,
            qsl_sent=:qsl_sent, qsl_rcvd=:qsl_rcvd,
            lotw_sent=:lotw_sent, lotw_rcvd=:lotw_rcvd,
            eqsl_sent=:eqsl_sent, eqsl_rcvd=:eqsl_rcvd,
            clublog_sent=:clublog_sent
        WHERE id=:id
    )");

    q.bindValue(":id",            qso.id);
    q.bindValue(":callsign",      qso.callsign.toUpper());
    q.bindValue(":date",          qso.date);
    q.bindValue(":time_on",       qso.timeOn);
    q.bindValue(":time_off",      qso.timeOff);
    q.bindValue(":band",          qso.band);
    q.bindValue(":freq",          qso.freq > 0 ? qso.freq : QVariant());
    q.bindValue(":mode",          qso.mode);
    q.bindValue(":submode",       qso.submode);
    q.bindValue(":rst_sent",      qso.rstSent);
    q.bindValue(":rst_rcvd",      qso.rstRcvd);
    q.bindValue(":name",          qso.name);
    q.bindValue(":qth",           qso.qth);
    q.bindValue(":country",       qso.country);
    q.bindValue(":dxcc_id",       qso.dxccId ? qso.dxccId : QVariant());
    q.bindValue(":cq_zone",       qso.cqZone  ? qso.cqZone  : QVariant());
    q.bindValue(":itu_zone",      qso.ituZone ? qso.ituZone : QVariant());
    q.bindValue(":continent",     qso.continent);
    q.bindValue(":lat",           qso.lat  != 0.0 ? qso.lat  : QVariant());
    q.bindValue(":lon",           qso.lon  != 0.0 ? qso.lon  : QVariant());
    q.bindValue(":state",         qso.state);
    q.bindValue(":gridsquare",    qso.gridsquare);
    q.bindValue(":distance",      qso.distance ? qso.distance : QVariant());
    q.bindValue(":bearing",       qso.bearing  ? qso.bearing  : QVariant());
    q.bindValue(":power",         qso.power > 0 ? qso.power : QVariant());
    q.bindValue(":comment",       qso.comment);
    q.bindValue(":notes",         qso.notes);
    q.bindValue(":iota",          qso.iota);
    q.bindValue(":sota_ref",      qso.sotaRef);
    q.bindValue(":pota_ref",      qso.potaRef);
    q.bindValue(":contest_id",    qso.contestId);
    q.bindValue(":srx",           qso.srx);
    q.bindValue(":stx",           qso.stx);
    q.bindValue(":manager",       qso.manager);
    q.bindValue(":operator_call", qso.operatorCall);
    q.bindValue(":station_call",  qso.stationCall);
    q.bindValue(":qsl_sent",      qso.qslSent);
    q.bindValue(":qsl_rcvd",      qso.qslRcvd);
    q.bindValue(":lotw_sent",     qso.lotwSent);
    q.bindValue(":lotw_rcvd",     qso.lotwRcvd);
    q.bindValue(":eqsl_sent",     qso.eqslSent);
    q.bindValue(":eqsl_rcvd",     qso.eqslRcvd);
    q.bindValue(":clublog_sent",  qso.clublogSent ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "updateQso:" << q.lastError().text();
        return false;
    }
    invalidateCache();
    emit qsoUpdated(qso.id);
    return true;
}

bool Database::deleteQso(int id)
{
    QSqlQuery q(db());
    q.prepare("DELETE FROM qso WHERE id=:id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "deleteQso:" << q.lastError().text();
        return false;
    }
    invalidateCache();
    emit qsoDeleted(id);
    return true;
}

Qso Database::getQso(int id) const
{
    QSqlQuery q(db());
    q.prepare("SELECT * FROM qso WHERE id=:id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return rowToQso(q);
    return Qso{};
}

QList<Qso> Database::getAllQsos(const QsoFilter& filter) const
{
    // Cache valida solo per l'ordinamento di default (date DESC) senza filtri
    bool isDefaultSort = (filter.orderBy.isEmpty() || filter.orderBy == "date") &&
                         (filter.orderDir.isEmpty() || filter.orderDir.toUpper() == "DESC");
    if (!filter.hasFilter() && isDefaultSort) {
        if (m_cacheValid) return m_cache;
    }

    QString sql = "SELECT * FROM qso WHERE log_id=:log";
    if (!filter.callsign.isEmpty())  sql += " AND callsign LIKE :cs";
    if (!filter.band.isEmpty())      sql += " AND band=:band";
    if (!filter.mode.isEmpty())      sql += " AND mode=:mode";
    if (!filter.dateFrom.isEmpty())  sql += " AND date>=:df";
    if (!filter.dateTo.isEmpty())    sql += " AND date<=:dt";
    if (!filter.continent.isEmpty()) sql += " AND continent=:cont";
    if (!filter.country.isEmpty())   sql += " AND country=:country";
    if (filter.lotwConfirmed)        sql += " AND lotw_rcvd='Y'";
    if (filter.lotwNotSent)         sql += " AND (lotw_sent IS NULL OR lotw_sent!='Y')";
    if (filter.eqslNotSent)         sql += " AND (eqsl_sent IS NULL OR eqsl_sent!='Y')";
    if (filter.clublogNotSent)      sql += " AND (clublog_sent IS NULL OR clublog_sent!='Y')";

    static const QStringList validCols = {
        "date","callsign","band","mode","rst_sent","rst_rcvd",
        "country","name","qth","continent","cq_zone","id","created_at"
    };
    QString orderCol = filter.orderBy.isEmpty() ? "date" : filter.orderBy;
    if (!validCols.contains(orderCol)) orderCol = "date";
    QString dir = (filter.orderDir.toUpper() == "ASC") ? "ASC" : "DESC";
    sql += " ORDER BY " + orderCol + " " + dir + ", time_on " + dir;

    QSqlQuery q(db());
    q.prepare(sql);
    q.bindValue(":log", m_currentLog);
    if (!filter.callsign.isEmpty())  q.bindValue(":cs",     "%" + filter.callsign.toUpper() + "%");
    if (!filter.band.isEmpty())      q.bindValue(":band",   filter.band);
    if (!filter.mode.isEmpty())      q.bindValue(":mode",   filter.mode);
    if (!filter.dateFrom.isEmpty())  q.bindValue(":df",     filter.dateFrom);
    if (!filter.dateTo.isEmpty())    q.bindValue(":dt",     filter.dateTo);
    if (!filter.continent.isEmpty()) q.bindValue(":cont",   filter.continent);
    if (!filter.country.isEmpty())   q.bindValue(":country",filter.country);

    QList<Qso> list;
    if (q.exec()) {
        while (q.next())
            list.append(rowToQso(q));
    } else {
        qWarning() << "getAllQsos:" << q.lastError().text();
    }

    if (!filter.hasFilter()) {
        m_cache      = list;
        m_cacheValid = true;
        m_countCache = list.size();
    }
    return list;
}

QList<Qso> Database::searchQsos(const QString& callsign) const
{
    QsoFilter f;
    f.callsign = callsign;
    return getAllQsos(f);
}

int Database::countQsos() const
{
    if (m_countCache >= 0) return m_countCache;
    QSqlQuery q(db());
    q.prepare("SELECT COUNT(*) FROM qso WHERE log_id=:log");
    q.bindValue(":log", m_currentLog);
    if (q.exec() && q.next())
        m_countCache = q.value(0).toInt();
    return m_countCache;
}

bool Database::qsoExists(const QString& callsign, const QString& date,
                          const QString& timeOn, const QString& band,
                          const QString& mode) const
{
    QSqlQuery q(db());
    q.prepare("SELECT 1 FROM qso WHERE log_id=:log AND callsign=:cs AND date=:dt "
              "AND time_on=:ton AND band=:band AND mode=:mode LIMIT 1");
    q.bindValue(":log",  m_currentLog);
    q.bindValue(":cs",   callsign.toUpper());
    q.bindValue(":dt",   date);
    q.bindValue(":ton",  timeOn);
    q.bindValue(":band", band);
    q.bindValue(":mode", mode);
    return q.exec() && q.next();
}

// ---------------------------------------------------------------------------
// Logs
// ---------------------------------------------------------------------------
QList<LogInfo> Database::getLogs() const
{
    QSqlQuery q(db());
    q.exec("SELECT id,name,callsign,qth,locator,created FROM logs ORDER BY id");
    QList<LogInfo> list;
    while (q.next()) {
        LogInfo l;
        l.id       = q.value(0).toInt();
        l.name     = q.value(1).toString();
        l.callsign = q.value(2).toString();
        l.qth      = q.value(3).toString();
        l.locator  = q.value(4).toString();
        l.created  = q.value(5).toString();
        list.append(l);
    }
    return list;
}

bool Database::addLog(const QString& name, const QString& call,
                      const QString& qth, const QString& locator)
{
    QSqlQuery q(db());
    q.prepare("INSERT INTO logs (name,callsign,qth,locator) VALUES (:n,:c,:q,:l)");
    q.bindValue(":n", name);
    q.bindValue(":c", call);
    q.bindValue(":q", qth);
    q.bindValue(":l", locator);
    return q.exec();
}

bool Database::deleteLog(int id, bool deleteQsos)
{
    QSqlDatabase d = db();
    d.transaction();
    QSqlQuery q(d);
    if (deleteQsos) {
        q.prepare("DELETE FROM qso WHERE log_id=:id");
        q.bindValue(":id", id);
        if (!q.exec()) { d.rollback(); return false; }
    } else {
        q.prepare("UPDATE qso SET log_id=1 WHERE log_id=:id");
        q.bindValue(":id", id);
        if (!q.exec()) { d.rollback(); return false; }
    }
    q.prepare("DELETE FROM logs WHERE id=:id AND id!=1");
    q.bindValue(":id", id);
    if (!q.exec()) { d.rollback(); return false; }
    d.commit();
    invalidateCache();
    return true;
}

bool Database::updateLog(const LogInfo& info)
{
    QSqlQuery q(db());
    q.prepare("UPDATE logs SET name=:n,callsign=:c,qth=:q,locator=:l WHERE id=:id");
    q.bindValue(":id", info.id);
    q.bindValue(":n",  info.name);
    q.bindValue(":c",  info.callsign);
    q.bindValue(":q",  info.qth);
    q.bindValue(":l",  info.locator);
    return q.exec();
}

int  Database::currentLogId() const { return m_currentLog; }
void Database::setCurrentLog(int id)
{
    m_currentLog = id;
    invalidateCache();
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
Stats Database::getStats(int logId) const
{
    int lid = (logId > 0) ? logId : m_currentLog;
    Stats s{};

    QSqlQuery q(db());
    q.prepare("SELECT COUNT(*) FROM qso WHERE log_id=:l");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.totalQsos = q.value(0).toInt();

    q.prepare("SELECT COUNT(DISTINCT callsign) FROM qso WHERE log_id=:l");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.uniqueCalls = q.value(0).toInt();

    q.prepare("SELECT COUNT(DISTINCT country) FROM qso WHERE log_id=:l AND country!=''");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.countries = q.value(0).toInt();

    q.prepare("SELECT COUNT(DISTINCT cq_zone) FROM qso WHERE log_id=:l AND cq_zone IS NOT NULL");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.cqZones = q.value(0).toInt();

    q.prepare("SELECT COUNT(DISTINCT itu_zone) FROM qso WHERE log_id=:l AND itu_zone IS NOT NULL");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.ituZones = q.value(0).toInt();

    q.prepare("SELECT COUNT(*) FROM qso WHERE log_id=:l AND lotw_rcvd='Y'");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.lotwConfirmed = q.value(0).toInt();

    q.prepare("SELECT COUNT(*) FROM qso WHERE log_id=:l AND eqsl_rcvd='Y'");
    q.bindValue(":l", lid);
    if (q.exec() && q.next()) s.eqslConfirmed = q.value(0).toInt();

    // Band breakdown
    q.prepare("SELECT band, COUNT(*) FROM qso WHERE log_id=:l GROUP BY band");
    q.bindValue(":l", lid);
    if (q.exec())
        while (q.next())
            s.byBand[q.value(0).toString()] = q.value(1).toInt();

    // Mode breakdown
    q.prepare("SELECT mode, COUNT(*) FROM qso WHERE log_id=:l GROUP BY mode");
    q.bindValue(":l", lid);
    if (q.exec())
        while (q.next())
            s.byMode[q.value(0).toString()] = q.value(1).toInt();

    // Continent breakdown
    q.prepare("SELECT continent, COUNT(*) FROM qso WHERE log_id=:l AND continent!='' GROUP BY continent");
    q.bindValue(":l", lid);
    if (q.exec())
        while (q.next())
            s.byContinent[q.value(0).toString()] = q.value(1).toInt();

    return s;
}

// ---------------------------------------------------------------------------
// Worked / Confirmed entities
// ---------------------------------------------------------------------------
QHash<QString,bool> Database::workedEntities(const QString& band,
                                              const QString& mode) const
{
    QString sql = "SELECT DISTINCT country FROM qso WHERE log_id=:l AND country!=''";
    if (!band.isEmpty()) sql += " AND band=:band";
    if (!mode.isEmpty()) sql += " AND mode=:mode";

    QSqlQuery q(db());
    q.prepare(sql);
    q.bindValue(":l", m_currentLog);
    if (!band.isEmpty()) q.bindValue(":band", band);
    if (!mode.isEmpty()) q.bindValue(":mode", mode);

    QHash<QString,bool> result;
    if (q.exec())
        while (q.next())
            result[q.value(0).toString()] = true;
    return result;
}

QHash<QString,bool> Database::confirmedEntities(const QString& band,
                                                 const QString& mode) const
{
    QString sql = "SELECT DISTINCT country FROM qso WHERE log_id=:l "
                  "AND country!='' AND (lotw_rcvd='Y' OR qsl_rcvd='Y' OR eqsl_rcvd='Y')";
    if (!band.isEmpty()) sql += " AND band=:band";
    if (!mode.isEmpty()) sql += " AND mode=:mode";

    QSqlQuery q(db());
    q.prepare(sql);
    q.bindValue(":l", m_currentLog);
    if (!band.isEmpty()) q.bindValue(":band", band);
    if (!mode.isEmpty()) q.bindValue(":mode", mode);

    QHash<QString,bool> result;
    if (q.exec())
        while (q.next())
            result[q.value(0).toString()] = true;
    return result;
}

// ---------------------------------------------------------------------------
// QSL markings (batch)
// ---------------------------------------------------------------------------
static bool batchUpdate(QSqlDatabase d, const QString& col,
                        const QList<int>& ids, const QVariant& value)
{
    if (ids.isEmpty()) return true;
    d.transaction();
    QSqlQuery q(d);
    q.prepare("UPDATE qso SET " + col + "=:v WHERE id=:id");
    for (int id : ids) {
        q.bindValue(":v",  value);
        q.bindValue(":id", id);
        if (!q.exec()) { d.rollback(); return false; }
    }
    d.commit();
    return true;
}

bool Database::markLotwSent(const QList<int>& ids, const QString& s)
{ bool ok = batchUpdate(db(),"lotw_sent",ids,s); if(ok) invalidateCache(); return ok; }

bool Database::markLotwRcvd(const QList<int>& ids, const QString& s)
{ bool ok = batchUpdate(db(),"lotw_rcvd",ids,s); if(ok) invalidateCache(); return ok; }

bool Database::markEqslSent(const QList<int>& ids, const QString& s)
{ bool ok = batchUpdate(db(),"eqsl_sent",ids,s); if(ok) invalidateCache(); return ok; }

bool Database::markEqslRcvd(const QList<int>& ids, const QString& s)
{ bool ok = batchUpdate(db(),"eqsl_rcvd",ids,s); if(ok) invalidateCache(); return ok; }

bool Database::markQslSent(const QList<int>& ids, const QString& s)
{ bool ok = batchUpdate(db(),"qsl_sent",ids,s); if(ok) invalidateCache(); return ok; }

bool Database::markQslRcvd(const QList<int>& ids, const QString& s)
{ bool ok = batchUpdate(db(),"qsl_rcvd",ids,s); if(ok) invalidateCache(); return ok; }

bool Database::markClublog(const QList<int>& ids, bool sent)
{ bool ok = batchUpdate(db(),"clublog_sent",ids,sent?1:0); if(ok) invalidateCache(); return ok; }

// ---------------------------------------------------------------------------
// DXCC enrichment
// ---------------------------------------------------------------------------
int Database::enrichMissingDxcc()
{
    QSqlQuery q(db());
    q.prepare("SELECT id,callsign FROM qso WHERE log_id=:l AND "
              "(country IS NULL OR country='')");
    q.bindValue(":l", m_currentLog);
    if (!q.exec()) return 0;

    QList<QPair<int,QString>> rows;
    while (q.next())
        rows.append({q.value(0).toInt(), q.value(1).toString()});

    if (rows.isEmpty()) return 0;

    int updated = 0;
    QSqlDatabase d = db();
    d.transaction();
    QSqlQuery upd(d);
    upd.prepare("UPDATE qso SET country=:c, continent=:cont, "
                "cq_zone=:cq, itu_zone=:itu, lat=:lat, lon=:lon "
                "WHERE id=:id");

    for (const auto& [id, call] : rows) {
        DxccInfo info = Dxcc::lookup(call);
        if (info.entity.isEmpty()) continue;
        upd.bindValue(":c",    info.entity);
        upd.bindValue(":cont", info.continent);
        upd.bindValue(":cq",   info.cqZone);
        upd.bindValue(":itu",  info.ituZone);
        upd.bindValue(":lat",  info.lat);
        upd.bindValue(":lon",  info.lon);
        upd.bindValue(":id",   id);
        if (upd.exec()) ++updated;
    }
    d.commit();
    if (updated > 0) invalidateCache();
    return updated;
}

// ---------------------------------------------------------------------------
// Callbook backfill (name / qth / grid for existing QSOs)
// ---------------------------------------------------------------------------
QStringList Database::callsignsMissingNameQth() const
{
    QSqlQuery q(db());
    q.prepare(R"(
        SELECT DISTINCT callsign FROM qso
        WHERE log_id=:l AND ((name IS NULL OR name='') OR (qth IS NULL OR qth=''))
        ORDER BY callsign
    )");
    q.bindValue(":l", m_currentLog);
    QStringList result;
    if (q.exec())
        while (q.next())
            result << q.value(0).toString();
    return result;
}

int Database::applyCallbookToQsos(const QString& callsign,
                                   const QString& name,
                                   const QString& qth,
                                   const QString& grid)
{
    QSqlDatabase d = db();
    QSqlQuery q(d);
    q.prepare(R"(
        UPDATE qso SET
            name = CASE WHEN (name IS NULL OR name='') THEN :name ELSE name END,
            qth  = CASE WHEN (qth  IS NULL OR qth ='') THEN :qth  ELSE qth  END,
            gridsquare = CASE WHEN (gridsquare IS NULL OR gridsquare='') THEN :grid ELSE gridsquare END
        WHERE log_id=:l AND callsign=:cs
    )");
    q.bindValue(":name", name);
    q.bindValue(":qth",  qth);
    q.bindValue(":grid", grid);
    q.bindValue(":l",    m_currentLog);
    q.bindValue(":cs",   callsign.toUpper());
    if (!q.exec()) return 0;
    int n = q.numRowsAffected();
    if (n > 0) invalidateCache();
    return n;
}

// ---------------------------------------------------------------------------
// Integrity
// ---------------------------------------------------------------------------
QList<QHash<QString,QVariant>> Database::findDuplicates() const
{
    QSqlQuery q(db());
    q.prepare(R"(
        SELECT a.id, a.callsign, a.date, a.time_on, a.band, a.mode, b.id as dup_id
        FROM qso a JOIN qso b
          ON a.callsign=b.callsign AND a.date=b.date
          AND a.time_on=b.time_on AND a.band=b.band AND a.mode=b.mode
          AND a.id < b.id
        WHERE a.log_id=:l AND b.log_id=:l
    )");
    q.bindValue(":l", m_currentLog);

    QList<QHash<QString,QVariant>> result;
    if (q.exec()) {
        while (q.next()) {
            QHash<QString,QVariant> row;
            row["id"]       = q.value("id");
            row["callsign"] = q.value("callsign");
            row["date"]     = q.value("date");
            row["time_on"]  = q.value("time_on");
            row["band"]     = q.value("band");
            row["mode"]     = q.value("mode");
            row["dup_id"]   = q.value("dup_id");
            result.append(row);
        }
    }
    return result;
}

QList<QHash<QString,QVariant>> Database::findDuplicateQsos() const
{
    // Return every QSO that shares (callsign, date, band, mode) with at least one other.
    // Ordered so rows in the same group are contiguous, lowest id first.
    QSqlQuery q(db());
    q.prepare(R"(
        SELECT a.id, a.callsign, a.date, a.time_on, a.band, a.mode,
               COALESCE(a.name,'') AS name
        FROM qso a
        WHERE a.log_id = :l
          AND EXISTS (
              SELECT 1 FROM qso b
              WHERE b.log_id=a.log_id
                AND b.callsign=a.callsign AND b.date=a.date
                AND b.band=a.band        AND b.mode=a.mode
                AND b.id != a.id
          )
        ORDER BY a.callsign, a.date, a.band, a.mode, a.id
    )");
    q.bindValue(":l", m_currentLog);

    QList<QHash<QString,QVariant>> result;
    if (q.exec()) {
        while (q.next()) {
            QHash<QString,QVariant> row;
            row["id"]       = q.value("id");
            row["callsign"] = q.value("callsign");
            row["date"]     = q.value("date");
            row["time_on"]  = q.value("time_on");
            row["band"]     = q.value("band");
            row["mode"]     = q.value("mode");
            row["name"]     = q.value("name");
            result.append(row);
        }
    }
    return result;
}

int Database::removeDuplicates()
{
    auto dups = findDuplicates();
    if (dups.isEmpty()) return 0;

    QSqlDatabase d = db();
    d.transaction();
    QSqlQuery q(d);
    q.prepare("DELETE FROM qso WHERE id=:id");
    int removed = 0;
    for (const auto& dup : dups) {
        q.bindValue(":id", dup["dup_id"]);
        if (q.exec()) ++removed;
    }
    d.commit();
    if (removed > 0) invalidateCache();
    return removed;
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------
bool Database::importQsos(const QList<Qso>& qsos, int* duplicatesOut)
{
    int dups = 0;
    QList<int> savedIds;

    QSqlDatabase d = db();
    d.transaction();

    {
        // Block all signals while inserting so UI doesn't query mid-transaction
        QSignalBlocker blocker(this);

        for (Qso q : qsos) {
            if (qsoExists(q.callsign, q.date, q.timeOn, q.band, q.mode)) {
                ++dups;
                continue;
            }
            q.logId = m_currentLog;

            if (q.country.isEmpty()) {
                DxccInfo info = Dxcc::lookup(q.callsign);
                if (!info.entity.isEmpty()) {
                    q.country   = info.entity;
                    q.continent = info.continent;
                    q.cqZone    = info.cqZone;
                    q.ituZone   = info.ituZone;
                    q.lat       = info.lat;
                    q.lon       = info.lon;
                }
            }

            if (saveQso(q))   // inserts + sets q.id (signal blocked)
                savedIds.append(q.id);
        }
    } // QSignalBlocker released here

    d.commit();
    if (duplicatesOut) *duplicatesOut = dups;
    invalidateCache();

    // Now emit one signal per inserted QSO (data is committed and readable)
    for (int id : savedIds)
        emit qsoSaved(id);

    return true;
}

// ---------------------------------------------------------------------------
// Callbook cache
// ---------------------------------------------------------------------------
bool Database::saveCallbookCache(const QString& callsign,
                                  const QHash<QString,QString>& data)
{
    QStringList pairs;
    for (auto it = data.begin(); it != data.end(); ++it)
        pairs.append(it.key() + "=" + it.value());

    QSqlQuery q(db());
    q.prepare("INSERT OR REPLACE INTO callbook_cache (callsign,data,fetched) "
              "VALUES (:cs,:d,datetime('now'))");
    q.bindValue(":cs", callsign.toUpper());
    q.bindValue(":d",  pairs.join('\n'));
    return q.exec();
}

QHash<QString,QString> Database::getCallbookCache(const QString& callsign) const
{
    QSqlQuery q(db());
    q.prepare("SELECT data, fetched FROM callbook_cache WHERE callsign=:cs");
    q.bindValue(":cs", callsign.toUpper());

    QHash<QString,QString> result;
    if (!q.exec() || !q.next()) return result;

    // Expire after 30 days (SQLite datetime() format: "yyyy-MM-dd HH:mm:ss")
    QDateTime fetched = QDateTime::fromString(q.value(1).toString(), "yyyy-MM-dd HH:mm:ss");
    if (fetched.daysTo(QDateTime::currentDateTimeUtc()) > 30)
        return result;

    for (const QString& line : q.value(0).toString().split('\n')) {
        int eq = line.indexOf('=');
        if (eq > 0)
            result[line.left(eq)] = line.mid(eq+1);
    }
    return result;
}
