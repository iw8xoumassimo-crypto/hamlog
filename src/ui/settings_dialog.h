#pragma once
#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QTabWidget;
class QLabel;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onAccept();
    void onBrowseDb();
    void onBrowseTqsl();

private:
    void build();
    void load();
    void save();

    // Station tab
    QLineEdit* m_callsign   = nullptr;
    QLineEdit* m_operator   = nullptr;
    QLineEdit* m_myName     = nullptr;
    QLineEdit* m_qth        = nullptr;
    QLineEdit* m_locator    = nullptr;
    QLineEdit* m_myCountry  = nullptr;
    QSpinBox*  m_myCq       = nullptr;
    QSpinBox*  m_myItu      = nullptr;
    QLineEdit* m_myPower    = nullptr;
    QLineEdit* m_myAntenna  = nullptr;

    // Log tab
    QCheckBox* m_autoUtc    = nullptr;
    QCheckBox* m_autosaveFt8= nullptr;
    QCheckBox* m_dupeBand   = nullptr;
    QCheckBox* m_dupeMode   = nullptr;

    // Database tab
    QLineEdit* m_dbPath     = nullptr;

    // CAT tab
    QCheckBox* m_catEnabled = nullptr;
    QComboBox* m_catBackend = nullptr;
    QLineEdit* m_catHost    = nullptr;
    QSpinBox*  m_catPort    = nullptr;

    // LoTW tab
    QLineEdit* m_lotwCall   = nullptr;
    QLineEdit* m_lotwPass   = nullptr;
    QLineEdit* m_tqslPath   = nullptr;

    // eQSL tab
    QLineEdit* m_eqslUser   = nullptr;
    QLineEdit* m_eqslPass   = nullptr;

    // ClubLog tab
    QLineEdit* m_clubEmail  = nullptr;
    QLineEdit* m_clubPass   = nullptr;
    QLineEdit* m_clubCall   = nullptr;

    // HamQTH tab
    QLineEdit* m_hamqthUser    = nullptr;
    QLineEdit* m_hamqthPass    = nullptr;
    QLabel*    m_hamqthStatus  = nullptr;

    // UI tab
    QComboBox* m_theme      = nullptr;

    // WSJT-X/UDP tab
    QCheckBox* m_wsjtxEn    = nullptr;
    QSpinBox*  m_wsjtxPort  = nullptr;

    // Decodium tab
    QLineEdit* m_decodiumPath = nullptr;
};
