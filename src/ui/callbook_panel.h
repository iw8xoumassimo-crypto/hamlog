#pragma once
#include <QWidget>
#include <QHash>
#include "types.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QFormLayout;
class HamQthService;
class HamDbService;

class CallbookPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CallbookPanel(QWidget* parent = nullptr);

public slots:
    void lookup(const QString& callsign);
    void showFromQso(const Qso& qso);

signals:
    // Emitted when callbook data is ready — connect to QsoEntry::fillFromCallbook
    void callbookData(const QString& callsign,
                      const QString& name,
                      const QString& qth,
                      const QString& grid,
                      const QString& country);

private slots:
    void onResult(const QString& callsign, const QHash<QString,QString>& data, bool fromCache);
    void onManualLookup();

private:
    void build();
    void showData(const QString& callsign, const QHash<QString,QString>& data);
    void clearData();

    QLineEdit*  m_input   = nullptr;
    QLabel*     m_call    = nullptr;
    QFormLayout*m_fl      = nullptr;

    // Data labels
    QLabel* m_name    = nullptr;
    QLabel* m_qth     = nullptr;
    QLabel* m_country = nullptr;
    QLabel* m_grid    = nullptr;
    QLabel* m_email   = nullptr;
    QLabel* m_web     = nullptr;

    HamQthService* m_hamqth = nullptr;
    HamDbService*  m_hamdb  = nullptr;
};
