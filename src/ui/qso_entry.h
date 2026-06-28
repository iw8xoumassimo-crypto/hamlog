#pragma once
#include <QWidget>
#include "types.h"

class QLineEdit;
class QComboBox;
class QDateEdit;
class QTimeEdit;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QTabWidget;
class QPlainTextEdit;
class QCheckBox;

class QsoEntry : public QWidget
{
    Q_OBJECT

public:
    explicit QsoEntry(QWidget* parent = nullptr);

    void reset();
    void loadQso(const Qso& qso);
    void prefill(const QString& callsign, double freqMhz, const QString& mode);
    void fillFromSpot(const QString& callsign, double freqMhz, const QString& mode);
    void setRigState(const RigState& state);

public slots:
    void onCatConnected();
    void onSave();
    void onClear();
    // Called when callbook returns data for the current callsign
    void fillFromCallbook(const QString& callsign,
                          const QString& name,
                          const QString& qth,
                          const QString& grid,
                          const QString& country);

signals:
    void callsignChanged(const QString& callsign);
    // Emitted when a well-formed callsign is entered: triggers auto callbook lookup
    void callsignForLookup(const QString& callsign);
    void qsoSaved(const Qso& qso);

private slots:
    void onCallsignChanged(const QString& text);
    void onLookup();
    void onNow();
    void onFreqChanged(double mhz);
    void onModeChanged(const QString& mode);

private:
    void build();
    Qso  collectQso() const;
    void fillFrom(const Qso& q);
    void fillFromLog(const QString& callsign);
    void populateBandFromFreq(double mhz);
    void applyDxccInfo(const QString& callsign);
    void applyQslDefaults(const QString& callsign);
    void updateDupeInfo();

    // Basic tab
    QLineEdit*      m_call     = nullptr;
    QDateEdit*      m_date     = nullptr;
    QTimeEdit*      m_timeOn   = nullptr;
    QTimeEdit*      m_timeOff  = nullptr;
    QComboBox*      m_band     = nullptr;
    QDoubleSpinBox* m_freq     = nullptr;
    QComboBox*      m_mode     = nullptr;
    QComboBox*      m_submode  = nullptr;
    QLineEdit*      m_rstSent  = nullptr;
    QLineEdit*      m_rstRcvd  = nullptr;
    QLineEdit*      m_name     = nullptr;
    QLineEdit*      m_qth      = nullptr;
    QLineEdit*      m_grid     = nullptr;
    QLineEdit*      m_power    = nullptr;

    // DXCC (auto-filled)
    QLabel* m_lblCountry   = nullptr;
    QLabel* m_lblContinent = nullptr;
    QLabel* m_lblCqZone    = nullptr;

    // Extra tab
    QLineEdit*    m_iota     = nullptr;
    QLineEdit*    m_sota     = nullptr;
    QLineEdit*    m_pota     = nullptr;
    QLineEdit*    m_contest  = nullptr;
    QLineEdit*    m_srx      = nullptr;
    QLineEdit*    m_stx      = nullptr;
    QLineEdit*    m_manager  = nullptr;
    QPlainTextEdit* m_comment = nullptr;
    QPlainTextEdit* m_notes   = nullptr;

    // QSL panel
    QCheckBox* m_qslSent  = nullptr;   // "Invia QSL card"
    QComboBox* m_qslRcvd  = nullptr;
    QCheckBox* m_lotwSent = nullptr;   // "Invia LoTW"
    QComboBox* m_lotwRcvd = nullptr;
    QCheckBox* m_eqslSent = nullptr;   // "Invia eQSL"
    QComboBox* m_eqslRcvd = nullptr;

    QPushButton* m_btnSave  = nullptr;
    QPushButton* m_btnClear = nullptr;

    QLabel* m_dupeInfo = nullptr;

    int     m_editId = 0;   // 0 = new QSO
    bool    m_catLocked = false;

    // Valori originali QSL sent — preservano stati intermedi (Q/R/I) al salvataggio
    QString m_origQslSent;
    QString m_origLotwSent;
    QString m_origEqslSent;
};
