#pragma once
#include <QString>
#include <QList>
#include <QHash>
#include <QVariant>
#include <QObject>
#include "types.h"

class QSqlDatabase;

class Database : public QObject
{
    Q_OBJECT

public:
    static Database& instance();

    bool open(const QString& path);
    void close();
    bool isOpen() const;
    QString path() const;

    // QSO CRUD
    bool          saveQso(Qso& qso);          // sets qso.id on insert; emits qsoSaved
    bool          updateQso(const Qso& qso);
    bool          deleteQso(int id);
    Qso           getQso(int id) const;
    QList<Qso>    getAllQsos(const QsoFilter& filter = QsoFilter()) const;
    QList<Qso>    searchQsos(const QString& callsign) const;
    int           countQsos() const;
    bool          qsoExists(const QString& callsign, const QString& date,
                            const QString& timeOn, const QString& band,
                            const QString& mode) const;

    // Logs / books
    QList<LogInfo> getLogs() const;
    bool           addLog(const QString& name, const QString& call,
                          const QString& qth, const QString& locator);
    bool           deleteLog(int id, bool deleteQsos = false);
    bool           updateLog(const LogInfo& info);
    int            currentLogId() const;
    void           setCurrentLog(int id);

    // Stats
    Stats          getStats(int logId = -1) const;

    // DXCC worked / confirmed
    QHash<QString,bool> workedEntities(const QString& band = QString(),
                                       const QString& mode = QString()) const;
    QHash<QString,bool> confirmedEntities(const QString& band = QString(),
                                          const QString& mode = QString()) const;

    // QSL markings
    bool markLotwSent(const QList<int>& ids, const QString& status = "Y");
    bool markLotwRcvd(const QList<int>& ids, const QString& status = "Y");
    bool markEqslSent(const QList<int>& ids, const QString& status = "Y");
    bool markEqslRcvd(const QList<int>& ids, const QString& status = "Y");
    bool markQslSent (const QList<int>& ids, const QString& status = "Y");
    bool markQslRcvd (const QList<int>& ids, const QString& status = "Y");
    bool markClublog (const QList<int>& ids, bool sent);

    // DXCC enrichment
    int  enrichMissingDxcc();

    // Returns distinct callsigns that have QSOs with empty name/qth fields
    QStringList callsignsMissingNameQth() const;
    // Update name+qth on ALL qsos matching callsign (from callbook data)
    int  applyCallbookToQsos(const QString& callsign,
                             const QString& name,
                             const QString& qth,
                             const QString& grid);

    // Integrity
    QList<QHash<QString,QVariant>> findDuplicates() const;
    // Returns ALL duplicate QSO records (grouped: first in group = original, rest = copies)
    QList<QHash<QString,QVariant>> findDuplicateQsos() const;
    int removeDuplicates();

    // Import / Export helpers
    bool importQsos(const QList<Qso>& qsos, int* duplicatesOut = nullptr);

    // Callbook cache
    bool saveCallbookCache(const QString& callsign,
                           const QHash<QString,QString>& data);
    QHash<QString,QString> getCallbookCache(const QString& callsign) const;

signals:
    void qsoSaved(int id);
    void qsoUpdated(int id);
    void qsoDeleted(int id);
    void databaseOpened(const QString& path);
    void databaseClosed();

private:
    explicit Database(QObject* parent = nullptr);
    ~Database() override;
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    void     createSchema();
    QSqlDatabase db() const;
    void     invalidateCache();

    QString  m_path;
    int      m_currentLog = 1;

    // mutable cache
    mutable bool         m_cacheValid = false;
    mutable QList<Qso>   m_cache;
    mutable int          m_countCache = -1;
};
