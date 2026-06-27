#pragma once
#include <QString>
#include <QVariant>
#include <QHash>
#include <QList>
#include <QPair>
#include <optional>

// ── QSO record (maps 1:1 to the DB qso table) ────────────────────────────────
struct Qso {
    int     id          = 0;
    int     logId       = 1;
    QString callsign;
    QString date;        // YYYY-MM-DD
    QString timeOn;      // HHMM
    QString timeOff;
    QString band;
    double  freq        = 0.0;
    QString mode;
    QString submode;
    QString rstSent     = "59";
    QString rstRcvd     = "59";
    QString name;
    QString qth;
    QString country;
    int     dxccId      = 0;
    int     cqZone      = 0;
    int     ituZone     = 0;
    QString continent;
    double  lat         = 0.0;
    double  lon         = 0.0;
    QString state;
    QString gridsquare;
    int     distance    = 0;
    int     bearing     = 0;
    double  power       = 0.0;
    QString comment;
    QString notes;
    QString iota;
    QString sotaRef;
    QString potaRef;
    QString contestId;
    QString srx;
    QString stx;
    QString manager;
    QString operatorCall;
    QString stationCall;
    QString qslSent     = "N";
    QString qslRcvd     = "N";
    QString lotwSent    = "N";
    QString lotwRcvd    = "N";
    QString eqslSent    = "N";
    QString eqslRcvd    = "N";
    bool    clublogSent = false;
    QString createdAt;

    bool isValid() const {
        return !callsign.isEmpty() && !date.isEmpty() &&
               !band.isEmpty()    && !mode.isEmpty();
    }

    static QString freqToBand(double mhz) {
        if (mhz >= 1.8  && mhz <= 2.0)   return "160m";
        if (mhz >= 3.5  && mhz <= 4.0)   return "80m";
        if (mhz >= 5.0  && mhz <= 5.5)   return "60m";
        if (mhz >= 7.0  && mhz <= 7.3)   return "40m";
        if (mhz >= 10.1 && mhz <= 10.15) return "30m";
        if (mhz >= 14.0 && mhz <= 14.35) return "20m";
        if (mhz >= 18.0 && mhz <= 18.2)  return "17m";
        if (mhz >= 21.0 && mhz <= 21.45) return "15m";
        if (mhz >= 24.8 && mhz <= 25.0)  return "12m";
        if (mhz >= 28.0 && mhz <= 29.7)  return "10m";
        if (mhz >= 50.0 && mhz <= 54.0)  return "6m";
        if (mhz >= 70.0 && mhz <= 71.0)  return "4m";
        if (mhz >= 144  && mhz <= 148)   return "2m";
        if (mhz >= 420  && mhz <= 450)   return "70cm";
        if (mhz >= 1240 && mhz <= 1300)  return "23cm";
        return QString();
    }

    QHash<QString,QVariant> toMap() const;
    static Qso fromMap(const QHash<QString,QVariant>& m);
};

// ── DXCC entity info ──────────────────────────────────────────────────────────
struct DxccInfo {
    QString entity;
    int     cqZone  = 0;
    int     ituZone = 0;
    QString continent;
    double  lat     = 0.0;
    double  lon     = 0.0;
};

// ── Search / display filter ───────────────────────────────────────────────────
struct QsoFilter {
    QString callsign;
    QString band;
    QString mode;
    QString country;
    QString continent;
    QString dateFrom;
    QString dateTo;
    bool    lotwConfirmed = false;
    QString orderBy;     // column name (validated in Database)
    QString orderDir;    // "ASC" or "DESC"

    bool hasFilter() const {
        return !callsign.isEmpty() || !band.isEmpty()  || !mode.isEmpty()  ||
               !country.isEmpty()  || !continent.isEmpty() ||
               !dateFrom.isEmpty() || !dateTo.isEmpty()  || lotwConfirmed;
    }
};

// ── Statistics ────────────────────────────────────────────────────────────────
struct Stats {
    int totalQsos      = 0;
    int uniqueCalls    = 0;
    int countries      = 0;
    int cqZones        = 0;
    int ituZones       = 0;
    int lotwConfirmed  = 0;
    int eqslConfirmed  = 0;
    QHash<QString,int> byBand;
    QHash<QString,int> byMode;
    QHash<QString,int> byContinent;
};

// ── Log / Logbook info ────────────────────────────────────────────────────────
struct LogInfo {
    int     id       = 0;
    QString name;
    QString callsign;
    QString qth;
    QString locator;
    QString created;
};

// ── Rig state (from CAT) ──────────────────────────────────────────────────────
struct RigState {
    double  freq    = 0.0;
    QString mode;
    QString submode;
    bool    ptt     = false;
    bool    valid   = false;
};
