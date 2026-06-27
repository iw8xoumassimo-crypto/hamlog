#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QTcpSocket>
#include <QTcpServer>
#include <QHash>
#include "types.h"

// ---------------------------------------------------------------------------
// WSJT-X UDP listener
// Listens on UDP port 2237 for WSJT-X/JTDX datagrams (magic 0xADBCCBDA).
// ---------------------------------------------------------------------------
class WsjtxListener : public QObject
{
    Q_OBJECT
public:
    explicit WsjtxListener(QObject* parent = nullptr);
    ~WsjtxListener() override;

    bool start(quint16 port = 2237);
    void stop();
    bool isListening() const;

signals:
    void qsoLogged(const Qso& qso);
    void statusChanged(const QString& callsign, double freq, const QString& mode);
    void error(const QString& message);

private slots:
    void onDatagram();

private:
    void parseQsoLogged(const QByteArray& payload);
    void parseStatus    (const QByteArray& payload);

    class QUdpSocket* m_sock = nullptr;
    quint16           m_port = 2237;
};

// ---------------------------------------------------------------------------
// Decodium ADIF TCP server
// Listens on TCP port 2333 — Decodium connects here (ADIFTcpEnabled=true,
// ADIFTcpServer=127.0.0.1, ADIFTcpPort=2333) and pushes ADIF records.
// Accepts multiple simultaneous connections.
// ---------------------------------------------------------------------------
class AdifTcpServer : public QObject
{
    Q_OBJECT
public:
    explicit AdifTcpServer(QObject* parent = nullptr);
    ~AdifTcpServer() override;

    bool listen(quint16 port = 2333);
    void close();

signals:
    void qsosReceived(const QList<Qso>& qsos);
    void clientConnected();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    class QTcpServer* m_server = nullptr;
    QHash<QTcpSocket*, QByteArray> m_bufs;
};

// ---------------------------------------------------------------------------
// Decodium / generic ADIF file watcher
// Watches a file for changes and imports new QSOs from it.
// ---------------------------------------------------------------------------
class AdifWatcher : public QObject
{
    Q_OBJECT
public:
    explicit AdifWatcher(QObject* parent = nullptr);

    void watch(const QString& filePath);
    void unwatch();

signals:
    void qsosFound(const QList<Qso>& qsos);
    void fileAppeared();   // emesso la prima volta che il file viene trovato sul disco

private slots:
    void onFileChanged(const QString& path);
    void onPollTimer();

private:
    void checkFile();

    QFileSystemWatcher m_watcher;
    QTimer*            m_pollTimer = nullptr;
    QString            m_path;
    qint64             m_lastPos   = 0;
    bool               m_fileKnown = false;  // true dopo la prima apertura riuscita
};
