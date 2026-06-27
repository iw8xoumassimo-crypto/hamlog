#include "services.h"
#include "database.h"
#include "adif.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QUrlQuery>
#include <QUrl>
#include <QXmlStreamReader>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QDebug>

// ---------------------------------------------------------------------------
// OnlineService base
// ---------------------------------------------------------------------------
OnlineService::OnlineService(QObject* parent) : QObject(parent) {}

QNetworkAccessManager* OnlineService::nam()
{
    if (!m_nam) m_nam = new QNetworkAccessManager(this);
    return m_nam;
}

QByteArray OnlineService::syncGet(const QString& url, int timeoutMs, int* httpStatusOut,
                                   const QHash<QString,QString>& extraHeaders)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) HamLog/1.0");
    for (auto it = extraHeaders.begin(); it != extraHeaders.end(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    QNetworkReply* reply = nam()->get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatusOut) *httpStatusOut = status;

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError)
        data = reply->readAll();
    else
        qWarning() << "syncGet" << url << reply->errorString() << "HTTP" << status;
    reply->deleteLater();
    return data;
}

QByteArray OnlineService::syncPost(const QString& url, const QByteArray& data,
                                   const QString& contentType, int timeoutMs)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    req.setHeader(QNetworkRequest::UserAgentHeader,   "HamLog/1.0");
    QNetworkReply* reply = nam()->post(req, data);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    QByteArray resp;
    if (reply->error() == QNetworkReply::NoError)
        resp = reply->readAll();
    else
        qWarning() << "syncPost" << url << reply->errorString();
    reply->deleteLater();
    return resp;
}

// ---------------------------------------------------------------------------
// LoTW
// ---------------------------------------------------------------------------
LotwService::LotwService(QObject* parent) : OnlineService(parent) {}

void LotwService::upload(const QString& adifFile, const QString& station,
                         const QString& tqslPath)
{
    // Batch upload via TQSL 2.x:
    //   -u  upload to LoTW without asking for confirmation
    //   -d  suppress the "QSO date range" dialog (process all dates)
    //   -x  allow QSOs outside the certificate validity period
    //   -l  station location name as configured in TQSL
    // Note: -a and -q are NOT used — they are rejected by some TQSL 2.8 builds.
    QStringList args;
    args << "-u" << "-d" << "-x" << "-l" << station << adifFile;

    emit progress(tr("TQSL batch upload…"));
    emit progress(tr("cmd: \"%1\" %2").arg(tqslPath, args.join(' ')));

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    QString allOut;
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, &allOut] {
        while (proc->canReadLine()) {
            QString line = QString::fromLocal8Bit(proc->readLine()).trimmed();
            if (!line.isEmpty()) {
                emit progress(tr("TQSL: ") + line);
                allOut += line + "\n";
            }
        }
    });

    QEventLoop loop;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            &loop, &QEventLoop::quit);

    proc->start(tqslPath, args);
    if (!proc->waitForStarted(5000)) {
        emit uploadFinished(false, tr("tqsl non avviato: ") + tqslPath);
        proc->deleteLater();
        return;
    }
    loop.exec();

    while (proc->canReadLine()) {
        QString line = QString::fromLocal8Bit(proc->readLine()).trimmed();
        if (!line.isEmpty()) { emit progress(tr("TQSL: ") + line); allOut += line + "\n"; }
    }
    QByteArray rem = proc->readAll();
    if (!rem.isEmpty()) {
        QString s = QString::fromLocal8Bit(rem).trimmed();
        if (!s.isEmpty()) { emit progress(tr("TQSL: ") + s); allOut += s + "\n"; }
    }

    int code = proc->exitCode();
    proc->deleteLater();

    bool ok = (code == 0);
    QString summary = allOut.trimmed().isEmpty()
        ? (ok ? tr("Upload completato (codice 0)") : tr("TQSL errore, codice %1").arg(code))
        : allOut.trimmed();
    emit uploadFinished(ok, summary);
}

void LotwService::fetchQsls(const QString& username, const QString& password,
                             const QString& since)
{
    // LoTW ADIF download URL
    QUrlQuery params;
    params.addQueryItem("login",    username);
    params.addQueryItem("password", password);
    params.addQueryItem("qso_query","1");
    params.addQueryItem("qso_qsl",  "yes");
    if (!since.isEmpty())
        params.addQueryItem("qso_startdate", since);  // YYYY-MM-DD

    QString url = "https://lotw.arrl.org/lotwuser/lotwreport.adi?" + params.toString();
    emit progress("Downloading LoTW QSLs…");
    QByteArray data = syncGet(url, 60000);

    if (data.isEmpty()) {
        emit finished(false, "No response from LoTW");
        return;
    }
    if (data.startsWith("<!-- Error")) {
        emit finished(false, "LoTW login failed");
        return;
    }

    QList<Qso> confirmed;
    // Write to temp file and use AdifHandler
    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    if (tmp.open()) {
        tmp.write(data);
        tmp.flush();
        confirmed = AdifHandler::importAdif(tmp.fileName());
    }
    emit qslsReceived(confirmed);
    emit finished(true, QString("Received %1 QSLs from LoTW").arg(confirmed.size()));
}

// ---------------------------------------------------------------------------
// eQSL
// ---------------------------------------------------------------------------
EqslService::EqslService(QObject* parent) : OnlineService(parent) {}

void EqslService::uploadAdif(const QString& username, const QString& password,
                              const QString& adifData)
{
    if (adifData.trimmed().isEmpty()) {
        emit uploadFinished(false, 0, tr("Nessun QSO da inviare."));
        return;
    }

    // Build application/x-www-form-urlencoded body manually so that
    // ADIF angle brackets and newlines are correctly percent-encoded.
    auto enc = [](const QString& s) -> QByteArray {
        return QUrl::toPercentEncoding(s);
    };
    QByteArray body =
        "eQSLUser="    + enc(username) +
        "&eQSLPass="   + enc(password) +
        "&QTHNickname=" +
        "&ADIFData="   + enc(adifData);

    emit progress(tr("Invio a eQSL in corso…"));
    QByteArray resp = syncPost("https://www.eqsl.cc/qslcard/ImportADIF.cfm",
                               body, "application/x-www-form-urlencoded", 60000);

    QString text = QString::fromUtf8(resp).trimmed();

    // Strip HTML tags to get a readable error message
    if (text.contains('<')) {
        QString plain = text;
        plain.remove(QRegularExpression("<[^>]*>"));
        plain = plain.simplified();
        // Try to extract only the meaningful error line
        for (const QString& line : plain.split('\n')) {
            QString t = line.trimmed();
            if (!t.isEmpty() && !t.startsWith("<!--")) { text = t; break; }
        }
    }

    bool ok = resp.contains("accepted");
    int count = 0;
    QRegularExpression re("(\\d+)\\s*record");
    auto m = re.match(QString::fromUtf8(resp));
    if (m.hasMatch()) count = m.captured(1).toInt();

    emit uploadFinished(ok, count, text);
}

void EqslService::fetchInbox(const QString& username, const QString& password,
                              const QString& since)
{
    QUrlQuery params;
    params.addQueryItem("UserName",     username);
    params.addQueryItem("Password",     password);
    params.addQueryItem("RcvdSince",    since.isEmpty() ? "1990-01-01" : since);
    params.addQueryItem("DownloadType", "Complete");

    emit progress(tr("Contatto eQSL.cc…"));
    QByteArray data = syncGet("https://www.eqsl.cc/qslcard/DownloadInBox.cfm?" +
                              params.toString(QUrl::FullyEncoded), 60000);

    if (data.isEmpty()) {
        emit finished(false, tr("Nessuna risposta da eQSL (timeout o errore di rete)"));
        return;
    }

    // eQSL ha cambiato API: restituisce una pagina HTML con un link al file ADIF
    // invece del file ADIF direttamente.
    if (data.contains("<html") || data.contains("<HTML")) {
        // Cerca il link al file .adi nella risposta HTML
        // Pattern: href="../downloadedfiles/FILENAME.adi"
        QRegularExpression re(
            R"(href\s*=\s*["']([^"']*downloadedfiles/[^"']+\.adi)["'])",
            QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(QString::fromUtf8(data));
        if (!m.hasMatch()) {
            // Nessun link trovato — probabilmente login fallito
            // Cerca il messaggio di errore nella pagina
            QRegularExpression errRe(R"(Invalid\s+Password|not\s+found|incorrect)",
                                     QRegularExpression::CaseInsensitiveOption);
            if (errRe.match(QString::fromUtf8(data)).hasMatch())
                emit finished(false, tr("Login eQSL fallito — controllare nominativo e password"));
            else
                emit finished(false, tr("eQSL: impossibile trovare il link ADIF nella risposta"));
            return;
        }

        // Risolvi URL relativo → assoluto
        QString relativePath = m.captured(1);
        // "../downloadedfiles/FILE.adi" → "https://www.eqsl.cc/downloadedfiles/FILE.adi"
        relativePath.remove(QRegularExpression(R"(^\.\./)"));
        QString adifUrl = "https://www.eqsl.cc/" + relativePath;

        emit progress(tr("Download ADIF eQSL (%1)…").arg(adifUrl.section('/', -1)));
        data = syncGet(adifUrl, 120000);   // file grosso — 2 min timeout

        if (data.isEmpty()) {
            emit finished(false, tr("Download file ADIF eQSL fallito"));
            return;
        }
    }

    QList<Qso> qsls;
    QTemporaryFile tmp;
    if (tmp.open()) {
        tmp.write(data);
        tmp.flush();
        qsls = AdifHandler::importAdif(tmp.fileName());
    }
    emit inboxReceived(qsls);
    emit finished(true, tr("eQSL: ricevuti %1 QSO").arg(qsls.size()));
}

QString EqslService::bandToEqslMhz(const QString& adifBand)
{
    // eQSL GetImage.cfm expects "14MHz", "7MHz", etc.
    static const QHash<QString,QString> map = {
        {"160m","1.8MHz"}, {"80m","3.5MHz"}, {"60m","5MHz"},
        {"40m","7MHz"},    {"30m","10MHz"},  {"20m","14MHz"},
        {"17m","18MHz"},   {"15m","21MHz"},  {"12m","24MHz"},
        {"10m","28MHz"},   {"6m","50MHz"},   {"4m","70MHz"},
        {"2m","144MHz"},   {"70cm","430MHz"},{"23cm","1240MHz"}
    };
    return map.value(adifBand.toLower(), adifBand);
}

bool EqslService::loginSession(const QString& username, const QString& password)
{
    if (!nam()->cookieJar() || nam()->cookieJar()->parent() != nam())
        nam()->setCookieJar(new QNetworkCookieJar(nam()));

    // Fetch Login.cfm to discover the form action and field names
    static const QHash<QString,QString> fHdr = { {"Accept", "text/html,*/*"} };
    QByteArray loginPage = syncGet("https://www.eqsl.cc/QSLCard/Login.cfm", 20000, nullptr, fHdr);
    QString loginHtml = QString::fromUtf8(loginPage);

    QRegularExpression actionRe(R"(<form[^>]+action=["\']([^"\']+)["\'])",
                                 QRegularExpression::CaseInsensitiveOption);
    auto am2 = actionRe.match(loginHtml);
    QString formAction = am2.hasMatch() ? am2.captured(1) : "/QSLCard/Login.cfm";
    if (formAction.startsWith("/"))       formAction = "https://www.eqsl.cc" + formAction;
    else if (!formAction.startsWith("http")) formAction = "https://www.eqsl.cc/QSLCard/" + formAction;

    QUrlQuery formData;
    QString callField = "Callsign", passField = "Password";
    QRegularExpression inputRe(R"(<input([^>]*)>)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression typeRe (R"(type=["\']?(\w+)["\']?)",    QRegularExpression::CaseInsensitiveOption);
    QRegularExpression nameRe (R"(name=["\']([^"\']+)["\'])",  QRegularExpression::CaseInsensitiveOption);
    QRegularExpression valRe  (R"(value=["\']([^"\']*)["\'])", QRegularExpression::CaseInsensitiveOption);
    auto it = inputRe.globalMatch(loginHtml);
    while (it.hasNext()) {
        auto im2 = it.next(); QString a = im2.captured(1);
        auto nm = nameRe.match(a); if (!nm.hasMatch()) continue;
        QString fn = nm.captured(1);
        auto tm = typeRe.match(a);
        if (tm.hasMatch() && tm.captured(1).toLower() == "hidden") {
            auto vm = valRe.match(a);
            formData.addQueryItem(fn, vm.hasMatch() ? vm.captured(1) : "");
        }
        QString al = a.toLower();
        if      (al.contains("call") || al.contains("user")) callField = fn;
        else if (al.contains("pass") || al.contains("pwd"))  passField = fn;
    }
    formData.addQueryItem(callField, username.toUpper());
    formData.addQueryItem(passField, password);

    QNetworkRequest req{QUrl(formAction)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) HamLog/1.0");
    req.setRawHeader("Referer", "https://www.eqsl.cc/QSLCard/Login.cfm");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam()->post(req, formData.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop; QTimer timer; timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(30000); loop.exec();
    int loginStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->readAll(); reply->deleteLater();

    auto cookies = nam()->cookieJar()->cookiesForUrl(QUrl("https://www.eqsl.cc/"));
    if ((loginStatus >= 400) || cookies.isEmpty()) {
        emit progress(tr("Login eQSL fallito — verifica nominativo e password"));
        return false;
    }
    return true;
}

void EqslService::fetchImages(const QString& username, const QString& password,
                               const QList<Qso>& qsls, const QString& saveDir,
                               const QString& myCallsign)
{
    // Login to get session cookie — eQSL requires auth to serve card images
    loginSession(username, password);

    QString myCall = myCallsign.isEmpty() ? username.toUpper() : myCallsign.toUpper();

    QDir().mkpath(saveDir);
    int total      = qsls.size();
    int downloaded = 0;
    int noCard     = 0;

    for (int i = 0; i < total; ++i) {
        if (m_cancel) {
            emit progress(tr("Download annullato dall'utente."));
            break;
        }

        const Qso& q = qsls[i];

        QString dateCompact = QString(q.date).remove('-');  // YYYY-MM-DD → YYYYMMDD
        QString safeCall = QString(q.callsign).replace('/', '_').replace('\\', '_');
        QString fname = saveDir + "/" +
                        safeCall + "_" + dateCompact + "_" +
                        q.band.toLower() + "_" + q.mode.toUpper() + ".jpg";

        if (QFile::exists(fname)) {
            emit imageDownloaded(q.callsign, q.date, q.band, fname);
            ++downloaded;
            emit progressPct((i + 1) * 100 / total);
            continue;
        }
        // Already known to have no custom card — skip without re-fetching
        QString noimgPath = QString(fname).replace(
            QRegularExpression(R"(\.(jpg|png)$)"), ".noimage");
        if (QFile::exists(noimgPath)) {
            ++noCard;
            emit progressPct((i + 1) * 100 / total);
            continue;
        }

        emit progress(tr("Card %1/%2: %3…").arg(i+1).arg(total).arg(q.callsign));

        // Band: eQSL expects "20M", "40M", etc. (uppercase ADIF band name)
        QString bandEqsl = q.band.toUpper();
        if (!bandEqsl.endsWith('M')) bandEqsl += 'M';

        QUrlQuery params;
        params.addQueryItem("Callsign",    myCall);              // us — recipient
        params.addQueryItem("Password",    password);            // required by API
        params.addQueryItem("CallPartner", q.callsign.toUpper()); // DX — card maker
        params.addQueryItem("QSO_Date",    dateCompact);
        params.addQueryItem("Band",        bandEqsl);
        params.addQueryItem("Mode",        q.mode.toUpper());

        QString url = "https://www.eqsl.cc/QSLCard/GetImage.cfm?" +
                      params.toString(QUrl::FullyEncoded);

        static const QHash<QString,QString> imgHeaders = {
            {"Referer", "https://www.eqsl.cc/QSLCard/DownloadInbox.cfm"},
            {"Accept",  "image/jpeg,image/png,*/*"},
        };

        int httpStatus = 0;
        QByteArray img = syncGet(url, 30000, &httpStatus, imgHeaders);


        // GetImage.cfm returns 404 when the station has no custom card uploaded.
        // eQSL's auto-generated cards are not accessible via any public API endpoint.
        if (httpStatus == 404 || img.size() < 50) {
            // Write a .noimage marker so we skip this QSO on future runs
            QString noimgPath = QString(fname).replace(
                QRegularExpression(R"(\.(jpg|png)$)"), ".noimage");
            if (!QFile::exists(noimgPath)) {
                QFile nf(noimgPath); nf.open(QIODevice::WriteOnly);
            }
            ++noCard;
            emit progressPct((i + 1) * 100 / total);
            continue;
        }

        if (img.size() < 50) { ++noCard; continue; }

        // Accept JPEG (FF D8) or PNG (89 50 4E 47)
        bool isJpeg = (static_cast<unsigned char>(img[0]) == 0xFF &&
                       static_cast<unsigned char>(img[1]) == 0xD8);
        bool isPng  = (static_cast<unsigned char>(img[0]) == 0x89 &&
                       img[1] == 'P' && img[2] == 'N' && img[3] == 'G');

        if (isJpeg || isPng) {
            QFile f(fname);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(img);
                f.close();
                ++downloaded;
                emit imageDownloaded(q.callsign, q.date, q.band, fname);
            }
        } else {
            ++noCard;
        }
        emit progressPct((i + 1) * 100 / total);
    }

    emit imagesComplete(downloaded, total);
    if (downloaded == 0)
        emit progress(tr("Nessuna cartolina scaricata: le %1 stazioni confermate usano "
                         "cartoline auto-generate da eQSL (non scaricabili via API).\n"
                         "Le conferme sono comunque registrate nel log (colonna eQSL ✓).")
                      .arg(noCard));
    else
        emit progress(tr("Cartoline: %1 scaricate, %2 senza immagine personalizzata.")
                      .arg(downloaded).arg(noCard));
}

// ---------------------------------------------------------------------------
// ClubLog
// ---------------------------------------------------------------------------
ClublogService::ClublogService(QObject* parent) : OnlineService(parent) {}

void ClublogService::uploadAdif(const QString& email, const QString& password,
                                 const QString& callsign, const QString& adifData)
{
    // ClubLog uses multipart/form-data
    QString boundary = "----HamLogBoundary7MA4YWxkTrZu0gW";

    auto addField = [&](const QString& name, const QString& value) -> QByteArray {
        return "--" + boundary.toUtf8() + "\r\n"
               "Content-Disposition: form-data; name=\"" + name.toUtf8() + "\"\r\n\r\n"
               + value.toUtf8() + "\r\n";
    };

    QByteArray body;
    body += addField("email",    email);
    body += addField("password", password);
    body += addField("callsign", callsign);
    body += addField("mode",     "ADIF");
    body += "--" + boundary.toUtf8() + "\r\n"
            "Content-Disposition: form-data; name=\"adif\"; filename=\"log.adi\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            + adifData.toUtf8() + "\r\n";
    body += "--" + boundary.toUtf8() + "--\r\n";

    emit progress("Uploading to ClubLog…");
    QByteArray resp = syncPost("https://clublog.org/putlogs.php",
                               body,
                               "multipart/form-data; boundary=" + boundary,
                               60000);

    bool ok = resp.toLower().contains("ok") || resp.contains("200");
    emit uploadFinished(ok, QString::fromUtf8(resp).trimmed());
}

// ---------------------------------------------------------------------------
// HamQTH
// ---------------------------------------------------------------------------
HamQthService::HamQthService(QObject* parent) : OnlineService(parent) {}

void HamQthService::setCredentials(const QString& user, const QString& pass)
{
    if (m_user != user || m_pass != pass) {
        m_user = user;
        m_pass = pass;
        m_session.clear();  // reset session only if credentials changed
    }
}

bool HamQthService::login()
{
    if (!m_session.isEmpty()) return true;
    QString url = QString("https://www.hamqth.com/xml.php?u=%1&p=%2")
                  .arg(m_user, m_pass);
    QByteArray data = syncGet(url);
    if (data.isEmpty()) {
        m_lastError = "Nessuna risposta da HamQTH (rete o SSL?)";
        return false;
    }
    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == QLatin1String("session_id")) {
            m_session = xml.readElementText();
            m_lastError.clear();
            return !m_session.isEmpty();
        }
        if (xml.name() == QLatin1String("error")) {
            m_lastError = xml.readElementText();
            return false;
        }
    }
    m_lastError = "Risposta XML inattesa da HamQTH";
    return false;
}

// Shared helper: network fetch + XML parse + key normalization.
// Does NOT check cache and does NOT save to cache — callers handle that.
QHash<QString,QString> HamQthService::fetchAndParse(const QString& callsign)
{
    QString url = QString("https://www.hamqth.com/xml.php?id=%1&callsign=%2&prg=HamLog")
                  .arg(m_session, callsign.toUpper());
    QByteArray data = syncGet(url);

    QHash<QString,QString> result;
    QXmlStreamReader xml(data);
    bool inSearch = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            QString n = xml.name().toString().toLower();
            if (n == "search") { inSearch = true; continue; }
            if (n == "error") {
                qWarning() << "HamQTH lookup error:" << xml.readElementText();
                m_session.clear();
                break;
            }
            if (inSearch) {
                QString val = xml.readElementText();
                if (!val.isEmpty()) result[n] = val;
            }
        }
        if (xml.isEndElement() && xml.name() == QLatin1String("search"))
            break;
    }

    // Normalize HamQTH keys:
    //   name  ← adr_name  (full name)
    //   fname ← nick      (first name / callsign nick)
    //   addr1 ← qth       (city/location)
    if (result.contains("adr_name") && !result["adr_name"].isEmpty())
        result["name"] = result["adr_name"];
    else if (result.contains("nick") && !result["nick"].isEmpty())
        result["name"] = result["nick"];

    if (!result.contains("fname") && result.contains("nick"))
        result["fname"] = result["nick"];

    if (result.contains("qth") && !result["qth"].isEmpty())
        result["addr1"] = result["qth"];
    else if (result.contains("adr_city") && !result["adr_city"].isEmpty())
        result["addr1"] = result["adr_city"];

    return result;
}

void HamQthService::lookup(const QString& callsign)
{
    auto cached = Database::instance().getCallbookCache(callsign);
    if (!cached.isEmpty()) {
        emit lookupResult(callsign, cached, true);
        return;
    }

    if (!login()) {
        QHash<QString,QString> err;
        err["_error"] = m_lastError.isEmpty() ? "Login HamQTH fallito" : m_lastError;
        emit lookupResult(callsign, err, false);
        return;
    }

    QHash<QString,QString> result = fetchAndParse(callsign);
    if (!result.isEmpty())
        Database::instance().saveCallbookCache(callsign, result);
    emit lookupResult(callsign, result, false);
}

QHash<QString,QString> HamQthService::lookupSync(const QString& callsign)
{
    auto cached = Database::instance().getCallbookCache(callsign);
    if (!cached.isEmpty()) return cached;

    if (!login())
        return {{"_error", m_lastError.isEmpty() ? "Login HamQTH fallito" : m_lastError}};

    QHash<QString,QString> result = fetchAndParse(callsign);
    if (!result.isEmpty())
        Database::instance().saveCallbookCache(callsign, result);
    return result;
}

// ---------------------------------------------------------------------------
// HamDB
// ---------------------------------------------------------------------------
HamDbService::HamDbService(QObject* parent) : OnlineService(parent) {}

static QHash<QString,QString> hamDbParseAndNormalize(const QByteArray& data)
{
    auto extract = [&](const QString& key) -> QString {
        QByteArray search = "\"" + key.toUtf8() + "\":\"";
        int i = data.indexOf(search);
        if (i < 0) return {};
        int s = i + search.size();
        int e = data.indexOf('"', s);
        if (e < 0) return {};
        return QString::fromUtf8(data.mid(s, e-s));
    };

    QHash<QString,QString> result;
    for (const QString& k : QStringList{"call","fname","name","addr1","addr2",
                                         "state","zip","country","lat","lon",
                                         "grid","dxcc","class"})
        result[k] = extract(k);

    if (!result["fname"].isEmpty())
        result["nick"] = result["fname"];
    if (!result["name"].isEmpty() && !result["fname"].isEmpty())
        result["name"] = result["fname"] + " " + result["name"];

    return result;
}

void HamDbService::lookup(const QString& callsign)
{
    auto cached = Database::instance().getCallbookCache(callsign);
    if (!cached.isEmpty()) {
        emit lookupResult(callsign, cached);
        return;
    }

    QString url = "https://api.hamdb.org/v1/" + callsign.toUpper() + "/json/HamLog";
    QHash<QString,QString> result = hamDbParseAndNormalize(syncGet(url));

    if (!result.isEmpty())
        Database::instance().saveCallbookCache(callsign, result);
    emit lookupResult(callsign, result);
}

QHash<QString,QString> HamDbService::lookupSync(const QString& callsign)
{
    auto cached = Database::instance().getCallbookCache(callsign);
    if (!cached.isEmpty()) return cached;

    QString url = "https://api.hamdb.org/v1/" + callsign.toUpper() + "/json/HamLog";
    QHash<QString,QString> result = hamDbParseAndNormalize(syncGet(url));

    if (!result.isEmpty())
        Database::instance().saveCallbookCache(callsign, result);
    return result;
}
