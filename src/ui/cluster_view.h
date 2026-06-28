#pragma once
#include <QWidget>
#include <QList>
#include <QHash>
#include <QDateTime>

class QTextEdit;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QTcpSocket;
class QTimer;

struct Spot {
    QString   spotter;
    double    freq;
    QString   callsign;
    QString   comment;
    QDateTime time;
    QString   band;
    QString   mode;
};

class ClusterView : public QWidget
{
    Q_OBJECT
public:
    explicit ClusterView(QWidget* parent = nullptr);
    ~ClusterView() override;

public slots:
    void highlightCall(const QString& callsign);

signals:
    void spotClicked(const QString& callsign, double freqMhz, const QString& mode);
    void newNeededSpot(const QString& callsign, double freqMhz, const QString& band, const QString& mode);

private slots:
    void onConnect();
    void onDisconnect();
    void onSend();
    void onReadyRead();
    void onSocketError();
    void onSocketConnected();
    void onSocketDisconnected();
    void onKeepAlive();

private:
    void build();
    void parseSpot(const QString& line);
    void appendSpot(const Spot& spot);
    void consoleAppend(const QString& line);
    QColor spotColor(const Spot& spot) const;

    QTableWidget* m_spotTable = nullptr;
    QTextEdit*    m_console   = nullptr;
    QComboBox*   m_serverCombo = nullptr;
    QLineEdit*   m_port     = nullptr;
    QLineEdit*   m_callsign = nullptr;
    QLineEdit*   m_input    = nullptr;
    QPushButton* m_btnConn  = nullptr;
    QComboBox*   m_bandFilter = nullptr;
    QLabel*      m_statusLbl  = nullptr;

    QTcpSocket*  m_socket   = nullptr;
    QTimer*      m_keepAlive= nullptr;

    QString      m_highlight;
    QList<Spot>  m_spots;
    QByteArray   m_rxBuf;
    QHash<QString, QDateTime> m_alertedSpots;
};
