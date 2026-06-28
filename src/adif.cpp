#include "adif.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>

static QString adifDate(const QString& d) {
    QString s = d.trimmed();
    if (s.size() == 10 && s[4] == '-' && s[7] == '-') return s;
    QString digits = s;
    digits.remove(QRegularExpression("[^0-9]"));
    if (digits.size() == 8)
        return digits.left(4) + "-" + digits.mid(4,2) + "-" + digits.mid(6,2);
    return s;
}

static QString dbDate(const QString& d) {
    QString s = d;
    s.remove('-');
    return s;
}

static QString adifTime(const QString& t) {
    QString digits = t;
    digits.remove(QRegularExpression("[^0-9]"));
    if (digits.size() >= 4) return digits.left(4);
    return digits.leftJustified(4, '0');
}

namespace AdifHandler {

QList<Qso> importAdif(const QString& filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString text = QString::fromUtf8(f.readAll());
    f.close();

    // skip header
    int pos = 0;
    int eoh = text.toUpper().indexOf("<EOH>");
    if (eoh >= 0) pos = eoh + 5;

    QList<Qso> result;
    static QHash<QString,QString> fieldMap = {
        {"CALL","callsign"}, {"QSO_DATE","date"}, {"TIME_ON","time_on"},
        {"TIME_OFF","time_off"}, {"BAND","band"}, {"FREQ","freq"},
        {"MODE","mode"}, {"SUBMODE","submode"},
        {"RST_SENT","rst_sent"}, {"RST_RCVD","rst_rcvd"},
        {"NAME","name"}, {"QTH","qth"}, {"COUNTRY","country"},
        {"DXCC","dxcc_id"}, {"CQZ","cq_zone"}, {"ITUZ","itu_zone"},
        {"CONT","continent"}, {"STATE","state"}, {"GRIDSQUARE","gridsquare"},
        {"TX_PWR","power"}, {"COMMENT","comment"}, {"NOTES","notes"},
        {"IOTA","iota"}, {"SOTA_REF","sota_ref"}, {"POTA_REF","pota_ref"},
        {"CONTEST_ID","contest_id"}, {"SRX","srx"}, {"STX","stx"},
        {"OPERATOR","operator_call"}, {"STATION_CALLSIGN","station_call"},
        {"QSLVIA","manager"}, {"QSL_VIA","manager"},
        {"QSL_SENT","qsl_sent"}, {"QSL_RCVD","qsl_rcvd"},
        {"LOTW_QSL_SENT","lotw_sent"}, {"LOTW_QSL_RCVD","lotw_rcvd"},
        {"EQSL_QSL_SENT","eqsl_sent"}, {"EQSL_QSL_RCVD","eqsl_rcvd"},
    };

    while (pos < text.size()) {
        int open = text.indexOf('<', pos);
        if (open < 0) break;

        QHash<QString,QVariant> rec;
        int cur = open;
        bool gotEor = false;

        while (cur < text.size()) {
            if (text[cur] != '<') { ++cur; continue; }
            int close = text.indexOf('>', cur);
            if (close < 0) break;
            QString tag = text.mid(cur+1, close-cur-1);
            cur = close + 1;
            QStringList parts = tag.split(':');
            QString name = parts[0].toUpper();
            if (name == "EOR") { gotEor = true; break; }
            if (parts.size() < 2) continue;
            int len = parts[1].toInt();
            if (len <= 0) continue;
            if (cur + len > text.size()) break;
            QString val = text.mid(cur, len);
            cur += len;
            QString dbKey = fieldMap.value(name);
            if (!dbKey.isEmpty()) {
                if      (name == "QSO_DATE")                rec[dbKey] = adifDate(val);
                else if (name == "TIME_ON" || name == "TIME_OFF") rec[dbKey] = adifTime(val);
                else if (name == "FREQ")    rec[dbKey] = val.toDouble();
                else                        rec[dbKey] = val;
            }
        }

        pos = cur;
        if (!gotEor || rec.isEmpty()) continue;

        // Derive band from freq if missing
        if (!rec.contains("band") || rec["band"].toString().isEmpty()) {
            double mhz = rec.value("freq").toDouble();
            if (mhz > 0) {
                QString b = Qso::freqToBand(mhz);
                if (!b.isEmpty()) rec["band"] = b;
            }
        }

        // Normalize band to lowercase (Decodium uses "10M", "40M" etc.)
        if (rec.contains("band"))
            rec["band"] = rec["band"].toString().toLower();

        Qso q = Qso::fromMap(rec);
        if (q.isValid()) {
            if (q.rstSent.isEmpty()) q.rstSent = "59";
            if (q.rstRcvd.isEmpty()) q.rstRcvd = "59";
            if (q.qslSent.isEmpty()) q.qslSent = "N";
            if (q.qslRcvd.isEmpty()) q.qslRcvd = "N";
            if (q.lotwSent.isEmpty()) q.lotwSent = "N";
            if (q.lotwRcvd.isEmpty()) q.lotwRcvd = "N";
            if (q.eqslSent.isEmpty()) q.eqslSent = "N";
            if (q.eqslRcvd.isEmpty()) q.eqslRcvd = "N";
            result.append(q);
        }
    }
    return result;
}

int exportAdif(const QString& filename, const QList<Qso>& qsos,
               const QString& myCall, const QString& program)
{
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return -1;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "ADIF export from " << program << "\n";
    out << "<ADIF_VER:5>3.1.4\n";
    out << "<PROGRAMID:" << program.size() << ">" << program << "\n";
    QString now = QDateTime::currentDateTimeUtc().toString("yyyyMMdd");
    out << "<CREATED_TIMESTAMP:15>" << now << " 000000\n";
    out << "<EOH>\n\n";

    auto add = [&](const QString& tag, const QString& val) {
        if (!val.trimmed().isEmpty()) {
            QByteArray utf8 = val.toUtf8();
            out << "<" << tag << ":" << utf8.size() << ">" << val;
        }
    };

    for (const Qso& q : qsos) {
        add("CALL",              q.callsign);
        add("QSO_DATE",          dbDate(q.date));
        add("TIME_ON",           q.timeOn);
        add("TIME_OFF",          q.timeOff);
        add("BAND",              q.band);
        if (q.freq > 0) add("FREQ", QString::number(q.freq, 'f', 6));
        add("MODE",              q.mode);
        add("SUBMODE",           q.submode);
        add("RST_SENT",          q.rstSent);
        add("RST_RCVD",          q.rstRcvd);
        add("NAME",              q.name);
        add("QTH",               q.qth);
        add("COUNTRY",           q.country);
        if (q.cqZone)  add("CQZ",  QString::number(q.cqZone));
        if (q.ituZone) add("ITUZ", QString::number(q.ituZone));
        add("CONT",              q.continent);
        add("STATE",             q.state);
        add("GRIDSQUARE",        q.gridsquare);
        if (q.power > 0) add("TX_PWR", QString::number(q.power, 'f', 0));
        add("COMMENT",           q.comment);
        add("NOTES",             q.notes);
        add("IOTA",              q.iota);
        add("SOTA_REF",          q.sotaRef);
        add("POTA_REF",          q.potaRef);
        add("CONTEST_ID",        q.contestId);
        add("QSLVIA",            q.manager);
        add("OPERATOR",          q.operatorCall);
        add("STATION_CALLSIGN",  q.stationCall.isEmpty() ? myCall : q.stationCall);
        add("QSL_SENT",          q.qslSent);
        add("QSL_RCVD",          q.qslRcvd);
        add("LOTW_QSL_SENT",     q.lotwSent);
        add("LOTW_QSL_RCVD",     q.lotwRcvd);
        add("EQSL_QSL_SENT",     q.eqslSent);
        add("EQSL_QSL_RCVD",     q.eqslRcvd);
        out << "<EOR>\n";
    }
    return qsos.size();
}

} // namespace AdifHandler
