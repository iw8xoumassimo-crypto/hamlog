#include "cabrillo.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

namespace Cabrillo {

int exportCabrillo(const QString& filename,
                   const QList<Qso>& qsos,
                   const QHash<QString,QString>& header)
{
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return -1;

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "START-OF-LOG: 3.0\n";
    out << "CREATED-BY: HamLog\n";
    out << "CONTEST: "        << header.value("contest", "")          << "\n";
    out << "CALLSIGN: "       << header.value("callsign", "")         << "\n";
    out << "CATEGORY-OPERATOR: " << header.value("category-operator","SINGLE-OP") << "\n";
    out << "CATEGORY-BAND: "  << header.value("category-band", "ALL") << "\n";
    out << "CATEGORY-MODE: "  << header.value("category-mode", "SSB") << "\n";
    out << "CATEGORY-POWER: " << header.value("category-power","LOW") << "\n";
    out << "CATEGORY-STATION: " << header.value("category-station","FIXED") << "\n";
    out << "CLAIMED-SCORE: "  << header.value("claimed-score", "0")   << "\n";
    out << "CLUB: "           << header.value("club", "")             << "\n";
    out << "NAME: "           << header.value("name", "")             << "\n";
    out << "ADDRESS: "        << header.value("address", "")          << "\n";
    out << "SOAPBOX: \n";

    const QString myCall = header.value("callsign");

    for (const Qso& q : qsos) {
        // DATE: YYYY-MM-DD -> YYYY-MM-DD (keep)
        QString date = q.date;
        if (date.size() == 8)  // YYYYMMDD
            date = date.left(4) + "-" + date.mid(4,2) + "-" + date.mid(6,2);

        QString time = q.timeOn;
        if (time.size() == 4)
            time = time.left(2) + time.mid(2,2);
        else if (time.size() >= 6)
            time = time.left(2) + time.mid(2,2);

        // Frequency in kHz
        QString freq;
        if (q.freq > 0) {
            freq = QString::number(static_cast<long long>(q.freq * 1000));
        } else {
            // Band -> freq mapping (approximate)
            static QHash<QString,QString> bandFreq = {
                {"160m","1800"},{"80m","3500"},{"60m","5330"},
                {"40m","7000"},{"30m","10100"},{"20m","14000"},
                {"17m","18068"},{"15m","21000"},{"12m","24890"},
                {"10m","28000"},{"6m","50000"},{"4m","70000"},
                {"2m","144000"},{"70cm","432000"},{"23cm","1296000"}
            };
            freq = bandFreq.value(q.band.toLower(), "14000");
        }

        QString mode = q.mode.toUpper();
        if (mode == "USB" || mode == "LSB") mode = "PH";
        else if (mode == "FT8" || mode == "FT4" || mode == "JS8" || mode == "RTTY")
            mode = "DG";
        else if (mode == "CW") mode = "CW";
        else mode = "PH";

        QString rstSent = q.rstSent.isEmpty() ? "59" : q.rstSent;
        QString exchSent = q.stx.isEmpty() ? header.value("exchange", "001") : q.stx;
        QString rstRcvd = q.rstRcvd.isEmpty() ? "59" : q.rstRcvd;
        QString exchRcvd = q.srx.isEmpty() ? "001" : q.srx;

        out << QString("QSO: %1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n")
               .arg(freq.leftJustified(6))
               .arg(mode.leftJustified(2))
               .arg(date)
               .arg(time.leftJustified(4))
               .arg(myCall.leftJustified(13))
               .arg(rstSent.leftJustified(4))
               .arg(exchSent.leftJustified(6))
               .arg(q.callsign.leftJustified(13))
               .arg(rstRcvd.leftJustified(4))
               .arg(exchRcvd.leftJustified(6));
    }

    out << "END-OF-LOG:\n";
    return qsos.size();
}

} // namespace Cabrillo
