#pragma once
#include <QDialog>
class QComboBox;
class QTableWidget;
class QLabel;
class QTabWidget;
class QCheckBox;

class AwardsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AwardsDialog(QWidget* parent = nullptr);
private slots:
    void onFilterChanged();
private:
    void build();
    void populateDxcc();
    void populateCqZones();
    void populateItuZones();

    QTabWidget*   m_tabs       = nullptr;
    QComboBox*    m_band       = nullptr;
    QComboBox*    m_mode       = nullptr;
    QCheckBox*    m_showAll    = nullptr;

    // DXCC tab
    QTableWidget* m_dxccTable  = nullptr;
    QLabel*       m_lblDxcc    = nullptr;

    // CQ Zone tab
    QTableWidget* m_cqTable    = nullptr;
    QLabel*       m_lblCq      = nullptr;

    // ITU Zone tab
    QTableWidget* m_ituTable   = nullptr;
    QLabel*       m_lblItu     = nullptr;
};
