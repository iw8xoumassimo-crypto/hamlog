#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include "types.h"

class CatService : public QObject
{
    Q_OBJECT

public:
    enum Backend { Rigctld, OmniRig };
    Q_ENUM(Backend)

    explicit CatService(QObject* parent = nullptr);
    ~CatService() override;

    void setBackend(Backend b);
    void setRigctldHost(const QString& host, quint16 port = 4532);
    void setOmniRigIndex(int rigNumber);   // 1 or 2

    bool    isConnected() const;
    RigState state() const;

public slots:
    void connectRig();
    void disconnectRig();
    void setFreq(double hz);
    void setMode(const QString& mode, const QString& submode = QString());
    void setPtt(bool on);
    void poll();

signals:
    void connected();
    void disconnected();
    void stateChanged(const RigState& state);
    void error(const QString& message);

private:
    // rigctld (TCP)
    QByteArray rigctldCommand(const QByteArray& cmd);

    // OmniRig (COM, Windows only)
#ifdef HAVE_AXCONTAINER
    void omnirigConnect();
    void omnirigDisconnect();
    RigState omnirigPoll();
    void omnirigSetFreq(double hz);
    void omnirigSetMode(const QString& mode);
    class QAxObject* m_omniRig   = nullptr;
    class QAxObject* m_rig       = nullptr;
#endif

    Backend  m_backend   = Rigctld;
    QString  m_host      = "127.0.0.1";
    quint16  m_port      = 4532;
    int      m_rigNo     = 1;
    bool     m_connected = false;
    RigState m_state;
    QTimer*  m_timer     = nullptr;

    class QTcpSocket* m_sock = nullptr;
};
