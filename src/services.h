#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include "types.h"

class QNetworkAccessManager;

// ---------------------------------------------------------------------------
// Base async service
// ---------------------------------------------------------------------------
class OnlineService : public QObject
{
    Q_OBJECT
public:
    explicit OnlineService(QObject* parent = nullptr);

    // Call from the UI thread to stop the current operation between requests
    void cancelPending() { m_cancel = 1; }
    void resetCancel()   { m_cancel = 0; }
    bool isCancelled()   { return m_cancel != 0; }

signals:
    void progress(const QString& message);
    void finished(bool success, const QString& message);
    void qsoStatusUpdated(int qsoId, const QString& field, const QString& value);

protected:
    QNetworkAccessManager* nam();
    QByteArray syncGet (const QString& url, int timeoutMs = 15000,
                        int* httpStatusOut = nullptr,
                        const QHash<QString,QString>& extraHeaders = {});
    QByteArray syncPost(const QString& url, const QByteArray& data,
                        const QString& contentType = "application/x-www-form-urlencoded",
                        int timeoutMs = 30000);

    int m_cancel = 0;   // checked between requests; set via cancelPending()

private:
    QNetworkAccessManager* m_nam = nullptr;
};

// ---------------------------------------------------------------------------
// LoTW upload (tqsl command-line wrapper)
// ---------------------------------------------------------------------------
class LotwService : public OnlineService
{
    Q_OBJECT
public:
    explicit LotwService(QObject* parent = nullptr);

public slots:
    void upload(const QString& adifFile, const QString& station,
                const QString& tqslPath = "tqsl");
    void fetchQsls(const QString& username, const QString& password,
                   const QString& since = QString());

signals:
    void uploadFinished(bool ok, const QString& msg);
    void qslsReceived(const QList<Qso>& confirmed);
};

// ---------------------------------------------------------------------------
// eQSL
// ---------------------------------------------------------------------------
class EqslService : public OnlineService
{
    Q_OBJECT
public:
    explicit EqslService(QObject* parent = nullptr);

public slots:
    void uploadAdif(const QString& username, const QString& password,
                    const QString& adifData);
    void fetchInbox(const QString& username, const QString& password,
                    const QString& since = QString());
    void fetchImages(const QString& username, const QString& password,
                     const QList<Qso>& qsls, const QString& saveDir,
                     const QString& myCallsign = QString());

private:
    bool loginSession(const QString& username, const QString& password);

signals:
    void uploadFinished(bool ok, int count, const QString& msg);
    void inboxReceived(const QList<Qso>& qsls);
    void imageDownloaded(const QString& callsign, const QString& date,
                         const QString& band, const QString& localPath);
    void imagesComplete(int downloaded, int total);
    void progressPct(int percent);  // 0..100 during fetchImages

private:
    static QString bandToEqslMhz(const QString& adifBand);
};

// ---------------------------------------------------------------------------
// ClubLog
// ---------------------------------------------------------------------------
class ClublogService : public OnlineService
{
    Q_OBJECT
public:
    explicit ClublogService(QObject* parent = nullptr);

public slots:
    void uploadAdif(const QString& email, const QString& password,
                    const QString& callsign, const QString& adifData);

signals:
    void uploadFinished(bool ok, const QString& msg);
};

// ---------------------------------------------------------------------------
// HamQTH callbook
// ---------------------------------------------------------------------------
class HamQthService : public OnlineService
{
    Q_OBJECT
public:
    explicit HamQthService(QObject* parent = nullptr);
    void setCredentials(const QString& user, const QString& pass);

public slots:
    void lookup(const QString& callsign);

public:
    // Synchronous variant — for use in loops/threads where signal/slot is inconvenient.
    // Returns data directly; may contain "_error" key on failure.
    QHash<QString,QString> lookupSync(const QString& callsign);

signals:
    void lookupResult(const QString& callsign,
                      const QHash<QString,QString>& data,
                      bool fromCache);

private:
    QString m_user, m_pass, m_session, m_lastError;
    bool    login();
    QHash<QString,QString> fetchAndParse(const QString& callsign);
};

// ---------------------------------------------------------------------------
// HamDB (free, no credentials)
// ---------------------------------------------------------------------------
class HamDbService : public OnlineService
{
    Q_OBJECT
public:
    explicit HamDbService(QObject* parent = nullptr);

public slots:
    void lookup(const QString& callsign);

public:
    QHash<QString,QString> lookupSync(const QString& callsign);

signals:
    void lookupResult(const QString& callsign,
                      const QHash<QString,QString>& data);
};
