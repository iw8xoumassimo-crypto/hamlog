#include "udp_listener.h"
#include "adif.h"
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QDebug>
#include <QAbstractSocket>

static constexpr quint32 WSJTX_MAGIC   = 0xADBCCBDA;
static constexpr quint32 WSJTX_SCHEMA  = 2;

enum WsjtxMsg : quint32 {
    Heartbeat    = 0,
    Status       = 1,
    Decode       = 2,
    Clear        = 3,
    Reply        = 4,
    QsoLogged    = 5,
    Close        = 6,
    Replay       = 7,
    HaltTx       = 8,
    FreeText     = 9,
    WSPRDecode   = 10,
    Location     = 11,
    LoggedAdif   = 12,
    HighlightCall= 13,
    SwitchConf   = 14,
    Configure    = 15,
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QString readUtf8(QDataStream& ds)
{
    quint32 len;
    ds >> len;
    if (len == 0xFFFFFFFF || len == 0) return {};
    QByteArray ba(len, '\0');
    ds.readRawData(ba.data(), len);
    return QString::fromUtf8(ba);
}

static bool readBool(QDataStream& ds)
{
    quint8 v; ds >> v; return v != 0;
}

// ---------------------------------------------------------------------------
// WsjtxListener
// ---------------------------------------------------------------------------
WsjtxListener::WsjtxListener(QObject* parent) : QObject(parent) {}

WsjtxListener::~WsjtxListener() { stop(); }

bool WsjtxListener::start(quint16 port)
{
    m_port = port;
    if (!m_sock) m_sock = new QUdpSocket(this);
    if (!m_sock->bind(QHostAddress::Any, port,
                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit error(QString("Cannot bind UDP port %1: %2").arg(port).arg(m_sock->errorString()));
        return false;
    }
    connect(m_sock, &QUdpSocket::readyRead, this, &WsjtxListener::onDatagram);
    return true;
}

void WsjtxListener::stop()
{
    if (m_sock) { m_sock->close(); m_sock->deleteLater(); m_sock = nullptr; }
}

bool WsjtxListener::isListening() const
{
    return m_sock && m_sock->state() == QAbstractSocket::BoundState;
}

void WsjtxListener::onDatagram()
{
    while (m_sock->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_sock->receiveDatagram();
        QByteArray data = dg.data();
        QDataStream ds(data);
        ds.setByteOrder(QDataStream::BigEndian);
        ds.setVersion(QDataStream::Qt_5_0);

        quint32 magic, schema, msgType, id;
        ds >> magic >> schema >> msgType;
        readUtf8(ds); // unique ID (skip via length-prefixed read)

        if (magic != WSJTX_MAGIC) {
            // Not WSJT-X binary — try plain ADIF text (Decodium UDP export)
            QString text = QString::fromUtf8(data).trimmed();
            if (text.contains('<')) {
                QTemporaryFile tmp;
                if (tmp.open()) {
                    tmp.write(data);
                    tmp.flush();
                    QList<Qso> qsos = AdifHandler::importAdif(tmp.fileName());
                    for (const Qso& q : qsos) emit qsoLogged(q);
                }
            }
            continue;
        }

        switch (msgType) {
        case QsoLogged:   parseQsoLogged(data); break;
        case Status:      parseStatus(data);    break;
        case LoggedAdif: {
            QString adif = readUtf8(ds);
            QTemporaryFile tmp;
            if (tmp.open()) {
                tmp.write(adif.toUtf8());
                tmp.flush();
                QList<Qso> qsos = AdifHandler::importAdif(tmp.fileName());
                for (const Qso& q : qsos) emit qsoLogged(q);
            }
            break;
        }
        default: break;
        }
    }
}

void WsjtxListener::parseQsoLogged(const QByteArray& raw)
{
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::BigEndian);
    ds.setVersion(QDataStream::Qt_5_0);

    quint32 magic, schema, msgType;
    ds >> magic >> schema >> msgType;
    readUtf8(ds); // unique id

    // QDateTime in WSJT-X protocol: julian_day(u64) + ms_into_day(u32) + timespec(u8)
    auto readQDT = [](QDataStream& s) -> QDateTime {
        quint64 jd; quint32 ms; quint8 spec;
        s >> jd >> ms >> spec;
        if (jd == 0) return {};
        qint64 daysSinceEpoch = static_cast<qint64>(jd) - 2440588LL;
        return QDateTime(QDate(1970,1,1), QTime(0,0,0), Qt::UTC)
                   .addDays(daysSinceEpoch).addMSecs(ms);
    };

    QDateTime dtOff = readQDT(ds);
    QString dxCall  = readUtf8(ds);
    QString dxGrid  = readUtf8(ds);
    quint64 txFreq; ds >> txFreq;
    QString mode    = readUtf8(ds);
    QString rptSent = readUtf8(ds);
    QString rptRcvd = readUtf8(ds);
    QString txPower = readUtf8(ds);
    QString comment = readUtf8(ds);
    QString name    = readUtf8(ds);
    QDateTime dtOn  = readQDT(ds);
    QString opCall  = readUtf8(ds);
    QString myCall  = readUtf8(ds);
    QString myGrid  = readUtf8(ds);

    if (!dtOn.isValid()) dtOn = QDateTime::currentDateTimeUtc();
    if (!dtOff.isValid()) dtOff = dtOn;

    Qso q;
    q.callsign    = dxCall.toUpper();
    q.date        = dtOn.toString("yyyy-MM-dd");
    q.timeOn      = dtOn.toString("HHmm");
    q.timeOff     = dtOff.toString("HHmm");
    q.band        = Qso::freqToBand(txFreq / 1e6);
    q.freq        = txFreq / 1e6;
    q.mode        = mode;
    q.rstSent     = rptSent.isEmpty() ? "+00" : rptSent;
    q.rstRcvd     = rptRcvd.isEmpty() ? "+00" : rptRcvd;
    q.name        = name;
    q.gridsquare  = dxGrid;
    q.comment     = comment;
    q.operatorCall= opCall;
    q.stationCall = myCall;

    // Emit even without band — caller can still save with best-effort data
    if (!q.callsign.isEmpty() && !q.date.isEmpty()) emit qsoLogged(q);
}

void WsjtxListener::parseStatus(const QByteArray& raw)
{
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::BigEndian);
    ds.setVersion(QDataStream::Qt_5_0);

    quint32 magic, schema, msgType;
    ds >> magic >> schema >> msgType;
    readUtf8(ds); // id

    quint64 freq; ds >> freq;
    QString mode     = readUtf8(ds);
    QString dxCall   = readUtf8(ds);
    QString report   = readUtf8(ds);
    QString txMode   = readUtf8(ds);
    bool    txEnabled = readBool(ds);
    bool    transmitting = readBool(ds);
    bool    decoding  = readBool(ds);

    emit statusChanged(dxCall, freq / 1e6, mode);
}

// ---------------------------------------------------------------------------
// AdifTcpServer
// ---------------------------------------------------------------------------
AdifTcpServer::AdifTcpServer(QObject* parent) : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &AdifTcpServer::onNewConnection);
}

AdifTcpServer::~AdifTcpServer() { close(); }

bool AdifTcpServer::listen(quint16 port)
{
    return m_server->listen(QHostAddress::LocalHost, port);
}

void AdifTcpServer::close()
{
    m_server->close();
}

void AdifTcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        m_bufs[sock] = QByteArray();
        connect(sock, &QTcpSocket::readyRead,    this, &AdifTcpServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &AdifTcpServer::onClientDisconnected);
        emit clientConnected();
    }
}

void AdifTcpServer::onReadyRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_bufs[sock] += sock->readAll();

    // Ricerca case-insensitive di <EOR> senza copiare il buffer intero (O(N²) → O(N))
    auto findEor = [](const QByteArray& buf) -> int {
        const char* p = buf.constData();
        int n = buf.size() - 4;
        for (int i = 0; i < n; ++i) {
            if (p[i] == '<' &&
                (p[i+1] == 'E' || p[i+1] == 'e') &&
                (p[i+2] == 'O' || p[i+2] == 'o') &&
                (p[i+3] == 'R' || p[i+3] == 'r') &&
                 p[i+4] == '>')
                return i;
        }
        return -1;
    };

    while (true) {
        int eor = findEor(m_bufs[sock]);
        if (eor < 0) break;
        QByteArray record = m_bufs[sock].left(eor + 5);
        m_bufs[sock] = m_bufs[sock].mid(eor + 5);

        QTemporaryFile tmp;
        if (tmp.open()) {
            tmp.write(record);
            tmp.flush();
            QList<Qso> qsos = AdifHandler::importAdif(tmp.fileName());
            if (!qsos.isEmpty()) emit qsosReceived(qsos);
        }
    }
}

void AdifTcpServer::onClientDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) { m_bufs.remove(sock); sock->deleteLater(); }
}

// ---------------------------------------------------------------------------
// AdifWatcher
// ---------------------------------------------------------------------------
AdifWatcher::AdifWatcher(QObject* parent) : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &AdifWatcher::onFileChanged);

    // Polling fallback: QFileSystemWatcher è inaffidabile su Windows
    // per file scritti da processi esterni → poll ogni 5 s
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(5000);
    connect(m_pollTimer, &QTimer::timeout, this, &AdifWatcher::onPollTimer);
}

void AdifWatcher::watch(const QString& filePath)
{
    unwatch();
    m_path      = filePath;
    m_lastPos   = 0;
    m_fileKnown = false;

    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly)) {
        m_lastPos   = f.size();  // file esiste → parti dalla fine, ignora record precedenti
        m_fileKnown = true;
        f.close();
        m_watcher.addPath(filePath);
    }
    m_pollTimer->start();
}

void AdifWatcher::unwatch()
{
    m_pollTimer->stop();
    if (!m_path.isEmpty()) m_watcher.removePath(m_path);
    m_path.clear();
    m_lastPos   = 0;
    m_fileKnown = false;
}

void AdifWatcher::checkFile()
{
    if (m_path.isEmpty()) return;

    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return;

    // File apparso per la prima volta dopo l'avvio — notifica e aggiungi al watcher
    if (!m_fileKnown) {
        m_fileKnown = true;
        m_watcher.addPath(m_path);
        emit fileAppeared();
        // m_lastPos resta 0: legge dall'inizio, cattura tutti i QSO nuovi
    }

    qint64 size = f.size();
    if (size < m_lastPos) m_lastPos = 0;   // file rimpiazzato/troncato — rileggi dall'inizio
    if (size <= m_lastPos) { f.close(); return; }  // nessun nuovo dato

    f.seek(m_lastPos);
    QByteArray chunk = f.readAll();
    m_lastPos = size;
    f.close();

    if (chunk.isEmpty()) return;

    QTemporaryFile tmp;
    if (tmp.open()) {
        tmp.write(chunk);
        tmp.flush();
        QList<Qso> qsos = AdifHandler::importAdif(tmp.fileName());
        if (!qsos.isEmpty()) emit qsosFound(qsos);
    }

    // Re-add path se rimosso (alcuni processi rimpiazzano il file)
    if (!m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
}

void AdifWatcher::onFileChanged(const QString& /*path*/)
{
    checkFile();
}

void AdifWatcher::onPollTimer()
{
    checkFile();
}
