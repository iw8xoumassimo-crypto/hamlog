#pragma once
#include <QDialog>
#include "types.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QLabel;
class QTableWidget;

class ContestDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ContestDialog(QWidget* parent = nullptr);

private slots:
    void onLog();
    void onCallsignChanged(const QString& cs);

private:
    void updateScore();
    bool isDupe(const QString& callsign) const;

    QLineEdit*    m_contestName = nullptr;
    QLineEdit*    m_exSent      = nullptr;   // exchange inviato
    QComboBox*    m_band        = nullptr;
    QComboBox*    m_mode        = nullptr;
    QLineEdit*    m_callsign    = nullptr;
    QLineEdit*    m_exRcvd      = nullptr;   // exchange ricevuto
    QLineEdit*    m_rst         = nullptr;
    QSpinBox*     m_serial      = nullptr;
    QLabel*       m_dupeLabel   = nullptr;
    QLabel*       m_scoreLabel  = nullptr;
    QTableWidget* m_log         = nullptr;

    QList<Qso>    m_qsos;       // QSO loggati in questa sessione
    int           m_score = 0;
};
