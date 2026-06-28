#include "cluster_view.h"
#include "../database.h"
#include "../dxcc.h"
#include <QTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QTcpSocket>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QScrollBar>
#include <QSettings>
#include <QColor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QDebug>

static constexpr int MAX_CONSOLE_LINES = 1000;
static constexpr int TRIM_AT           = 1100;

ClusterView::ClusterView(QWidget* parent) : QWidget(parent)
{
    build();

    m_socket   = new QTcpSocket(this);
    m_keepAlive= new QTimer(this);
    m_keepAlive->setInterval(120000); // 2 min ping

    connect(m_socket, &QTcpSocket::readyRead,    this, &ClusterView::onReadyRead);
    connect(m_socket, &QTcpSocket::connected,    this, &ClusterView::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClusterView::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,this, &ClusterView::onSocketError);
    connect(m_keepAlive, &QTimer::timeout, this, &ClusterView::onKeepAlive);

    // Auto-set porta quando si cambia server
    connect(m_serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        QVariantList meta = m_serverCombo->itemData(idx).toList();
        if (meta.size() >= 2)
            m_port->setText(QString::number(meta[1].toInt()));
    });

    // Load saved settings
    QSettings cfg;
    QString savedHost = cfg.value("cluster/host", "dxc.nc7j.com").toString();
    bool found = false;
    for (int i = 0; i < m_serverCombo->count(); ++i) {
        QVariantList meta = m_serverCombo->itemData(i).toList();
        if (!meta.isEmpty() && meta[0].toString() == savedHost) {
            m_serverCombo->setCurrentIndex(i);
            found = true;
            break;
        }
    }
    if (!found) { m_serverCombo->setCurrentIndex(-1); m_serverCombo->setEditText(savedHost); }
    m_port->setText(cfg.value("cluster/port","7373").toString());
    m_callsign->setText(cfg.value("station/callsign","N0CALL").toString());
}

ClusterView::~ClusterView()
{
    if (m_socket->isOpen()) m_socket->disconnectFromHost();
}

void ClusterView::build()
{
    auto* vl = new QVBoxLayout(this);
    vl->setSpacing(2);

    // Connection bar
    auto* top = new QHBoxLayout;

    // Predefined server list (editable)
    m_serverCombo = new QComboBox(this);
    m_serverCombo->setEditable(true);
    m_serverCombo->setMinimumWidth(180);
    struct { const char* label; const char* host; int port; } servers[] = {
        // Italia
        {"IZ5GST – Firenze (ITA)",   "dxc.iz5gst.it",          7373},
        {"IK4ZHH – Bologna (ITA)",   "cluster.ik4zhh.it",       7300},
        {"IW3SQY – Padova (ITA)",    "dxc.iw3sqy.it",           7373},
        {"IR4M – Emilia (ITA)",      "dxc.ir4m.it",             7373},
        {"IK5PWQ – Toscana (ITA)",   "dxcluster.ik5pwq.it",     7373},
        // Europa
        {"GB7DJK – UK (EU)",         "gb7djk.gb7djk.ampr.org",  7373},
        {"DL5MBY – Germania (EU)",   "db0sue.de",               7373},
        {"ON0AR – Belgio (EU)",      "on0ar.be",                7373},
        {"EA5HT – Spagna (EU)",      "ea5ht.ure.es",            7373},
        {"OZ5DX – Danimarca (EU)",   "oz5dx.oz5dx.ampr.org",    7373},
        // Mondo
        {"dxc.nc7j.com (NA)",        "dxc.nc7j.com",            7373},
        {"w3lpl.net (NA)",           "w3lpl.net",               7373},
        {"vk2io.com (OC)",           "vk2io.com",               7373},
        {"dxc.dx.to (WW)",           "dxc.dx.to",               7373},
        {"hamqth.com (WW)",          "hamqth.com",              7373},
    };
    for (auto& s : servers) {
        QVariantList meta = {QString(s.host), s.port};
        m_serverCombo->addItem(QString(s.label), meta);
    }

    m_port     = new QLineEdit("7373", this);
    m_port->setMaximumWidth(55);
    m_callsign = new QLineEdit(this);
    m_callsign->setMaximumWidth(100);
    m_btnConn  = new QPushButton(tr("Connetti"), this);
    m_bandFilter = new QComboBox(this);
    m_bandFilter->addItem(tr("Tutte"), QString());
    for (const QString& b : {"160m","80m","40m","30m","20m","17m","15m","12m","10m","6m","2m"})
        m_bandFilter->addItem(b, b);
    m_statusLbl = new QLabel(tr("Disconnesso"), this);
    m_statusLbl->setMinimumWidth(120);

    top->addWidget(new QLabel(tr("Server:")));
    top->addWidget(m_serverCombo, 2);
    top->addWidget(new QLabel(tr("Porta:")));
    top->addWidget(m_port);
    top->addWidget(new QLabel(tr("Call:")));
    top->addWidget(m_callsign);
    top->addWidget(m_btnConn);
    top->addWidget(new QLabel(tr("Banda:")));
    top->addWidget(m_bandFilter);
    top->addStretch();
    top->addWidget(m_statusLbl);
    vl->addLayout(top);

    // Spot table
    m_spotTable = new QTableWidget(0, 7, this);
    m_spotTable->setHorizontalHeaderLabels(
        {tr("Ora"), tr("kHz"), tr("Nominativo"), tr("Banda"), tr("Modo"), tr("Commento"), tr("Spotter")});
    m_spotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_spotTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_spotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_spotTable->setSortingEnabled(false);
    m_spotTable->verticalHeader()->hide();
    m_spotTable->setAlternatingRowColors(true);
    m_spotTable->setWordWrap(false);
    m_spotTable->setColumnWidth(0, 50);
    m_spotTable->setColumnWidth(1, 80);
    m_spotTable->setColumnWidth(2, 90);
    m_spotTable->setColumnWidth(3, 50);
    m_spotTable->setColumnWidth(4, 55);
    m_spotTable->setColumnWidth(5, 200);
    m_spotTable->horizontalHeader()->setStretchLastSection(true);
    m_spotTable->verticalHeader()->setDefaultSectionSize(18);

    // Raw console (server messages only)
    m_console = new QTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setLineWrapMode(QTextEdit::NoWrap);
    m_console->setMaximumHeight(100);
    QFont mono("Monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(8);
    m_console->setFont(mono);

    auto* innerSplit = new QSplitter(Qt::Vertical, this);
    innerSplit->addWidget(m_spotTable);
    innerSplit->addWidget(m_console);
    innerSplit->setStretchFactor(0, 4);
    innerSplit->setStretchFactor(1, 1);
    vl->addWidget(innerSplit, 1);

    connect(m_spotTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* item){
        int row = item->row();
        QString call    = m_spotTable->item(row, 2)->text();
        double  freqKhz = m_spotTable->item(row, 1)->text().toDouble();
        QString mode    = m_spotTable->item(row, 4)->text();
        emit spotClicked(call, freqKhz / 1000.0, mode);  // kHz → MHz
    });

    // Input line
    auto* bot = new QHBoxLayout;
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Comando…"));
    auto* btnSend = new QPushButton(tr("Invia"), this);
    bot->addWidget(m_input, 1);
    bot->addWidget(btnSend);
    vl->addLayout(bot);

    connect(m_btnConn, &QPushButton::clicked, this, [this]{
        if (!m_socket || m_socket->state() == QAbstractSocket::UnconnectedState)
            onConnect();
        else
            onDisconnect();
    });
    connect(btnSend, &QPushButton::clicked, this, &ClusterView::onSend);
    connect(m_input, &QLineEdit::returnPressed, this, &ClusterView::onSend);
    connect(m_console, &QTextEdit::cursorPositionChanged, this, [this]{
        Qso q;
        // spot click detection via link handling would go here
    });
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------
void ClusterView::onConnect()
{
    // Resolve host: prefer user data (predefined), fallback to edit text
    QVariantList _meta = m_serverCombo->currentData().toList();
    QString host = _meta.isEmpty() ? QString() : _meta[0].toString();
    if (host.isEmpty()) host = m_serverCombo->currentText().trimmed();
    quint16 port = m_port->text().toUShort();
    if (host.isEmpty()) { consoleAppend(tr("*** Inserire host")); return; }
    if (port == 0) port = 7373;

    QSettings cfg;
    cfg.setValue("cluster/host", host);
    cfg.setValue("cluster/port", port);

    // Abort stale connection before reconnecting
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
        m_rxBuf.clear();
    }

    m_statusLbl->setText(tr("Connessione a %1:%2…").arg(host).arg(port));
    consoleAppend(tr("*** Connessione a %1:%2…").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void ClusterView::onDisconnect()
{
    m_keepAlive->stop();
    m_socket->disconnectFromHost();
}

void ClusterView::onSocketConnected()
{
    m_btnConn->setText(tr("Disconnetti"));
    QVariantList _meta = m_serverCombo->currentData().toList();
    QString host = _meta.isEmpty() ? QString() : _meta[0].toString();
    if (host.isEmpty()) host = m_serverCombo->currentText().trimmed();
    m_statusLbl->setText(tr("Connesso – %1").arg(host));
    consoleAppend(tr("*** Connesso a %1").arg(host));
    m_keepAlive->start();

    // Send callsign (login)
    QTimer::singleShot(500, this, [this]{
        QString cs = m_callsign->text().trimmed();
        if (!cs.isEmpty())
            m_socket->write((cs + "\r\n").toLatin1());
    });
}

void ClusterView::onSocketDisconnected()
{
    m_btnConn->setText(tr("Connetti"));
    m_statusLbl->setText(tr("Disconnesso"));
    consoleAppend(tr("*** Disconnesso"));
    m_keepAlive->stop();
    m_rxBuf.clear();
}

void ClusterView::onSocketError()
{
    QString err = m_socket->errorString();
    consoleAppend(tr("*** Errore: %1").arg(err));
    m_statusLbl->setText(tr("Errore: %1").arg(err));
    m_btnConn->setText(tr("Connetti"));
}

void ClusterView::onKeepAlive()
{
    if (m_socket->isOpen())
        m_socket->write("\r\n");
}

// ---------------------------------------------------------------------------
// Read / Parse
// ---------------------------------------------------------------------------
void ClusterView::onReadyRead()
{
    m_rxBuf += m_socket->readAll();

    while (m_rxBuf.contains('\n')) {
        int nl = m_rxBuf.indexOf('\n');
        QString line = QString::fromLatin1(m_rxBuf.left(nl)).trimmed();
        m_rxBuf = m_rxBuf.mid(nl + 1);
        if (line.isEmpty()) continue;

        if (line.startsWith("DX de "))
            parseSpot(line);
        else
            consoleAppend(line);
    }
}

void ClusterView::parseSpot(const QString& line)
{
    // Format: DX de SP5ABC:     14025.0  IW8XOU        comment           1234Z
    // "DX de " = 6, spotter to colon, then freq, callsign, comment, time
    Spot spot;
    QString s = line.mid(6);  // after "DX de "
    int colon = s.indexOf(':');
    if (colon < 0) return;
    spot.spotter = s.left(colon).trimmed();

    QString rest = s.mid(colon + 1).trimmed();
    // First token: freq
    QStringList tokens = rest.simplified().split(' ', Qt::SkipEmptyParts);
    if (tokens.size() < 2) return;

    spot.freq     = tokens[0].toDouble();
    spot.callsign = tokens[1].toUpper();
    spot.time     = QDateTime::currentDateTimeUtc();

    // Comment: everything between callsign and the trailing HHMMz
    if (tokens.size() > 2) {
        QString last = tokens.last();
        if (last.endsWith('z') || last.endsWith('Z')) {
            QStringList parts = tokens.mid(2, tokens.size() - 3);
            spot.comment = parts.join(' ').trimmed();
        } else {
            spot.comment = tokens.mid(2).join(' ').trimmed();
        }
    }

    spot.band = Qso::freqToBand(spot.freq / 1000.0);
    // Detect mode from comment
    for (const QString& m : {"FT8","FT4","JS8","CW","RTTY","SSB","PSK"})
        if (spot.comment.toUpper().contains(m)) { spot.mode = m; break; }

    m_spots.prepend(spot);
    if (m_spots.size() > 500) m_spots.removeLast();

    // Notifica se stazione mai lavorata (cooldown 5 min per evitare beep continui)
    if (Database::instance().searchQsos(spot.callsign).isEmpty()) {
        QDateTime& last = m_alertedSpots[spot.callsign];
        if (!last.isValid() || last.secsTo(QDateTime::currentDateTime()) > 300) {
            last = QDateTime::currentDateTime();
            emit newNeededSpot(spot.callsign, spot.freq / 1000.0, spot.band, spot.mode);
        }
    }

    appendSpot(spot);
}

void ClusterView::appendSpot(const Spot& spot)
{
    // Band filter
    QString bandF = m_bandFilter->currentData().toString();
    if (!bandF.isEmpty() && spot.band != bandF) return;

    QColor color = spotColor(spot);
    bool highlight = !m_highlight.isEmpty() &&
                     spot.callsign.contains(m_highlight, Qt::CaseInsensitive);

    // Insert at top of table
    m_spotTable->setSortingEnabled(false);
    m_spotTable->insertRow(0);

    // Background per stato: nuovo=rosa, lavorato=azzurro, confermato=verde pallido
    QColor bgColor;
    if      (color == QColor(0x00, 0xa0, 0x00)) bgColor = QColor(0xe8, 0xff, 0xe8); // confermato
    else if (color == QColor(0x00, 0x60, 0xff)) bgColor = QColor(0xe8, 0xf0, 0xff); // lavorato
    else                                         bgColor = QColor(0xff, 0xec, 0xec); // nuovo

    auto makeItem = [&](const QString& txt) {
        auto* it = new QTableWidgetItem(txt);
        it->setForeground(color);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        if (highlight) {
            it->setBackground(QColor(0xff, 0xff, 0x00));
            QFont f = it->font(); f.setBold(true); it->setFont(f);
        } else {
            it->setBackground(bgColor);
        }
        return it;
    };
    m_spotTable->setItem(0, 0, makeItem(spot.time.toString("HH:mm")));
    m_spotTable->setItem(0, 1, makeItem(QString::number(spot.freq, 'f', 1)));
    m_spotTable->setItem(0, 2, makeItem(spot.callsign));
    m_spotTable->setItem(0, 3, makeItem(spot.band));
    m_spotTable->setItem(0, 4, makeItem(spot.mode));
    m_spotTable->setItem(0, 5, makeItem(spot.comment.left(60)));
    m_spotTable->setItem(0, 6, makeItem(spot.spotter));

    // Keep max 500 rows
    while (m_spotTable->rowCount() > 500)
        m_spotTable->removeRow(m_spotTable->rowCount() - 1);
}

void ClusterView::consoleAppend(const QString& line)
{
    QTextCursor cursor(m_console->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(0x80, 0x80, 0x80));
    cursor.insertText(line + "\n", fmt);

    if (m_console->document()->blockCount() > TRIM_AT) {
        QTextCursor trimmer(m_console->document());
        trimmer.movePosition(QTextCursor::Start);
        trimmer.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                             m_console->document()->blockCount() - MAX_CONSOLE_LINES);
        trimmer.removeSelectedText();
    }

    m_console->verticalScrollBar()->setValue(
        m_console->verticalScrollBar()->maximum());
}

QColor ClusterView::spotColor(const Spot& spot) const
{
    // Check if worked before
    auto qsos = Database::instance().searchQsos(spot.callsign);
    bool worked    = !qsos.isEmpty();
    bool confirmed = false;
    for (const Qso& q : qsos) {
        if (q.lotwRcvd == "Y" || q.eqslRcvd == "Y" || q.qslRcvd == "Y") {
            confirmed = true;
            break;
        }
    }

    if (confirmed) return QColor(0x00, 0xa0, 0x00);   // green: confirmed
    if (worked)    return QColor(0x00, 0x60, 0xff);   // blue: worked
    return QColor(0xc0, 0x00, 0x00);                  // red: new
}

void ClusterView::onSend()
{
    QString cmd = m_input->text().trimmed();
    if (cmd.isEmpty()) return;
    m_socket->write((cmd + "\r\n").toLatin1());
    consoleAppend("> " + cmd);
    m_input->clear();
}

void ClusterView::highlightCall(const QString& callsign)
{
    m_highlight = callsign.toUpper();
}
