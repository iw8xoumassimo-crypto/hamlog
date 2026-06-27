#pragma once
#include <QDialog>
class QTableWidget;
class QComboBox;

class DxHunterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DxHunterDialog(QWidget* parent = nullptr);
private slots:
    void onFilter();
private:
    void build();
    void populate();
    QTableWidget* m_table = nullptr;
    QComboBox*    m_band  = nullptr;
    QComboBox*    m_mode  = nullptr;
};
