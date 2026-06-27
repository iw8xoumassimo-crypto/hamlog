#include "cat_service.h"
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>
#ifdef HAVE_AXCONTAINER
#  include <QAxObject>
#endif

CatService::CatService(QObject* parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &CatService::poll);
}

CatService::~CatService() { disconnectRig(); }

void CatService::setBackend(Backend b)        { m_backend = b; }
void CatService::setRigctldHost(const QString& host, quint16 port)
{ m_host = host; m_port = port; }
void CatService::setOmniRigIndex(int n)       { m_rigNo = n; }
bool CatService::isConnected() const          { return m_connected; }
RigState CatService::state() const            { return m_state; }

// ---------------------------------------------------------------------------
// Connect / Disconnect
// ---------------------------------------------------------------------------
void CatService::connectRig()
{
    if (m_connected) return;

#ifdef HAVE_AXCONTAINER
    if (m_backend == OmniRig) {
        omnirigConnect();
        return;
    }
#endif
    // rigctld TCP
    if (!m_sock) {
        m_sock = new QTcpSocket(this);
        connect(m_sock, &QTcpSocket::errorOccurred, this,
                [this](){ emit error(m_sock->errorString()); });
    }
    m_sock->connectToHost(m_host, m_port);
    if (!m_sock->waitForConnected(3000)) {
        emit error("rigctld: " + m_sock->errorString());
        return;
    }
    m_connected = true;
    m_timer->start();
    emit connected();
}

void CatService::disconnectRig()
{
    m_timer->stop();
#ifdef HAVE_AXCONTAINER
    if (m_backend == OmniRig) {
        omnirigDisconnect();
        return;
    }
#endif
    if (m_sock) {
        m_sock->disconnectFromHost();
        m_sock->waitForDisconnected(1000);
    }
    m_connected = false;
    emit disconnected();
}

// ---------------------------------------------------------------------------
// rigctld helpers
// ---------------------------------------------------------------------------
QByteArray CatService::rigctldCommand(const QByteArray& cmd)
{
    if (!m_sock || m_sock->state() != QAbstractSocket::ConnectedState) return {};
    m_sock->write(cmd + "\n");
    m_sock->waitForBytesWritten(1000);
    m_sock->waitForReadyRead(1000);
    return m_sock->readAll().trimmed();
}

// ---------------------------------------------------------------------------
// Poll
// ---------------------------------------------------------------------------
void CatService::poll()
{
#ifdef HAVE_AXCONTAINER
    if (m_backend == OmniRig) {
        RigState s = omnirigPoll();
        if (s.valid && (s.freq != m_state.freq || s.mode != m_state.mode)) {
            m_state = s;
            emit stateChanged(m_state);
        }
        return;
    }
#endif

    // +f (freq) + +m (mode)
    QByteArray freqResp = rigctldCommand("+f");
    QByteArray modeResp = rigctldCommand("+m");

    bool ok;
    double freq = freqResp.toDouble(&ok);
    if (ok && freq > 0) m_state.freq = freq;

    if (!modeResp.isEmpty()) {
        QList<QByteArray> parts = modeResp.split('\n');
        if (!parts.isEmpty()) m_state.mode = QString::fromLatin1(parts[0]);
        if (parts.size() > 1) m_state.submode = QString::fromLatin1(parts[1]);
    }

    m_state.valid = ok;
    emit stateChanged(m_state);
}

// ---------------------------------------------------------------------------
// Set freq / mode / PTT
// ---------------------------------------------------------------------------
void CatService::setFreq(double hz)
{
#ifdef HAVE_AXCONTAINER
    if (m_backend == OmniRig) { omnirigSetFreq(hz); return; }
#endif
    rigctldCommand(QByteArray("F ") + QByteArray::number(static_cast<qint64>(hz)));
}

void CatService::setMode(const QString& mode, const QString& submode)
{
#ifdef HAVE_AXCONTAINER
    if (m_backend == OmniRig) { omnirigSetMode(mode); return; }
#endif
    QString s = submode.isEmpty() ? mode : mode + " " + submode;
    rigctldCommand(("M " + s).toLatin1());
}

void CatService::setPtt(bool on)
{
#ifdef HAVE_AXCONTAINER
    if (m_backend == OmniRig) {
        if (m_rig)
            m_rig->setProperty("Ptt", on ? 1 : 0);
        return;
    }
#endif
    rigctldCommand(on ? "T 1" : "T 0");
}

// ---------------------------------------------------------------------------
// OmniRig (Windows only)
// ---------------------------------------------------------------------------
#ifdef HAVE_AXCONTAINER
void CatService::omnirigConnect()
{
    m_omniRig = new QAxObject("OmniRig.OmniRigX", this);
    if (!m_omniRig || m_omniRig->isNull()) {
        emit error("OmniRig COM object not found — is OmniRig installed?");
        delete m_omniRig; m_omniRig = nullptr;
        return;
    }
    QString propName = m_rigNo == 2 ? "Rig2" : "Rig1";
    m_rig = m_omniRig->querySubObject(propName.toLatin1());
    if (!m_rig) {
        emit error("OmniRig: cannot get " + propName);
        return;
    }
    m_connected = true;
    m_timer->start();
    emit connected();
}

void CatService::omnirigDisconnect()
{
    delete m_rig;    m_rig    = nullptr;
    delete m_omniRig; m_omniRig = nullptr;
    m_connected = false;
    emit disconnected();
}

RigState CatService::omnirigPoll()
{
    RigState s;
    if (!m_rig) return s;
    s.freq  = m_rig->property("Freq").toDouble();
    int modeCode = m_rig->property("Mode").toInt();
    // OmniRig mode codes (partial):
    static QHash<int,QString> modes = {
        {1,"SSB"},{2,"CW"},{4,"AM"},{8,"FM"},{16,"RTTY"},{32,"USB"},
        {64,"LSB"},{128,"CW"},{256,"FSK"},{512,"PSK31"}
    };
    s.mode  = modes.value(modeCode, "SSB");
    s.valid = s.freq > 0;
    return s;
}

void CatService::omnirigSetFreq(double hz)
{
    if (m_rig) m_rig->setProperty("Freq", hz);
}

void CatService::omnirigSetMode(const QString& mode)
{
    if (!m_rig) return;
    static QHash<QString,int> codes = {
        {"USB",32},{"LSB",64},{"CW",2},{"AM",4},{"FM",8},
        {"RTTY",16},{"PSK31",512}
    };
    int code = codes.value(mode.toUpper(), 32);
    m_rig->setProperty("Mode", code);
}
#endif
